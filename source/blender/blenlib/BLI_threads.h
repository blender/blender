/*
 *
 *
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

#include <pthread.h>

/* for tables, button in UI, etc */
#define BLENDER_MAX_THREADS		64

struct ListBase;

/* Threading API */

/*this is run once at startup*/
void BLI_threadapi_init(void);

void	BLI_init_threads	(struct ListBase *threadbase, void *(*do_thread)(void *), int tot);
int		BLI_available_threads(struct ListBase *threadbase);
int		BLI_available_thread_index(struct ListBase *threadbase);
void	BLI_insert_thread	(struct ListBase *threadbase, void *callerdata);
void	BLI_remove_thread	(struct ListBase *threadbase, void *callerdata);
void	BLI_remove_thread_index(struct ListBase *threadbase, int index);
void	BLI_remove_threads(struct ListBase *threadbase);
void	BLI_end_threads		(struct ListBase *threadbase);
int BLI_thread_is_main(void);


void BLI_begin_threaded_malloc(void);
void BLI_end_threaded_malloc(void);

/* System Information */

int		BLI_system_thread_count(void); /* gets the number of threads the system can make use of */

/* Global Mutex Locks
 * 
 * One custom lock available now. can be extended. */

#define LOCK_IMAGE		0
#define LOCK_PREVIEW	1
#define LOCK_VIEWER		2
#define LOCK_CUSTOM1	3
#define LOCK_RCACHE		4
#define LOCK_OPENGL		5
#define LOCK_NODES		6
#define LOCK_MOVIECLIP	7

void	BLI_lock_thread(int type);
void	BLI_unlock_thread(int type);

/* Mutex Lock */

typedef pthread_mutex_t ThreadMutex;
#define BLI_MUTEX_INITIALIZER	PTHREAD_MUTEX_INITIALIZER

void BLI_mutex_init(ThreadMutex *mutex);
void BLI_mutex_lock(ThreadMutex *mutex);
void BLI_mutex_unlock(ThreadMutex *mutex);
void BLI_mutex_end(ThreadMutex *mutex);

/* Read/Write Mutex Lock */

#define THREAD_LOCK_READ	1
#define THREAD_LOCK_WRITE	2

typedef pthread_rwlock_t ThreadRWMutex;

void BLI_rw_mutex_init(ThreadRWMutex *mutex);
void BLI_rw_mutex_lock(ThreadRWMutex *mutex, int mode);
void BLI_rw_mutex_unlock(ThreadRWMutex *mutex);
void BLI_rw_mutex_end(ThreadRWMutex *mutex);

/* ThreadedWorker
 *
 * A simple tool for dispatching work to a limited number of threads
 * in a transparent fashion from the caller's perspective. */

struct ThreadedWorker;

/* Create a new worker supporting tot parallel threads.
 * When new work in inserted and all threads are busy, sleep(sleep_time) before checking again
 */
struct ThreadedWorker *BLI_create_worker(void *(*do_thread)(void *), int tot, int sleep_time);

/* join all working threads */
void BLI_end_worker(struct ThreadedWorker *worker);

/* also ends all working threads */
void BLI_destroy_worker(struct ThreadedWorker *worker);

/* Spawns a new work thread if possible, sleeps until one is available otherwise
 * NOTE: inserting work is NOT thread safe, so make sure it is only done from one thread */
void BLI_insert_work(struct ThreadedWorker *worker, void *param);

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

void BLI_thread_queue_nowait(ThreadQueue *queue);

#endif

