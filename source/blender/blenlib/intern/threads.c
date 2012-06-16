/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/threads.c
 *  \ingroup bli
 */


#include <errno.h>
#include <string.h>

#include "MEM_guardedalloc.h"


#include "BLI_blenlib.h"
#include "BLI_gsqueue.h"
#include "BLI_threads.h"

#include "PIL_time.h"

/* for checking system threads - BLI_system_thread_count */
#ifdef WIN32
#  include <windows.h>
#  include <sys/timeb.h>
#elif defined(__APPLE__)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#else
#  include <unistd.h>
#  include <sys/time.h>
#endif

#if defined(__APPLE__) && (PARALLEL == 1) && (__GNUC__ == 4) && (__GNUC_MINOR__ == 2)
/* ************** libgomp (Apple gcc 4.2.1) TLS bug workaround *************** */
extern pthread_key_t gomp_tls_key;
static void *thread_tls_data;
#endif

/* ********** basic thread control API ************ 
 * 
 * Many thread cases have an X amount of jobs, and only an Y amount of
 * threads are useful (typically amount of cpus)
 *
 * This code can be used to start a maximum amount of 'thread slots', which
 * then can be filled in a loop with an idle timer. 
 *
 * A sample loop can look like this (pseudo c);
 *
 *     ListBase lb;
 *     int maxthreads = 2;
 *     int cont = 1;
 * 
 *     BLI_init_threads(&lb, do_something_func, maxthreads);
 * 
 *     while (cont) {
 *         if (BLI_available_threads(&lb) && !(escape loop event)) {
 *             // get new job (data pointer)
 *             // tag job 'processed 
 *             BLI_insert_thread(&lb, job);
 *         }
 *         else PIL_sleep_ms(50);
 *         
 *         // find if a job is ready, this the do_something_func() should write in job somewhere
 *         cont = 0;
 *         for (go over all jobs)
 *             if (job is ready) {
 *                 if (job was not removed) {
 *                     BLI_remove_thread(&lb, job);
 *                 }
 *             }
 *             else cont = 1;
 *         }
 *         // conditions to exit loop 
 *         if (if escape loop event) {
 *             if (BLI_available_threadslots(&lb)==maxthreads)
 *                 break;
 *         }
 *     }
 * 
 *     BLI_end_threads(&lb);
 *
 ************************************************ */
static pthread_mutex_t _malloc_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _image_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _preview_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _viewer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _custom1_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _rcache_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _opengl_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _nodes_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _movieclip_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t mainid;
static int thread_levels = 0;  /* threads can be invoked inside threads */

/* just a max for security reasons */
#define RE_MAX_THREAD BLENDER_MAX_THREADS

typedef struct ThreadSlot {
	struct ThreadSlot *next, *prev;
	void *(*do_thread)(void *);
	void *callerdata;
	pthread_t pthread;
	int avail;
} ThreadSlot;

static void BLI_lock_malloc_thread(void)
{
	pthread_mutex_lock(&_malloc_lock);
}

static void BLI_unlock_malloc_thread(void)
{
	pthread_mutex_unlock(&_malloc_lock);
}

void BLI_threadapi_init(void)
{
	mainid = pthread_self();
}

/* tot = 0 only initializes malloc mutex in a safe way (see sequence.c)
 * problem otherwise: scene render will kill of the mutex!
 */

void BLI_init_threads(ListBase *threadbase, void *(*do_thread)(void *), int tot)
{
	int a;

	if (threadbase != NULL && tot > 0) {
		threadbase->first = threadbase->last = NULL;
	
		if (tot > RE_MAX_THREAD) tot = RE_MAX_THREAD;
		else if (tot < 1) tot = 1;
	
		for (a = 0; a < tot; a++) {
			ThreadSlot *tslot = MEM_callocN(sizeof(ThreadSlot), "threadslot");
			BLI_addtail(threadbase, tslot);
			tslot->do_thread = do_thread;
			tslot->avail = 1;
		}
	}
	
	if (thread_levels == 0) {
		MEM_set_lock_callback(BLI_lock_malloc_thread, BLI_unlock_malloc_thread);

#if defined(__APPLE__) && (PARALLEL == 1) && (__GNUC__ == 4) && (__GNUC_MINOR__ == 2)
		/* workaround for Apple gcc 4.2.1 omp vs background thread bug,
		 * we copy gomp thread local storage pointer to setting it again
		 * inside the thread that we start */
		thread_tls_data = pthread_getspecific(gomp_tls_key);
#endif
	}

	thread_levels++;
}

/* amount of available threads */
int BLI_available_threads(ListBase *threadbase)
{
	ThreadSlot *tslot;
	int counter = 0;
	
	for (tslot = threadbase->first; tslot; tslot = tslot->next) {
		if (tslot->avail)
			counter++;
	}
	return counter;
}

