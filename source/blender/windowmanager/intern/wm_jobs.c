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
 */


#include <string.h>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"

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



/* ********************** Threaded Jobs Manager ****************************** */

/*
Add new job
- register in WM
- configure callbacks

Start or re-run job
- if job running
  - signal job to end
  - add timer notifier to verify when it has ended, to start it
- else
  - start job
  - add timer notifier to handle progress

Stop job
  - signal job to end
	on end, job will tag itself as sleeping

Remove job
- signal job to end
	on end, job will remove itself

When job is done:
- it puts timer to sleep (or removes?)

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
	short suspended, running, ready, do_update, stop;
	float progress;

	/* for display in header, identification */
	char name[128];
	
	/* once running, we store this separately */
	void *run_customdata;
	void (*run_free)(void *);
	
	/* we use BLI_threads api, but per job only 1 thread runs */
	ListBase threads;

};

/* finds:
 * 1st priority: job with same owner and name
 * 2nd priority: job with same owner
 */
static wmJob *wm_job_find(wmWindowManager *wm, void *owner, const char *name)
{
	wmJob *steve, *found=NULL;
	
	for(steve= wm->jobs.first; steve; steve= steve->next)
		if(steve->owner==owner) {
			found= steve;
			if (name && strcmp(steve->name, name)==0)
				return steve;
		}
	
	return found;
}

/* ******************* public API ***************** */

/* returns current or adds new job, but doesnt run it */
/* every owner only gets a single job, adding a new one will stop running stop and 
   when stopped it starts the new one */
wmJob *WM_jobs_get(wmWindowManager *wm, wmWindow *win, void *owner, const char *name, int flag)
{
	wmJob *steve= wm_job_find(wm, owner, name);
	
	if(steve==NULL) {
		steve= MEM_callocN(sizeof(wmJob), "new job");
	
		BLI_addtail(&wm->jobs, steve);
		steve->win= win;
		steve->owner= owner;
		steve->flag= flag;
		BLI_strncpy(steve->name, name, sizeof(steve->name));
	}
	
	return steve;
}

/* returns true if job runs, for UI (progress) indicators */
int WM_jobs_test(wmWindowManager *wm, void *owner)
{
	wmJob *steve;
	
	for(steve= wm->jobs.first; steve; steve= steve->next)
		if(steve->owner==owner)
			if(steve->running)
				return 1;
	return 0;
}

float WM_jobs_progress(wmWindowManager *wm, void *owner)
{
	wmJob *steve= wm_job_find(wm, owner, NULL);
	
	if (steve && steve->flag & WM_JOB_PROGRESS)
		return steve->progress;
	
	return 0.0;
}

char *WM_jobs_name(wmWindowManager *wm, void *owner)
{
	wmJob *steve= wm_job_find(wm, owner, NULL);
	
	if (steve)
		return steve->name;
	
	return NULL;
}

int WM_jobs_is_running(wmJob *steve)
{
	return steve->running;
}

void* WM_jobs_get_customdata(wmJob * steve)
{
	if (!steve->customdata) {
		return steve->run_customdata;
	} else {
		return steve->customdata;
	}
}

void WM_jobs_customdata(wmJob *steve, void *customdata, void (*free)(void *))
{
	/* pending job? just free */
	if(steve->customdata)
		steve->free(steve->customdata);
	
	steve->customdata= customdata;
	steve->free= free;

	if(steve->running) {
		/* signal job to end */
		steve->stop= 1;
	}
}

void WM_jobs_timer(wmJob *steve, double timestep, unsigned int note, unsigned int endnote)
{
	steve->timestep = timestep;
	steve->note = note;
	steve->endnote = endnote;
}

void WM_jobs_callbacks(wmJob *steve, 
					   void (*startjob)(void *, short *, short *, float *),
					   void (*initjob)(void *),
					   void (*update)(void  *),
					   void (*endjob)(void  *))
{
	steve->startjob= startjob;
	steve->initjob= initjob;
	steve->update= update;
	steve->endjob= endjob;
}

