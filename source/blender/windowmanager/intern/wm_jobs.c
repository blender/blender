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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_jobs.c
 *  \ingroup wm
 *
 * Threaded job manager (high level job access).
 */

#include <string.h>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm.h"

#include "PIL_time.h"

/*
 * Add new job
 * - register in WM
 * - configure callbacks
 *
 * Start or re-run job
 * - if job running
 *   - signal job to end
 *   - add timer notifier to verify when it has ended, to start it
 * - else
 *   - start job
 *   - add timer notifier to handle progress
 *
 * Stop job
 *   - signal job to end
 *  on end, job will tag itself as sleeping
 *
 * Remove job
 * - signal job to end
 *  on end, job will remove itself
 *
 * When job is done:
 * - it puts timer to sleep (or removes?)
 *
 */

struct wmJob {
	struct wmJob *next, *prev;
	
	/* job originating from, keep track of this when deleting windows */
	wmWindow *win;
	
	/* should store entire own context, for start, update, free */
	void *customdata;
	/* to prevent cpu overhead, use this one which only gets called when job really starts, not in thread */
	void (*initjob)(void *);
	/* this runs inside thread, and does full job */
	void (*startjob)(void *, short *stop, short *do_update, float *progress);
	/* update gets called if thread defines so, and max once per timerstep */
	/* it runs outside thread, blocking blender, no drawing! */
	void (*update)(void *);
	/* free entire customdata, doesn't run in thread */
	void (*free)(void *);
	/* gets called when job is stopped, not in thread */
	void (*endjob)(void *);
	
	/* running jobs each have own timer */
	double timestep;
	wmTimer *wt;
	/* the notifier event timers should send */
	unsigned int note, endnote;
	
	
/* internal */
	void *owner;
	int flag;
	short suspended, running, ready, do_update, stop, job_type;
	float progress;

	/* for display in header, identification */
	char name[128];
	
	/* once running, we store this separately */
	void *run_customdata;
	void (*run_free)(void *);
	
	/* we use BLI_threads api, but per job only 1 thread runs */
	ListBase threads;

	double start_time;

	/* ticket mutex for main thread locking while some job accesses
	 * data that the main thread might modify at the same time */
	TicketMutex *main_thread_mutex;
	bool main_thread_mutex_ending;
};

/* Main thread locking */

void WM_job_main_thread_lock_acquire(wmJob *wm_job)
{
	BLI_ticket_mutex_lock(wm_job->main_thread_mutex);

	/* if BLI_end_threads is being called to stop the job before it's finished,
	 * we no longer need to lock to get access to the main thread as it's
	 * waiting and can't respond */
	if (wm_job->main_thread_mutex_ending)
		BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
}

void WM_job_main_thread_lock_release(wmJob *wm_job)
{
	if (!wm_job->main_thread_mutex_ending)
		BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
}

static void wm_job_main_thread_yield(wmJob *wm_job, bool ending)
{
	if (ending)
		wm_job->main_thread_mutex_ending = true;

	/* unlock and lock the ticket mutex. because it's a fair mutex any job that
	 * is waiting to acquire the lock will get it first, before we can lock */
	BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
	BLI_ticket_mutex_lock(wm_job->main_thread_mutex);
}

/* finds:
 * if type or owner, compare for it, otherwise any matching job
 */
static wmJob *wm_job_find(wmWindowManager *wm, void *owner, const int job_type)
{
	wmJob *wm_job;
	
	if (owner && job_type) {
		for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next)
			if (wm_job->owner == owner && wm_job->job_type == job_type)
				return wm_job;
	}
	else if (owner) {
		for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next)
			if (wm_job->owner == owner)
				return wm_job;
	}
	else if (job_type) {
		for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next)
			if (wm_job->job_type == job_type)
				return wm_job;
	}
	
	return NULL;
}

/* ******************* public API ***************** */

/* returns current or adds new job, but doesnt run it */
/* every owner only gets a single job, adding a new one will stop running job and 
 * when stopped it starts the new one */
wmJob *WM_jobs_get(wmWindowManager *wm, wmWindow *win, void *owner, const char *name, int flag, int job_type)
{
	wmJob *wm_job = wm_job_find(wm, owner, job_type);
	
	if (wm_job == NULL) {
		wm_job = MEM_callocN(sizeof(wmJob), "new job");

		BLI_addtail(&wm->jobs, wm_job);
		wm_job->win = win;
		wm_job->owner = owner;
		wm_job->flag = flag;
		wm_job->job_type = job_type;
		BLI_strncpy(wm_job->name, name, sizeof(wm_job->name));

		wm_job->main_thread_mutex = BLI_ticket_mutex_alloc();
		BLI_ticket_mutex_lock(wm_job->main_thread_mutex);
	}
	/* else: a running job, be careful */
	
	/* prevent creating a job with an invalid type */
	BLI_assert(wm_job->job_type != WM_JOB_TYPE_ANY);

	return wm_job;
}

