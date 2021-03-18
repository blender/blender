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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Threaded job manager (high level job access).
 */

#include <string.h>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"

#include "SEQ_prefetch.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_event_types.h"

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
 * - signal job to end
 * on end, job will tag itself as sleeping
 *
 * Remove job
 * - signal job to end
 * on end, job will remove itself
 *
 * When job is done:
 * - it puts timer to sleep (or removes?)
 */

struct wmJob {
  struct wmJob *next, *prev;

  /** Job originating from, keep track of this when deleting windows */
  wmWindow *win;

  /** Should store entire own context, for start, update, free */
  void *customdata;
  /**
   * To prevent cpu overhead, use this one which only gets called when job really starts.
   * Executed in main thread.
   */
  void (*initjob)(void *);
  /**
   * This performs the actual parallel work.
   * Executed in worker thread(s).
   */
  wm_jobs_start_callback startjob;
  /**
   * Called if thread defines so (see `do_update` flag), and max once per timer step.
   * Executed in main thread.
   */
  void (*update)(void *);
  /**
   * Free callback (typically for customdata).
   * Executed in main thread.
   */
  void (*free)(void *);
  /**
   * Called when job is stopped.
   * Executed in main thread.
   */
  void (*endjob)(void *);

  /** Running jobs each have own timer */
  double timestep;
  wmTimer *wt;
  /** Only start job after specified time delay */
  double start_delay_time;
  /** The notifier event timers should send */
  unsigned int note, endnote;

  /* internal */
  void *owner;
  int flag;
  short suspended, running, ready, do_update, stop, job_type;
  float progress;

  /** For display in header, identification */
  char name[128];

  /** Once running, we store this separately */
  void *run_customdata;
  void (*run_free)(void *);

  /** We use BLI_threads api, but per job only 1 thread runs */
  ListBase threads;

  double start_time;

  /** Ticket mutex for main thread locking while some job accesses
   * data that the main thread might modify at the same time */
  TicketMutex *main_thread_mutex;
};

/* Main thread locking */

void WM_job_main_thread_lock_acquire(wmJob *wm_job)
{
  BLI_ticket_mutex_lock(wm_job->main_thread_mutex);
}

void WM_job_main_thread_lock_release(wmJob *wm_job)
{
  BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
}

static void wm_job_main_thread_yield(wmJob *wm_job)
{
  /* unlock and lock the ticket mutex. because it's a fair mutex any job that
   * is waiting to acquire the lock will get it first, before we can lock */
  BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
  BLI_ticket_mutex_lock(wm_job->main_thread_mutex);
}

/**
 * Finds if type or owner, compare for it, otherwise any matching job.
 */
static wmJob *wm_job_find(wmWindowManager *wm, void *owner, const int job_type)
{
  if (owner && job_type) {
    LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
      if (wm_job->owner == owner && wm_job->job_type == job_type) {
        return wm_job;
      }
    }
  }
  else if (owner) {
    LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
      if (wm_job->owner == owner) {
        return wm_job;
      }
    }
  }
  else if (job_type) {
    LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
      if (wm_job->job_type == job_type) {
        return wm_job;
      }
    }
  }

  return NULL;
}

/* ******************* public API ***************** */

/**
 * \return current job or adds new job, but doesn't run it.
 *
 * \note every owner only gets a single job,
 * adding a new one will stop running job and when stopped it starts the new one.
 */
wmJob *WM_jobs_get(
    wmWindowManager *wm, wmWindow *win, void *owner, const char *name, int flag, int job_type)
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
    WM_job_main_thread_lock_acquire(wm_job);
  }
  /* else: a running job, be careful */

  /* prevent creating a job with an invalid type */
  BLI_assert(wm_job->job_type != WM_JOB_TYPE_ANY);

  return wm_job;
}

