/*
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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

#ifndef __BLI_THREADS_H__
#define __BLI_THREADS_H__

/** \file
 * \ingroup bli
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "BLI_sys_types.h"

#ifdef __APPLE__
#  include <libkern/OSAtomic.h>
#endif

/* for tables, button in UI, etc */
#define BLENDER_MAX_THREADS 1024

struct ListBase;
struct TaskScheduler;

/* Threading API */

/*this is run once at startup*/
void BLI_threadapi_init(void);
void BLI_threadapi_exit(void);

struct TaskScheduler *BLI_task_scheduler_get(void);

void BLI_threadpool_init(struct ListBase *threadbase, void *(*do_thread)(void *), int tot);
int BLI_available_threads(struct ListBase *threadbase);
int BLI_threadpool_available_thread_index(struct ListBase *threadbase);
void BLI_threadpool_insert(struct ListBase *threadbase, void *callerdata);
void BLI_threadpool_remove(struct ListBase *threadbase, void *callerdata);
void BLI_threadpool_remove_index(struct ListBase *threadbase, int index);
void BLI_threadpool_clear(struct ListBase *threadbase);
void BLI_threadpool_end(struct ListBase *threadbase);
int BLI_thread_is_main(void);

void BLI_threaded_malloc_begin(void);
void BLI_threaded_malloc_end(void);

/* System Information */

int BLI_system_thread_count(void); /* gets the number of threads the system can make use of */
void BLI_system_num_threads_override_set(int num);
int BLI_system_num_threads_override_get(void);

/* Global Mutex Locks
 *
 * One custom lock available now. can be extended. */

#define LOCK_IMAGE 0
#define LOCK_DRAW_IMAGE 1
#define LOCK_VIEWER 2
#define LOCK_CUSTOM1 3
#define LOCK_RCACHE 4
#define LOCK_OPENGL 5
#define LOCK_NODES 6
#define LOCK_MOVIECLIP 7
#define LOCK_COLORMANAGE 8
#define LOCK_FFTW 9
#define LOCK_VIEW3D 10

void BLI_thread_lock(int type);
void BLI_thread_unlock(int type);

/* Mutex Lock */

typedef pthread_mutex_t ThreadMutex;
#define BLI_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

void BLI_mutex_init(ThreadMutex *mutex);
void BLI_mutex_end(ThreadMutex *mutex);

ThreadMutex *BLI_mutex_alloc(void);
void BLI_mutex_free(ThreadMutex *mutex);

void BLI_mutex_lock(ThreadMutex *mutex);
bool BLI_mutex_trylock(ThreadMutex *mutex);
void BLI_mutex_unlock(ThreadMutex *mutex);

/* Spin Lock */

#if defined(__APPLE__)
typedef OSSpinLock SpinLock;
#elif defined(_MSC_VER)
typedef volatile int SpinLock;
#else
typedef pthread_spinlock_t SpinLock;
#endif

void BLI_spin_init(SpinLock *spin);
void BLI_spin_lock(SpinLock *spin);
void BLI_spin_unlock(SpinLock *spin);
void BLI_spin_end(SpinLock *spin);

/* Read/Write Mutex Lock */

#define THREAD_LOCK_READ 1
#define THREAD_LOCK_WRITE 2

#define BLI_RWLOCK_INITIALIZER PTHREAD_RWLOCK_INITIALIZER

typedef pthread_rwlock_t ThreadRWMutex;

void BLI_rw_mutex_init(ThreadRWMutex *mutex);
void BLI_rw_mutex_end(ThreadRWMutex *mutex);

ThreadRWMutex *BLI_rw_mutex_alloc(void);
void BLI_rw_mutex_free(ThreadRWMutex *mutex);

void BLI_rw_mutex_lock(ThreadRWMutex *mutex, int mode);
void BLI_rw_mutex_unlock(ThreadRWMutex *mutex);

/* Ticket Mutex Lock
 *
 * This is a 'fair' mutex in that it will grant the lock to the first thread
 * that requests it. */

typedef struct TicketMutex TicketMutex;

TicketMutex *BLI_ticket_mutex_alloc(void);
void BLI_ticket_mutex_free(TicketMutex *ticket);
void BLI_ticket_mutex_lock(TicketMutex *ticket);
void BLI_ticket_mutex_unlock(TicketMutex *ticket);

/* Condition */

typedef pthread_cond_t ThreadCondition;

void BLI_condition_init(ThreadCondition *cond);
void BLI_condition_wait(ThreadCondition *cond, ThreadMutex *mutex);
void BLI_condition_wait_global_mutex(ThreadCondition *cond, const int type);
void BLI_condition_notify_one(ThreadCondition *cond);
void BLI_condition_notify_all(ThreadCondition *cond);
void BLI_condition_end(ThreadCondition *cond);

/* ThreadWorkQueue
 *
 * Thread-safe work queue to push work/pointers between threads. */

typedef struct ThreadQueue ThreadQueue;

ThreadQueue *BLI_thread_queue_init(void);
void BLI_thread_queue_free(ThreadQueue *queue);

void BLI_thread_queue_push(ThreadQueue *queue, void *work);
void *BLI_thread_queue_pop(ThreadQueue *queue);
void *BLI_thread_queue_pop_timeout(ThreadQueue *queue, int ms);
int BLI_thread_queue_len(ThreadQueue *queue);
bool BLI_thread_queue_is_empty(ThreadQueue *queue);

void BLI_thread_queue_wait_finish(ThreadQueue *queue);
void BLI_thread_queue_nowait(ThreadQueue *queue);

/* Thread local storage */

#if defined(__APPLE__)
#  define ThreadLocal(type) pthread_key_t
#  define BLI_thread_local_create(name) pthread_key_create(&name, NULL)
#  define BLI_thread_local_delete(name) pthread_key_delete(name)
#  define BLI_thread_local_get(name) pthread_getspecific(name)
#  define BLI_thread_local_set(name, value) pthread_setspecific(name, value)
#else /* defined(__APPLE__) */
#  ifdef _MSC_VER
#    define ThreadLocal(type) __declspec(thread) type
#  else
#    define ThreadLocal(type) __thread type
#  endif
#  define BLI_thread_local_create(name)
#  define BLI_thread_local_delete(name)
#  define BLI_thread_local_get(name) name
#  define BLI_thread_local_set(name, value) name = value
#endif /* defined(__APPLE__) */

/* **** Special functions to help performance on crazy NUMA setups. **** */

/* Make sure process/thread is using NUMA node with fast memory access. */
void BLI_thread_put_process_on_fast_node(void);
void BLI_thread_put_thread_on_fast_node(void);

#ifdef __cplusplus
}
#endif

#endif