/* returns thread number, for sample patterns or threadsafe tables */
int BLI_available_thread_index(ListBase *threadbase)
{
	ThreadSlot *tslot;
	int counter = 0;
	
	for (tslot = threadbase->first; tslot; tslot = tslot->next, counter++) {
		if (tslot->avail)
			return counter;
	}
	return 0;
}

static void *tslot_thread_start(void *tslot_p)
{
	ThreadSlot *tslot = (ThreadSlot *)tslot_p;

#if defined(__APPLE__) && (PARALLEL == 1) && (__GNUC__ == 4) && (__GNUC_MINOR__ == 2)
	/* workaround for Apple gcc 4.2.1 omp vs background thread bug,
	 * set gomp thread local storage pointer which was copied beforehand */
	pthread_setspecific(gomp_tls_key, thread_tls_data);
#endif

	return tslot->do_thread(tslot->callerdata);
}

int BLI_thread_is_main(void)
{
	return pthread_equal(pthread_self(), mainid);
}

void BLI_insert_thread(ListBase *threadbase, void *callerdata)
{
	ThreadSlot *tslot;
	
	for (tslot = threadbase->first; tslot; tslot = tslot->next) {
		if (tslot->avail) {
			tslot->avail = 0;
			tslot->callerdata = callerdata;
			pthread_create(&tslot->pthread, NULL, tslot_thread_start, tslot);
			return;
		}
	}
	printf("ERROR: could not insert thread slot\n");
}

void BLI_remove_thread(ListBase *threadbase, void *callerdata)
{
	ThreadSlot *tslot;
	
	for (tslot = threadbase->first; tslot; tslot = tslot->next) {
		if (tslot->callerdata == callerdata) {
			pthread_join(tslot->pthread, NULL);
			tslot->callerdata = NULL;
			tslot->avail = 1;
		}
	}
}

void BLI_remove_thread_index(ListBase *threadbase, int index)
{
	ThreadSlot *tslot;
	int counter = 0;
	
	for (tslot = threadbase->first; tslot; tslot = tslot->next, counter++) {
		if (counter == index && tslot->avail == 0) {
			pthread_join(tslot->pthread, NULL);
			tslot->callerdata = NULL;
			tslot->avail = 1;
			break;
		}
	}
}

void BLI_remove_threads(ListBase *threadbase)
{
	ThreadSlot *tslot;
	
	for (tslot = threadbase->first; tslot; tslot = tslot->next) {
		if (tslot->avail == 0) {
			pthread_join(tslot->pthread, NULL);
			tslot->callerdata = NULL;
			tslot->avail = 1;
		}
	}
}

void BLI_end_threads(ListBase *threadbase)
{
	ThreadSlot *tslot;
	
	/* only needed if there's actually some stuff to end
	 * this way we don't end up decrementing thread_levels on an empty threadbase 
	 * */
	if (threadbase && threadbase->first != NULL) {
		for (tslot = threadbase->first; tslot; tslot = tslot->next) {
			if (tslot->avail == 0) {
				pthread_join(tslot->pthread, NULL);
			}
		}
		BLI_freelistN(threadbase);
	}

	thread_levels--;
	if (thread_levels == 0)
		MEM_set_lock_callback(NULL, NULL);
}

/* System Information */

/* how many threads are native on this system? */
int BLI_system_thread_count(void)
{
	int t;
#ifdef WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	t = (int) info.dwNumberOfProcessors;
#else 
#   ifdef __APPLE__
	int mib[2];
	size_t len;
	
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(t);
	sysctl(mib, 2, &t, &len, NULL, 0);
#   else
	t = (int)sysconf(_SC_NPROCESSORS_ONLN);
#   endif
#endif
	
	if (t > RE_MAX_THREAD)
		return RE_MAX_THREAD;
	if (t < 1)
		return 1;
	
	return t;
}

/* Global Mutex Locks */

void BLI_lock_thread(int type)
{
	if (type == LOCK_IMAGE)
		pthread_mutex_lock(&_image_lock);
	else if (type == LOCK_PREVIEW)
		pthread_mutex_lock(&_preview_lock);
	else if (type == LOCK_VIEWER)
		pthread_mutex_lock(&_viewer_lock);
	else if (type == LOCK_CUSTOM1)
		pthread_mutex_lock(&_custom1_lock);
	else if (type == LOCK_RCACHE)
		pthread_mutex_lock(&_rcache_lock);
	else if (type == LOCK_OPENGL)
		pthread_mutex_lock(&_opengl_lock);
	else if (type == LOCK_NODES)
		pthread_mutex_lock(&_nodes_lock);
	else if (type == LOCK_MOVIECLIP)
		pthread_mutex_lock(&_movieclip_lock);
}