static void *do_job_thread(void *job_v)
{
	wmJob *steve= job_v;
	
	steve->startjob(steve->run_customdata, &steve->stop, &steve->do_update, &steve->progress);
	steve->ready= 1;
	
	return NULL;
}

/* dont allow same startjob to be executed twice */
static void wm_jobs_test_suspend_stop(wmWindowManager *wm, wmJob *test)
{
	wmJob *steve;
	int suspend= 0;
	
	/* job added with suspend flag, we wait 1 timer step before activating it */
	if(test->flag & WM_JOB_SUSPEND) {
		suspend= 1;
		test->flag &= ~WM_JOB_SUSPEND;
	}
	else {
		/* check other jobs */
		for(steve= wm->jobs.first; steve; steve= steve->next) {
			/* obvious case, no test needed */
			if(steve==test || !steve->running) continue;
			
			/* if new job is not render, then check for same startjob */
			if(0==(test->flag & WM_JOB_EXCL_RENDER)) 
				if(steve->startjob!=test->startjob)
					continue;
			
			/* if new job is render, any render job should be stopped */
			if(test->flag & WM_JOB_EXCL_RENDER)
				if(0==(steve->flag & WM_JOB_EXCL_RENDER))
					continue;

			suspend= 1;

			/* if this job has higher priority, stop others */
			if(test->flag & WM_JOB_PRIORITY) {
				steve->stop= 1;
				// printf("job stopped: %s\n", steve->name);
			}
		}
	}
	
	/* possible suspend ourselfs, waiting for other jobs, or de-suspend */
	test->suspended= suspend;
	// if(suspend) printf("job suspended: %s\n", test->name);
}

/* if job running, the same owner gave it a new job */
/* if different owner starts existing startjob, it suspends itself */
void WM_jobs_start(wmWindowManager *wm, wmJob *steve)
{
	if(steve->running) {
		/* signal job to end and restart */
		steve->stop= 1;
		// printf("job started a running job, ending... %s\n", steve->name);
	}
	else {
		
		if(steve->customdata && steve->startjob) {
			
			wm_jobs_test_suspend_stop(wm, steve);
			
			if(steve->suspended==0) {
				/* copy to ensure proper free in end */
				steve->run_customdata= steve->customdata;
				steve->run_free= steve->free;
				steve->free= NULL;
				steve->customdata= NULL;
				steve->running= 1;
				
				if(steve->initjob)
					steve->initjob(steve->run_customdata);
				
				steve->stop= 0;
				steve->ready= 0;
				steve->progress= 0.0;

				// printf("job started: %s\n", steve->name);
				
				BLI_init_threads(&steve->threads, do_job_thread, 1);
				BLI_insert_thread(&steve->threads, steve);
			}
			
			/* restarted job has timer already */
			if(steve->wt==NULL)
				steve->wt= WM_event_add_timer(wm, steve->win, TIMERJOBS, steve->timestep);
		}
		else printf("job fails, not initialized\n");
	}
}

/* stop job, free data completely */
static void wm_jobs_kill_job(wmWindowManager *wm, wmJob *steve)
{
	if(steve->running) {
		/* signal job to end */
		steve->stop= 1;
		BLI_end_threads(&steve->threads);

		if(steve->endjob)
			steve->endjob(steve->run_customdata);
	}
	
	if(steve->wt)
		WM_event_remove_timer(wm, steve->win, steve->wt);
	if(steve->customdata)
		steve->free(steve->customdata);
	if(steve->run_customdata)
		steve->run_free(steve->run_customdata);
	
	/* remove steve */
	BLI_remlink(&wm->jobs, steve);
	MEM_freeN(steve);
	
}

void WM_jobs_stop_all(wmWindowManager *wm)
{
	wmJob *steve;
	
	while((steve= wm->jobs.first))
		wm_jobs_kill_job(wm, steve);
	
}