/* returns true if job runs, for UI (progress) indicators */
bool WM_jobs_test(wmWindowManager *wm, void *owner, int job_type)
{
	wmJob *wm_job;
	
	/* job can be running or about to run (suspended) */
	for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next) {
		if (wm_job->owner == owner) {
			if (job_type == WM_JOB_TYPE_ANY || (wm_job->job_type == job_type)) {
				if (wm_job->running || wm_job->suspended) {
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

float WM_jobs_progress(wmWindowManager *wm, void *owner)
{
	wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);
	
	if (wm_job && wm_job->flag & WM_JOB_PROGRESS)
		return wm_job->progress;
	
	return 0.0;
}

char *WM_jobs_name(wmWindowManager *wm, void *owner)
{
	wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);
	
	if (wm_job)
		return wm_job->name;
	
	return NULL;
}

void *WM_jobs_customdata(wmWindowManager *wm, void *owner)
{
	wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);
	
	if (wm_job)
		return WM_jobs_customdata_get(wm_job);
	
	return NULL;
}

void *WM_jobs_customdata_from_type(wmWindowManager *wm, int job_type)
{
	wmJob *wm_job = wm_job_find(wm, NULL, job_type);
	
	if (wm_job)
		return WM_jobs_customdata_get(wm_job);
	
	return NULL;
}

bool WM_jobs_is_running(wmJob *wm_job)
{
	return wm_job->running;
}

void *WM_jobs_customdata_get(wmJob *wm_job)
{
	if (!wm_job->customdata) {
		return wm_job->run_customdata;
	}
	else {
		return wm_job->customdata;
	}
}

void WM_jobs_customdata_set(wmJob *wm_job, void *customdata, void (*free)(void *))
{
	/* pending job? just free */
	if (wm_job->customdata)
		wm_job->free(wm_job->customdata);
	
	wm_job->customdata = customdata;
	wm_job->free = free;

	if (wm_job->running) {
		/* signal job to end */
		wm_job->stop = TRUE;
	}
}

void WM_jobs_timer(wmJob *wm_job, double timestep, unsigned int note, unsigned int endnote)
{
	wm_job->timestep = timestep;
	wm_job->note = note;
	wm_job->endnote = endnote;
}

void WM_jobs_callbacks(wmJob *wm_job,
                       void (*startjob)(void *, short *, short *, float *),
                       void (*initjob)(void *),
                       void (*update)(void *),
                       void (*endjob)(void *))
{
	wm_job->startjob = startjob;
	wm_job->initjob = initjob;
	wm_job->update = update;
	wm_job->endjob = endjob;
}

static void *do_job_thread(void *job_v)
{
	wmJob *wm_job = job_v;
	
	wm_job->startjob(wm_job->run_customdata, &wm_job->stop, &wm_job->do_update, &wm_job->progress);
	wm_job->ready = TRUE;
	
	return NULL;
}

/* don't allow same startjob to be executed twice */
static void wm_jobs_test_suspend_stop(wmWindowManager *wm, wmJob *test)
{
	wmJob *wm_job;
	int suspend = FALSE;
	
	/* job added with suspend flag, we wait 1 timer step before activating it */
	if (test->flag & WM_JOB_SUSPEND) {
		suspend = TRUE;
		test->flag &= ~WM_JOB_SUSPEND;
	}
	else {
		/* check other jobs */
		for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next) {
			/* obvious case, no test needed */
			if (wm_job == test || !wm_job->running) {
				continue;
			}
			
			/* if new job is not render, then check for same startjob */
			if (0 == (test->flag & WM_JOB_EXCL_RENDER))
				if (wm_job->startjob != test->startjob)
					continue;
			
			/* if new job is render, any render job should be stopped */
			if (test->flag & WM_JOB_EXCL_RENDER)
				if (0 == (wm_job->flag & WM_JOB_EXCL_RENDER))
					continue;

			suspend = TRUE;

			/* if this job has higher priority, stop others */
			if (test->flag & WM_JOB_PRIORITY) {
				wm_job->stop = TRUE;
				// printf("job stopped: %s\n", wm_job->name);
			}
		}
	}
	
	/* possible suspend ourselfs, waiting for other jobs, or de-suspend */
	test->suspended = suspend;
	// if (suspend) printf("job suspended: %s\n", test->name);
}