/* returns true if job runs, for UI (progress) indicators */
bool WM_jobs_test(wmWindowManager *wm, void *owner, int job_type)
{
  /* job can be running or about to run (suspended) */
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner == owner) {
      if (ELEM(job_type, WM_JOB_TYPE_ANY, wm_job->job_type)) {
        if (wm_job->running || wm_job->suspended) {
          return true;
        }
      }
    }
  }

  return false;
}

float WM_jobs_progress(wmWindowManager *wm, void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job && wm_job->flag & WM_JOB_PROGRESS) {
    return wm_job->progress;
  }

  return 0.0;
}

static void wm_jobs_update_progress_bars(wmWindowManager *wm)
{
  float total_progress = 0.0f;
  float jobs_progress = 0;

  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->threads.first && !wm_job->ready) {
      if (wm_job->flag & WM_JOB_PROGRESS) {
        /* accumulate global progress for running jobs */
        jobs_progress++;
        total_progress += wm_job->progress;
      }
    }
  }

  /* if there are running jobs, set the global progress indicator */
  if (jobs_progress > 0) {
    float progress = total_progress / (float)jobs_progress;

    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      WM_progress_set(win, progress);
    }
  }
  else {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      WM_progress_clear(win);
    }
  }
}

/* time that job started */
double WM_jobs_starttime(wmWindowManager *wm, void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job && wm_job->flag & WM_JOB_PROGRESS) {
    return wm_job->start_time;
  }

  return 0;
}

char *WM_jobs_name(wmWindowManager *wm, void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job) {
    return wm_job->name;
  }

  return NULL;
}

void *WM_jobs_customdata(wmWindowManager *wm, void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job) {
    return WM_jobs_customdata_get(wm_job);
  }

  return NULL;
}

void *WM_jobs_customdata_from_type(wmWindowManager *wm, int job_type)
{
  wmJob *wm_job = wm_job_find(wm, NULL, job_type);

  if (wm_job) {
    return WM_jobs_customdata_get(wm_job);
  }

  return NULL;
}

bool WM_jobs_is_running(wmJob *wm_job)
{
  return wm_job->running;
}

bool WM_jobs_is_stopped(wmWindowManager *wm, void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);
  return wm_job ? wm_job->stop : true; /* XXX to be redesigned properly. */
}

void *WM_jobs_customdata_get(wmJob *wm_job)
{
  if (!wm_job->customdata) {
    return wm_job->run_customdata;
  }
  return wm_job->customdata;
}

void WM_jobs_customdata_set(wmJob *wm_job, void *customdata, void (*free)(void *))
{
  /* pending job? just free */
  if (wm_job->customdata) {
    wm_job->free(wm_job->customdata);
  }

  wm_job->customdata = customdata;
  wm_job->free = free;

  if (wm_job->running) {
    /* signal job to end */
    wm_job->stop = true;
  }
}

void WM_jobs_timer(wmJob *wm_job, double timestep, unsigned int note, unsigned int endnote)
{
  wm_job->timestep = timestep;
  wm_job->note = note;
  wm_job->endnote = endnote;
}

void WM_jobs_delay_start(wmJob *wm_job, double delay_time)
{
  wm_job->start_delay_time = delay_time;
}

void WM_jobs_callbacks(wmJob *wm_job,
                       wm_jobs_start_callback startjob,
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

  BLI_thread_put_thread_on_fast_node();
  wm_job->startjob(wm_job->run_customdata, &wm_job->stop, &wm_job->do_update, &wm_job->progress);
  wm_job->ready = true;

  return NULL;
}