void BLI_unlock_thread(int type)
{
	if (type == LOCK_IMAGE)
		pthread_mutex_unlock(&_image_lock);
	else if (type == LOCK_PREVIEW)
		pthread_mutex_unlock(&_preview_lock);
	else if (type == LOCK_VIEWER)
		pthread_mutex_unlock(&_viewer_lock);
	else if (type == LOCK_CUSTOM1)
		pthread_mutex_unlock(&_custom1_lock);
	else if (type == LOCK_RCACHE)
		pthread_mutex_unlock(&_rcache_lock);
	else if (type == LOCK_OPENGL)
		pthread_mutex_unlock(&_opengl_lock);
	else if (type == LOCK_NODES)
		pthread_mutex_unlock(&_nodes_lock);
	else if (type == LOCK_MOVIECLIP)
		pthread_mutex_unlock(&_movieclip_lock);
}

/* Mutex Locks */

void BLI_mutex_init(ThreadMutex *mutex)
{
	pthread_mutex_init(mutex, NULL);
}

void BLI_mutex_lock(ThreadMutex *mutex)
{
	pthread_mutex_lock(mutex);
}

void BLI_mutex_unlock(ThreadMutex *mutex)
{
	pthread_mutex_unlock(mutex);
}

void BLI_mutex_end(ThreadMutex *mutex)
{
	pthread_mutex_destroy(mutex);
}

/* Read/Write Mutex Lock */

void BLI_rw_mutex_init(ThreadRWMutex *mutex)
{
	pthread_rwlock_init(mutex, NULL);
}

void BLI_rw_mutex_lock(ThreadRWMutex *mutex, int mode)
{
	if (mode == THREAD_LOCK_READ)
		pthread_rwlock_rdlock(mutex);
	else
		pthread_rwlock_wrlock(mutex);
}

void BLI_rw_mutex_unlock(ThreadRWMutex *mutex)
{
	pthread_rwlock_unlock(mutex);
}

void BLI_rw_mutex_end(ThreadRWMutex *mutex)
{
	pthread_rwlock_destroy(mutex);
}

/* ************************************************ */

typedef struct ThreadedWorker {
	ListBase threadbase;
	void *(*work_fnct)(void *);
	char busy[RE_MAX_THREAD];
	int total;
	int sleep_time;
} ThreadedWorker;

typedef struct WorkParam {
	ThreadedWorker *worker;
	void *param;
	int index;
} WorkParam;

static void *exec_work_fnct(void *v_param)
{
	WorkParam *p = (WorkParam *)v_param;
	void *value;
	
	value = p->worker->work_fnct(p->param);
	
	p->worker->busy[p->index] = 0;
	MEM_freeN(p);
	
	return value;
}

ThreadedWorker *BLI_create_worker(void *(*do_thread)(void *), int tot, int sleep_time)
{
	ThreadedWorker *worker;
	
	(void)sleep_time; /* unused */
	
	worker = MEM_callocN(sizeof(ThreadedWorker), "threadedworker");
	
	if (tot > RE_MAX_THREAD) {
		tot = RE_MAX_THREAD;
	}
	else if (tot < 1) {
		tot = 1;
	}
	
	worker->total = tot;
	worker->work_fnct = do_thread;
	
	BLI_init_threads(&worker->threadbase, exec_work_fnct, tot);
	
	return worker;
}

void BLI_end_worker(ThreadedWorker *worker)
{
	BLI_remove_threads(&worker->threadbase);
}

void BLI_destroy_worker(ThreadedWorker *worker)
{
	BLI_end_worker(worker);
	BLI_freelistN(&worker->threadbase);
	MEM_freeN(worker);
}

void BLI_insert_work(ThreadedWorker *worker, void *param)
{
	WorkParam *p = MEM_callocN(sizeof(WorkParam), "workparam");
	int index;
	
	if (BLI_available_threads(&worker->threadbase) == 0) {
		index = worker->total;
		while (index == worker->total) {
			PIL_sleep_ms(worker->sleep_time);
			
			for (index = 0; index < worker->total; index++) {
				if (worker->busy[index] == 0) {
					BLI_remove_thread_index(&worker->threadbase, index);
					break;
				}
			}
		}
	}
	else {
		index = BLI_available_thread_index(&worker->threadbase);
	}
	
	worker->busy[index] = 1;
	
	p->param = param;
	p->index = index;
	p->worker = worker;
	
	BLI_insert_thread(&worker->threadbase, p);
}

/* ************************************************ */

struct ThreadQueue {
	GSQueue *queue;
	pthread_mutex_t mutex;
	pthread_cond_t push_cond;
	pthread_cond_t finish_cond;
	volatile int nowait;
	volatile int cancelled;
};