/* if job running, the same owner gave it a new job */
/* if different owner starts existing startjob, it suspends itself */
void WM_jobs_start(wmWindowManager *wm, wmJob *wm_job)
{
	if (wm_job->running) {
		/* signal job to end and restart */
		wm_job->stop = TRUE;
		// printf("job started a running job, ending... %s\n", wm_job->name);
	}
	else {
		
		if (wm_job->customdata && wm_job->startjob) {
			
			wm_jobs_test_suspend_stop(wm, wm_job);
			
			if (wm_job->suspended == FALSE) {
				/* copy to ensure proper free in end */
				wm_job->run_customdata = wm_job->customdata;
				wm_job->run_free = wm_job->free;
				wm_job->free = NULL;
				wm_job->customdata = NULL;
				wm_job->running = TRUE;
				
				if (wm_job->initjob)
					wm_job->initjob(wm_job->run_customdata);
				
				wm_job->stop = FALSE;
				wm_job->ready = FALSE;
				wm_job->progress = 0.0;

				// printf("job started: %s\n", wm_job->name);
				
				BLI_init_threads(&wm_job->threads, do_job_thread, 1);
				BLI_insert_thread(&wm_job->threads, wm_job);
			}
			
			/* restarted job has timer already */
			if (wm_job->wt == NULL)
				wm_job->wt = WM_event_add_timer(wm, wm_job->win, TIMERJOBS, wm_job->timestep);

			if (G.debug & G_DEBUG_JOBS)
				wm_job->start_time = PIL_check_seconds_timer();
		}
		else {
			printf("job fails, not initialized\n");
		}
	}
}

static void wm_job_free(wmWindowManager *wm, wmJob *wm_job)
{
	BLI_remlink(&wm->jobs, wm_job);
	BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
	BLI_ticket_mutex_free(wm_job->main_thread_mutex);
	MEM_freeN(wm_job);
}

/* stop job, end thread, free data completely */
static void wm_jobs_kill_job(wmWindowManager *wm, wmJob *wm_job)
{
	if (wm_job->running) {
		/* signal job to end */
		wm_job->stop = TRUE;
		wm_job_main_thread_yield(wm_job, true);
		BLI_end_threads(&wm_job->threads);

		if (wm_job->endjob)
			wm_job->endjob(wm_job->run_customdata);
	}
	
	if (wm_job->wt)
		WM_event_remove_timer(wm, wm_job->win, wm_job->wt);
	if (wm_job->customdata)
		wm_job->free(wm_job->customdata);
	if (wm_job->run_customdata)
		wm_job->run_free(wm_job->run_customdata);
	
	/* remove wm_job */
	wm_job_free(wm, wm_job);
}

/* wait until every job ended */
void WM_jobs_kill_all(wmWindowManager *wm)
{
	wmJob *wm_job;
	
	while ((wm_job = wm->jobs.first))
		wm_jobs_kill_job(wm, wm_job);
	
}

/* wait until every job ended, except for one owner (used in undo to keep screen job alive) */
void WM_jobs_kill_all_except(wmWindowManager *wm, void *owner)
{
	wmJob *wm_job, *next_job;
	
	for (wm_job = wm->jobs.first; wm_job; wm_job = next_job) {
		next_job = wm_job->next;

		if (wm_job->owner != owner)
			wm_jobs_kill_job(wm, wm_job);
	}
}


void WM_jobs_kill_type(struct wmWindowManager *wm, void *owner, int job_type)
{
	wmJob *wm_job, *next_job;
	
	for (wm_job = wm->jobs.first; wm_job; wm_job = next_job) {
		next_job = wm_job->next;

		if (!owner || wm_job->owner == owner)
			if (wm_job->job_type == job_type)
				wm_jobs_kill_job(wm, wm_job);
	}
}

/* signal job(s) from this owner or callback to stop, timer is required to get handled */
void WM_jobs_stop(wmWindowManager *wm, void *owner, void *startjob)
{
	wmJob *wm_job;
	
	for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next) {
		if (wm_job->owner == owner || wm_job->startjob == startjob) {
			if (wm_job->running) {
				wm_job->stop = TRUE;
			}
		}
	}
}