/* don't allow same startjob to be executed twice */
static void wm_jobs_test_suspend_stop(wmWindowManager *wm, wmJob *test)
{
  bool suspend = false;

  /* job added with suspend flag, we wait 1 timer step before activating it */
  if (test->start_delay_time > 0.0) {
    suspend = true;
    test->start_delay_time = 0.0;
  }
  else {
    /* check other jobs */
    LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
      /* obvious case, no test needed */
      if (wm_job == test || !wm_job->running) {
        continue;
      }

      /* if new job is not render, then check for same startjob */
      if (0 == (test->flag & WM_JOB_EXCL_RENDER)) {
        if (wm_job->startjob != test->startjob) {
          continue;
        }
      }

      /* if new job is render, any render job should be stopped */
      if (test->flag & WM_JOB_EXCL_RENDER) {
        if (0 == (wm_job->flag & WM_JOB_EXCL_RENDER)) {
          continue;
        }
      }

      suspend = true;

      /* if this job has higher priority, stop others */
      if (test->flag & WM_JOB_PRIORITY) {
        wm_job->stop = true;
        // printf("job stopped: %s\n", wm_job->name);
      }
    }
  }

  /* Possible suspend ourselves, waiting for other jobs, or de-suspend. */
  test->suspended = suspend;
#if 0
  if (suspend) {
    printf("job suspended: %s\n", test->name);
  }
#endif
}

/**
 * if job running, the same owner gave it a new job.
 * if different owner starts existing startjob, it suspends itself
 */
void WM_jobs_start(wmWindowManager *wm, wmJob *wm_job)
{
  if (wm_job->running) {
    /* signal job to end and restart */
    wm_job->stop = true;
    // printf("job started a running job, ending... %s\n", wm_job->name);
  }
  else {

    if (wm_job->customdata && wm_job->startjob) {
      const double timestep = (wm_job->start_delay_time > 0.0) ? wm_job->start_delay_time :
                                                                 wm_job->timestep;

      wm_jobs_test_suspend_stop(wm, wm_job);

      if (wm_job->suspended == false) {
        /* copy to ensure proper free in end */
        wm_job->run_customdata = wm_job->customdata;
        wm_job->run_free = wm_job->free;
        wm_job->free = NULL;
        wm_job->customdata = NULL;
        wm_job->running = true;

        if (wm_job->initjob) {
          wm_job->initjob(wm_job->run_customdata);
        }

        wm_job->stop = false;
        wm_job->ready = false;
        wm_job->progress = 0.0;

        // printf("job started: %s\n", wm_job->name);

        BLI_threadpool_init(&wm_job->threads, do_job_thread, 1);
        BLI_threadpool_insert(&wm_job->threads, wm_job);
      }

      /* restarted job has timer already */
      if (wm_job->wt && (wm_job->wt->timestep > timestep)) {
        WM_event_remove_timer(wm, wm_job->win, wm_job->wt);
        wm_job->wt = WM_event_add_timer(wm, wm_job->win, TIMERJOBS, timestep);
      }
      if (wm_job->wt == NULL) {
        wm_job->wt = WM_event_add_timer(wm, wm_job->win, TIMERJOBS, timestep);
      }

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
  WM_job_main_thread_lock_release(wm_job);
  BLI_ticket_mutex_free(wm_job->main_thread_mutex);
  MEM_freeN(wm_job);
}

/* stop job, end thread, free data completely */
static void wm_jobs_kill_job(wmWindowManager *wm, wmJob *wm_job)
{
  bool update_progress = (wm_job->flag & WM_JOB_PROGRESS) != 0;

  if (wm_job->running) {
    /* signal job to end */
    wm_job->stop = true;

    WM_job_main_thread_lock_release(wm_job);
    BLI_threadpool_end(&wm_job->threads);
    WM_job_main_thread_lock_acquire(wm_job);

    if (wm_job->endjob) {
      wm_job->endjob(wm_job->run_customdata);
    }
  }

  if (wm_job->wt) {
    WM_event_remove_timer(wm, wm_job->win, wm_job->wt);
  }
  if (wm_job->customdata) {
    wm_job->free(wm_job->customdata);
  }
  if (wm_job->run_customdata) {
    wm_job->run_free(wm_job->run_customdata);
  }

  /* remove wm_job */
  wm_job_free(wm, wm_job);

  /* Update progress bars in windows. */
  if (update_progress) {
    wm_jobs_update_progress_bars(wm);
  }
}

/* wait until every job ended */
void WM_jobs_kill_all(wmWindowManager *wm)
{
  wmJob *wm_job;

  while ((wm_job = wm->jobs.first)) {
    wm_jobs_kill_job(wm, wm_job);
  }

  /* This job will be automatically restarted */
  SEQ_prefetch_stop_all();
}

/* wait until every job ended, except for one owner (used in undo to keep screen job alive) */
void WM_jobs_kill_all_except(wmWindowManager *wm, void *owner)
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner != owner) {
      wm_jobs_kill_job(wm, wm_job);
    }
  }
}