ThreadQueue *BLI_thread_queue_init(void)
{
	ThreadQueue *queue;

	queue = MEM_callocN(sizeof(ThreadQueue), "ThreadQueue");
	queue->queue = BLI_gsqueue_new(sizeof(void *));

	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->push_cond, NULL);
	pthread_cond_init(&queue->finish_cond, NULL);

	return queue;
}

void BLI_thread_queue_free(ThreadQueue *queue)
{
	/* destroy everything, assumes no one is using queue anymore */
	pthread_cond_destroy(&queue->finish_cond);
	pthread_cond_destroy(&queue->push_cond);
	pthread_mutex_destroy(&queue->mutex);

	BLI_gsqueue_free(queue->queue);

	MEM_freeN(queue);
}

void BLI_thread_queue_push(ThreadQueue *queue, void *work)
{
	pthread_mutex_lock(&queue->mutex);

	BLI_gsqueue_push(queue->queue, &work);

	/* signal threads waiting to pop */
	pthread_cond_signal(&queue->push_cond);
	pthread_mutex_unlock(&queue->mutex);
}

void *BLI_thread_queue_pop(ThreadQueue *queue)
{
	void *work = NULL;

	/* wait until there is work */
	pthread_mutex_lock(&queue->mutex);
	while (BLI_gsqueue_is_empty(queue->queue) && !queue->nowait)
		pthread_cond_wait(&queue->push_cond, &queue->mutex);
	
	/* if we have something, pop it */
	if (!BLI_gsqueue_is_empty(queue->queue)) {
		BLI_gsqueue_pop(queue->queue, &work);
		
		if(BLI_gsqueue_is_empty(queue->queue))
			pthread_cond_broadcast(&queue->finish_cond);
	}

	pthread_mutex_unlock(&queue->mutex);

	return work;
}

static void wait_timeout(struct timespec *timeout, int ms)
{
	ldiv_t div_result;
	long sec, usec, x;

#ifdef WIN32
	{
		struct _timeb now;
		_ftime(&now);
		sec = now.time;
		usec = now.millitm * 1000; /* microsecond precision would be better */
	}
#else
	{
		struct timeval now;
		gettimeofday(&now, NULL);
		sec = now.tv_sec;
		usec = now.tv_usec;
	}
#endif

	/* add current time + millisecond offset */
	div_result = ldiv(ms, 1000);
	timeout->tv_sec = sec + div_result.quot;

	x = usec + (div_result.rem * 1000);

	if (x >= 1000000) {
		timeout->tv_sec++;
		x -= 1000000;
	}

	timeout->tv_nsec = x * 1000;
}

void *BLI_thread_queue_pop_timeout(ThreadQueue *queue, int ms)
{
	double t;
	void *work = NULL;
	struct timespec timeout;

	t = PIL_check_seconds_timer();
	wait_timeout(&timeout, ms);

	/* wait until there is work */
	pthread_mutex_lock(&queue->mutex);
	while (BLI_gsqueue_is_empty(queue->queue) && !queue->nowait) {
		if (pthread_cond_timedwait(&queue->push_cond, &queue->mutex, &timeout) == ETIMEDOUT)
			break;
		else if (PIL_check_seconds_timer() - t >= ms * 0.001)
			break;
	}

	/* if we have something, pop it */
	if (!BLI_gsqueue_is_empty(queue->queue)) {
		BLI_gsqueue_pop(queue->queue, &work);
		
		if(BLI_gsqueue_is_empty(queue->queue))
			pthread_cond_broadcast(&queue->finish_cond);
	}
	
	pthread_mutex_unlock(&queue->mutex);

	return work;
}

int BLI_thread_queue_size(ThreadQueue *queue)
{
	int size;

	pthread_mutex_lock(&queue->mutex);
	size = BLI_gsqueue_size(queue->queue);
	pthread_mutex_unlock(&queue->mutex);

	return size;
}

void BLI_thread_queue_nowait(ThreadQueue *queue)
{
	pthread_mutex_lock(&queue->mutex);

	queue->nowait = 1;

	/* signal threads waiting to pop */
	pthread_cond_broadcast(&queue->push_cond);
	pthread_mutex_unlock(&queue->mutex);
}

void BLI_thread_queue_wait_finish(ThreadQueue *queue)
{
	/* wait for finish condition */
	pthread_mutex_lock(&queue->mutex);

    while(!BLI_gsqueue_is_empty(queue->queue))
		pthread_cond_wait(&queue->finish_cond, &queue->mutex);

	pthread_mutex_unlock(&queue->mutex);
}

/* ************************************************ */

void BLI_begin_threaded_malloc(void)
{
	if (thread_levels == 0) {
		MEM_set_lock_callback(BLI_lock_malloc_thread, BLI_unlock_malloc_thread);
	}
	thread_levels++;
}

void BLI_end_threaded_malloc(void)
{
	thread_levels--;
	if (thread_levels == 0)
		MEM_set_lock_callback(NULL, NULL);
}