/* actually terminate thread and job timer */
void WM_jobs_kill(wmWindowManager *wm, void *owner, void (*startjob)(void *, short int *, short int *, float *))
{
	wmJob *wm_job;
	
	wm_job = wm->jobs.first;
	while (wm_job) {
		if (wm_job->owner == owner || wm_job->startjob == startjob) {
			wmJob *wm_job_kill = wm_job;
			wm_job = wm_job->next;
			wm_jobs_kill_job(wm, wm_job_kill);
		}
		else {
			wm_job = wm_job->next;
		}
	}
}


/* kill job entirely, also removes timer itself */
void wm_jobs_timer_ended(wmWindowManager *wm, wmTimer *wt)
{
	wmJob *wm_job;
	
	for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next) {
		if (wm_job->wt == wt) {
			wm_jobs_kill_job(wm, wm_job);
			return;
		}
	}
}

/* hardcoded to event TIMERJOBS */
void wm_jobs_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt)
{
	wmJob *wm_job, *wm_jobnext;
	float total_progress = 0.f;
	float jobs_progress = 0;
	
	for (wm_job = wm->jobs.first; wm_job; wm_job = wm_jobnext) {
		wm_jobnext = wm_job->next;
		
		if (wm_job->wt == wt) {
			
			/* running threads */
			if (wm_job->threads.first) {

				/* let threads get temporary lock over main thread if needed */
				wm_job_main_thread_yield(wm_job, false);
				
				/* always call note and update when ready */
				if (wm_job->do_update || wm_job->ready) {
					if (wm_job->update)
						wm_job->update(wm_job->run_customdata);
					if (wm_job->note)
						WM_event_add_notifier(C, wm_job->note, NULL);

					if (wm_job->flag & WM_JOB_PROGRESS)
						WM_event_add_notifier(C, NC_WM | ND_JOB, NULL);
					wm_job->do_update = FALSE;
				}
				
				if (wm_job->ready) {
					if (wm_job->endjob)
						wm_job->endjob(wm_job->run_customdata);

					/* free own data */
					wm_job->run_free(wm_job->run_customdata);
					wm_job->run_customdata = NULL;
					wm_job->run_free = NULL;
					
					// if (wm_job->stop) printf("job ready but stopped %s\n", wm_job->name);
					// else printf("job finished %s\n", wm_job->name);

					if (G.debug & G_DEBUG_JOBS) {
						printf("Job '%s' finished in %f seconds\n", wm_job->name,
						       PIL_check_seconds_timer() - wm_job->start_time);
					}

					wm_job->running = FALSE;
					wm_job_main_thread_yield(wm_job, true);
					BLI_end_threads(&wm_job->threads);
					wm_job->main_thread_mutex_ending = false;
					
					if (wm_job->endnote)
						WM_event_add_notifier(C, wm_job->endnote, NULL);
					
					WM_event_add_notifier(C, NC_WM | ND_JOB, NULL);

					/* new job added for wm_job? */
					if (wm_job->customdata) {
						// printf("job restarted with new data %s\n", wm_job->name);
						WM_jobs_start(wm, wm_job);
					}
					else {
						WM_event_remove_timer(wm, wm_job->win, wm_job->wt);
						wm_job->wt = NULL;
						
						/* remove wm_job */
						wm_job_free(wm, wm_job);
					}
				}
				else if (wm_job->flag & WM_JOB_PROGRESS) {
					/* accumulate global progress for running jobs */
					jobs_progress++;
					total_progress += wm_job->progress;
				}
			}
			else if (wm_job->suspended) {
				WM_jobs_start(wm, wm_job);
			}
		}
		else if (wm_job->threads.first && !wm_job->ready) {
			if (wm_job->flag & WM_JOB_PROGRESS) {
				/* accumulate global progress for running jobs */
				jobs_progress++;
				total_progress += wm_job->progress;
			}
		}
	}
	
	/* on file load 'winactive' can be NULL, possibly it should not happen but for now do a NULL check - campbell */
	if (wm->winactive) {
		/* if there are running jobs, set the global progress indicator */
		if (jobs_progress > 0) {
			float progress = total_progress / (float)jobs_progress;
			WM_progress_set(wm->winactive, progress);
		}
		else {
			WM_progress_clear(wm->winactive);
		}
	}
}

bool WM_jobs_has_running(wmWindowManager *wm)
{
	wmJob *wm_job;

	for (wm_job = wm->jobs.first; wm_job; wm_job = wm_job->next) {
		if (wm_job->running) {
			return TRUE;
		}
	}

	return FALSE;
}