/* signal job(s) from this owner or callback to stop, timer is required to get handled */
void WM_jobs_stop(wmWindowManager *wm, void *owner, void *startjob)
{
	wmJob *steve;
	
	for(steve= wm->jobs.first; steve; steve= steve->next)
		if(steve->owner==owner || steve->startjob==startjob)
			if(steve->running)
				steve->stop= 1;
}

/* actually terminate thread and job timer */
void WM_jobs_kill(wmWindowManager *wm, void *owner, void (*startjob)(void *, short int *, short int *, float *))
{
	wmJob *steve;
	
	steve= wm->jobs.first;
	while(steve) {
		if(steve->owner==owner || steve->startjob==startjob) {
			wmJob* bill = steve;
			steve= steve->next;
			wm_jobs_kill_job(wm, bill);
		} else {
			steve= steve->next;
		}
	}
}


/* kill job entirely, also removes timer itself */
void wm_jobs_timer_ended(wmWindowManager *wm, wmTimer *wt)
{
	wmJob *steve;
	
	for(steve= wm->jobs.first; steve; steve= steve->next) {
		if(steve->wt==wt) {
			wm_jobs_kill_job(wm, steve);
			return;
		}
	}
}

/* hardcoded to event TIMERJOBS */
void wm_jobs_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt)
{
	wmJob *steve= wm->jobs.first, *stevenext;
	float total_progress= 0.f;
	float jobs_progress=0;
	
	
	for(; steve; steve= stevenext) {
		stevenext= steve->next;
		
		if(steve->wt==wt) {
			
			/* running threads */
			if(steve->threads.first) {
				
				/* always call note and update when ready */
				if(steve->do_update || steve->ready) {
					if(steve->update)
						steve->update(steve->run_customdata);
					if(steve->note)
						WM_event_add_notifier(C, steve->note, NULL);

					if (steve->flag & WM_JOB_PROGRESS)
						WM_event_add_notifier(C, NC_WM|ND_JOB, NULL);
					steve->do_update= 0;
				}	
				
				if(steve->ready) {
					if(steve->endjob)
						steve->endjob(steve->run_customdata);

					/* free own data */
					steve->run_free(steve->run_customdata);
					steve->run_customdata= NULL;
					steve->run_free= NULL;
					
					// if(steve->stop) printf("job ready but stopped %s\n", steve->name);
					// else printf("job finished %s\n", steve->name);

					steve->running= 0;
					BLI_end_threads(&steve->threads);
					
					if(steve->endnote)
						WM_event_add_notifier(C, steve->endnote, NULL);
					
					WM_event_add_notifier(C, NC_WM|ND_JOB, NULL);
					
					/* new job added for steve? */
					if(steve->customdata) {
						// printf("job restarted with new data %s\n", steve->name);
						WM_jobs_start(wm, steve);
					}
					else {
						WM_event_remove_timer(wm, steve->win, steve->wt);
						steve->wt= NULL;
						
						/* remove steve */
						BLI_remlink(&wm->jobs, steve);
						MEM_freeN(steve);
					}
				} else if (steve->flag & WM_JOB_PROGRESS) {
					/* accumulate global progress for running jobs */
					jobs_progress++;
					total_progress += steve->progress;
				}
			}
			else if(steve->suspended) {
				WM_jobs_start(wm, steve);
			}
		}
	}
	
	/* on file load 'winactive' can be NULL, possibly it should not happen but for now do a NULL check - campbell */
	if(wm->winactive) {
		/* if there are running jobs, set the global progress indicator */
		if (jobs_progress > 0) {
			float progress = total_progress / (float)jobs_progress;
			WM_progress_set(wm->winactive, progress);
		} else {
			WM_progress_clear(wm->winactive);
		}
	}
}

int WM_jobs_has_running(wmWindowManager *wm)
{
	wmJob *steve;

	for(steve= wm->jobs.first; steve; steve= steve->next)
		if(steve->running)
			return 1;

	return 0;
}
