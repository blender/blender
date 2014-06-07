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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_THREADS_H__
#define __BLI_THREADS_H__ 

/** \file BLI_threads.h
 *  \ingroup bli
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#endif

/* for tables, button in UI, etc */
#define BLENDER_MAX_THREADS     64

struct ListBase;
struct TaskScheduler;

/* Threading API */

/*this is run once at startup*/
void BLI_threadapi_init(void);
void BLI_threadapi_exit(void);

struct TaskScheduler *BLI_task_scheduler_get(void);

void    BLI_init_threads(struct ListBase *threadbase, void *(*do_thread)(void *), int tot);
int     BLI_available_threads(struct ListBase *threadbase);
int     BLI_available_thread_index(struct ListBase *threadbase);
void    BLI_insert_thread(struct ListBase *threadbase, void *callerdata);
void    BLI_remove_thread(struct ListBase *threadbase, void *callerdata);
void    BLI_remove_thread_index(struct ListBase *threadbase, int index);
void    BLI_remove_threads(struct ListBase *threadbase);
void    BLI_end_threads(struct ListBase *threadbase);
int     BLI_thread_is_main(void);


void BLI_begin_threaded_malloc(void);
void BLI_end_threaded_malloc(void);

/* System Information */

int     BLI_system_thread_count(void); /* gets the number of threads the system can make use of */
void    BLI_system_num_threads_override_set(int num);
int     BLI_system_num_threads_override_get(void);
	
/* Global Mutex Locks
 * 
 * One custom lock available now. can be extended. */

#define LOCK_IMAGE      0
#define LOCK_DRAW_IMAGE 1
#define LOCK_VIEWER     2
#define LOCK_CUSTOM1    3
#define LOCK_RCACHE     4
#define LOCK_OPENGL     5
#define LOCK_NODES      6
#define LOCK_MOVIECLIP  7
#define LOCK_COLORMANAGE 8
#define LOCK_FFTW       9

void    BLI_lock_thread(int type);
void    BLI_unlock_thread(int type);

/* Mutex Lock */

typedef pthread_mutex_t ThreadMutex;
#define BLI_MUTEX_INITIALIZER   PTHREAD_MUTEX_INITIALIZER

void BLI_mutex_init(ThreadMutex *mutex);
void BLI_mutex_end(ThreadMutex *mutex);

ThreadMutex *BLI_mutex_alloc(void);
void BLI_mutex_free(ThreadMutex *mutex);

void BLI_mutex_lock(ThreadMutex *mutex);
bool BLI_mutex_trylock(ThreadMutex *mutex);
void BLI_mutex_unlock(ThreadMutex *mutex);

/* Spin Lock */

#ifdef __APPLE__
typedef OSSpinLock SpinLock;
#else
typedef pthread_spinlock_t SpinLock;
#endif

void BLI_spin_init(SpinLock *spin);
void BLI_spin_lock(SpinLock *spin);
void BLI_spin_unlock(SpinLock *spin);
void BLI_spin_end(SpinLock *spin);

/* Read/Write Mutex Lock */

#define THREAD_LOCK_READ    1
#define THREAD_LOCK_WRITE   2

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
int BLI_thread_queue_size(ThreadQueue *queue);

void BLI_thread_queue_wait_finish(ThreadQueue *queue);
void BLI_thread_queue_nowait(ThreadQueue *queue);

#ifdef __cplusplus
}
#endif

#endif