void WM_jobs_kill_type(struct wmWindowManager *wm, void *owner, int job_type)
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (!owner || wm_job->owner == owner) {
      if (ELEM(job_type, WM_JOB_TYPE_ANY, wm_job->job_type)) {
        wm_jobs_kill_job(wm, wm_job);
      }
    }
  }
}

/* signal job(s) from this owner or callback to stop, timer is required to get handled */
void WM_jobs_stop(wmWindowManager *wm, void *owner, void *startjob)
{
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner == owner || wm_job->startjob == startjob) {
      if (wm_job->running) {
        wm_job->stop = true;
      }
    }
  }
}

/* actually terminate thread and job timer */
void WM_jobs_kill(wmWindowManager *wm,
                  void *owner,
                  void (*startjob)(void *, short int *, short int *, float *))
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner == owner || wm_job->startjob == startjob) {
      wm_jobs_kill_job(wm, wm_job);
    }
  }
}

/* kill job entirely, also removes timer itself */
void wm_jobs_timer_end(wmWindowManager *wm, wmTimer *wt)
{
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->wt == wt) {
      wm_jobs_kill_job(wm, wm_job);
      return;
    }
  }
}

/* hardcoded to event TIMERJOBS */
void wm_jobs_timer(wmWindowManager *wm, wmTimer *wt)
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->wt == wt) {
      /* running threads */
      if (wm_job->threads.first) {

        /* let threads get temporary lock over main thread if needed */
        wm_job_main_thread_yield(wm_job);

        /* always call note and update when ready */
        if (wm_job->do_update || wm_job->ready) {
          if (wm_job->update) {
            wm_job->update(wm_job->run_customdata);
          }
          if (wm_job->note) {
            WM_event_add_notifier_ex(wm, wm_job->win, wm_job->note, NULL);
          }

          if (wm_job->flag & WM_JOB_PROGRESS) {
            WM_event_add_notifier_ex(wm, wm_job->win, NC_WM | ND_JOB, NULL);
          }
          wm_job->do_update = false;
        }

        if (wm_job->ready) {
          if (wm_job->endjob) {
            wm_job->endjob(wm_job->run_customdata);
          }

          /* free own data */
          wm_job->run_free(wm_job->run_customdata);
          wm_job->run_customdata = NULL;
          wm_job->run_free = NULL;

#if 0
          if (wm_job->stop) {
            printf("job ready but stopped %s\n", wm_job->name);
          }
          else {
            printf("job finished %s\n", wm_job->name);
          }
#endif

          if (G.debug & G_DEBUG_JOBS) {
            printf("Job '%s' finished in %f seconds\n",
                   wm_job->name,
                   PIL_check_seconds_timer() - wm_job->start_time);
          }

          wm_job->running = false;

          WM_job_main_thread_lock_release(wm_job);
          BLI_threadpool_end(&wm_job->threads);
          WM_job_main_thread_lock_acquire(wm_job);

          if (wm_job->endnote) {
            WM_event_add_notifier_ex(wm, wm_job->win, wm_job->endnote, NULL);
          }

          WM_event_add_notifier_ex(wm, wm_job->win, NC_WM | ND_JOB, NULL);

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
      }
      else if (wm_job->suspended) {
        WM_jobs_start(wm, wm_job);
      }
    }
  }

  /* Update progress bars in windows. */
  wm_jobs_update_progress_bars(wm);
}

bool WM_jobs_has_running(wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->running) {
      return true;
    }
  }

  return false;
}
