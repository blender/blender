/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Threaded job manager (high level job access).
 */

#include <cstring>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_global.hh"
#include "BKE_report.hh"

#include "SEQ_prefetch.hh"

#include "WM_api.hh"
#include "WM_types.hh"
#include "wm.hh"
#include "wm_event_types.hh"

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
  wmJob *next, *prev;

  /** Job originating from, keep track of this when deleting windows. */
  wmWindow *win;

  /** Should store entirely owned context, for start, update, free. */
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
  /**
   * Called when job is stopped normally, i.e. by simply completing the startjob function.
   * Executed in main thread.
   */
  void (*completed)(void *);
  /**
   * Called when job is stopped abnormally, i.e. when stop=true but ready=false.
   * Executed in main thread.
   */
  void (*canceled)(void *);

  /** Running jobs each have their own timer. */
  double time_step;
  wmTimer *wt;
  /** Only start job after specified time delay. */
  double start_delay_time;
  /** The notifier event timers should send. */
  uint note, endnote;

  /* Internal. */
  const void *owner;
  eWM_JobFlag flag;
  bool suspended, running, ready;
  eWM_JobType job_type;

  /** Data shared with the worker code, so can be accessed and edited from several threads. */
  wmJobWorkerStatus worker_status;

  /** For display in header, identification. */
  char name[128];

  /** Once running, we store this separately. */
  void *run_customdata;
  void (*run_free)(void *);

  /** We use BLI_threads api, but per job only 1 thread runs. */
  ListBase threads;

  double start_time;

  /**
   * Ticket mutex for main thread locking while some job accesses
   * data that the main thread might modify at the same time.
   */
  TicketMutex *main_thread_mutex;
};

/* Main thread locking. */

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
  /* Unlock and lock the ticket mutex. because it's a fair mutex any job that
   * is waiting to acquire the lock will get it first, before we can lock. */
  BLI_ticket_mutex_unlock(wm_job->main_thread_mutex);
  BLI_ticket_mutex_lock(wm_job->main_thread_mutex);
}

/**
 * Finds if type or owner, compare for it, otherwise any matching job.
 */
static wmJob *wm_job_find(const wmWindowManager *wm, const void *owner, const eWM_JobType job_type)
{
  if (owner && (job_type != WM_JOB_TYPE_ANY)) {
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
  else if (job_type != WM_JOB_TYPE_ANY) {
    LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
      if (wm_job->job_type == job_type) {
        return wm_job;
      }
    }
  }

  return nullptr;
}

/* ******************* public API ***************** */

wmJob *WM_jobs_get(wmWindowManager *wm,
                   wmWindow *win,
                   const void *owner,
                   const char *name,
                   const eWM_JobFlag flag,
                   const eWM_JobType job_type)
{
  wmJob *wm_job = wm_job_find(wm, owner, job_type);

  if (wm_job == nullptr) {
    wm_job = static_cast<wmJob *>(MEM_callocN(sizeof(wmJob), "new job"));

    BLI_addtail(&wm->jobs, wm_job);
    wm_job->win = win;
    wm_job->owner = owner;
    wm_job->flag = flag;
    wm_job->job_type = job_type;
    STRNCPY(wm_job->name, name);

    wm_job->main_thread_mutex = BLI_ticket_mutex_alloc();
    WM_job_main_thread_lock_acquire(wm_job);

    wm_job->worker_status.reports = MEM_new<ReportList>(__func__);
    BKE_reports_init(wm_job->worker_status.reports, RPT_STORE | RPT_PRINT);
    BKE_report_print_level_set(wm_job->worker_status.reports, RPT_WARNING);
  }
  /* Else: a running job, be careful. */

  /* Prevent creating a job with an invalid type. */
  BLI_assert(wm_job->job_type != WM_JOB_TYPE_ANY);

  return wm_job;
}

bool WM_jobs_test(const wmWindowManager *wm, const void *owner, int job_type)
{
  /* Job can be running or about to run (suspended). */
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner != owner) {
      continue;
    }

    if (!ELEM(job_type, WM_JOB_TYPE_ANY, wm_job->job_type)) {
      continue;
    }

    if ((wm_job->flag & WM_JOB_PROGRESS) && (wm_job->running || wm_job->suspended)) {
      return true;
    }
  }

  return false;
}

float WM_jobs_progress(const wmWindowManager *wm, const void *owner)
{
  const wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job && wm_job->flag & WM_JOB_PROGRESS) {
    return wm_job->worker_status.progress;
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
        /* Accumulate global progress for running jobs. */
        jobs_progress++;
        total_progress += wm_job->worker_status.progress;
      }
    }
  }

  /* If there are running jobs, set the global progress indicator. */
  if (jobs_progress > 0) {
    float progress = total_progress / float(jobs_progress);

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

double WM_jobs_starttime(const wmWindowManager *wm, const void *owner)
{
  const wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job && wm_job->flag & WM_JOB_PROGRESS) {
    return wm_job->start_time;
  }

  return 0;
}

const char *WM_jobs_name(const wmWindowManager *wm, const void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);

  if (wm_job) {
    return wm_job->name;
  }

  return nullptr;
}

void *WM_jobs_customdata_from_type(wmWindowManager *wm, const void *owner, int job_type)
{
  wmJob *wm_job = wm_job_find(wm, owner, eWM_JobType(job_type));

  if (wm_job) {
    return WM_jobs_customdata_get(wm_job);
  }

  return nullptr;
}

bool WM_jobs_is_running(const wmJob *wm_job)
{
  return wm_job->running;
}

bool WM_jobs_is_stopped(const wmWindowManager *wm, const void *owner)
{
  wmJob *wm_job = wm_job_find(wm, owner, WM_JOB_TYPE_ANY);
  return wm_job ? wm_job->worker_status.stop : true; /* XXX to be redesigned properly. */
}

void *WM_jobs_customdata_get(wmJob *wm_job)
{
  if (!wm_job->customdata) {
    return wm_job->run_customdata;
  }
  return wm_job->customdata;
}

void WM_jobs_customdata_set(wmJob *wm_job, void *customdata, void (*free)(void *customdata))
{
  /* Pending job? just free. */
  if (wm_job->customdata) {
    wm_job->free(wm_job->customdata);
  }

  wm_job->customdata = customdata;
  wm_job->free = free;

  if (wm_job->running) {
    /* Signal job to end. */
    wm_job->worker_status.stop = true;
  }
}

void WM_jobs_timer(wmJob *wm_job, double time_step, uint note, uint endnote)
{
  wm_job->time_step = time_step;
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
  WM_jobs_callbacks_ex(wm_job, startjob, initjob, update, endjob, nullptr, nullptr);
}

void WM_jobs_callbacks_ex(wmJob *wm_job,
                          wm_jobs_start_callback startjob,
                          void (*initjob)(void *),
                          void (*update)(void *),
                          void (*endjob)(void *),
                          void (*completed)(void *),
                          void (*canceled)(void *))
{
  wm_job->startjob = startjob;
  wm_job->initjob = initjob;
  wm_job->update = update;
  wm_job->endjob = endjob;
  wm_job->completed = completed;
  wm_job->canceled = canceled;
}

static void wm_jobs_reports_update(wmWindowManager *wm, wmJob *wm_job)
{
  WM_reports_from_reports_move(wm, wm_job->worker_status.reports);
}

static void *do_job_thread(void *job_v)
{
  wmJob *wm_job = static_cast<wmJob *>(job_v);

  wm_job->startjob(wm_job->run_customdata, &wm_job->worker_status);
  wm_job->ready = true;

  return nullptr;
}

/* Don't allow same startjob to be executed twice. */
static void wm_jobs_test_suspend_stop(wmWindowManager *wm, wmJob *test)
{
  bool suspend = false;

  /* Job added with suspend flag, we wait 1 timer step before activating it. */
  if (test->start_delay_time > 0.0) {
    suspend = true;
    test->start_delay_time = 0.0;
  }
  else {
    /* Check other jobs. */
    LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
      /* Obvious case, no test needed. */
      if (wm_job == test || !wm_job->running) {
        continue;
      }

      /* If new job is not render, then check for same startjob. */
      if (0 == (test->flag & WM_JOB_EXCL_RENDER)) {
        if (wm_job->startjob != test->startjob) {
          continue;
        }
      }

      /* If new job is render, any render job should be stopped. */
      if (test->flag & WM_JOB_EXCL_RENDER) {
        if (0 == (wm_job->flag & WM_JOB_EXCL_RENDER)) {
          continue;
        }
      }

      suspend = true;

      /* If this job has higher priority, stop others. */
      if (test->flag & WM_JOB_PRIORITY) {
        wm_job->worker_status.stop = true;
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

void WM_jobs_start(wmWindowManager *wm, wmJob *wm_job)
{
  if (wm_job->running) {
    /* Signal job to end and restart. */
    wm_job->worker_status.stop = true;
    // printf("job started a running job, ending... %s\n", wm_job->name);
  }
  else {

    if (wm_job->customdata && wm_job->startjob) {
      const double time_step = (wm_job->start_delay_time > 0.0) ? wm_job->start_delay_time :
                                                                  wm_job->time_step;

      wm_jobs_test_suspend_stop(wm, wm_job);

      if (wm_job->suspended == false) {
        /* Copy to ensure proper free in end. */
        wm_job->run_customdata = wm_job->customdata;
        wm_job->run_free = wm_job->free;
        wm_job->free = nullptr;
        wm_job->customdata = nullptr;
        wm_job->running = true;

        if (wm_job->initjob) {
          wm_job->initjob(wm_job->run_customdata);
        }

        wm_job->worker_status.stop = false;
        wm_job->ready = false;
        wm_job->worker_status.progress = 0.0;

        // printf("job started: %s\n", wm_job->name);

        BLI_threadpool_init(&wm_job->threads, do_job_thread, 1);
        BLI_threadpool_insert(&wm_job->threads, wm_job);
      }

      /* Restarted job has timer already. */
      if (wm_job->wt && (wm_job->wt->time_step > time_step)) {
        WM_event_timer_remove(wm, wm_job->win, wm_job->wt);
        wm_job->wt = WM_event_timer_add(wm, wm_job->win, TIMERJOBS, time_step);
      }
      if (wm_job->wt == nullptr) {
        wm_job->wt = WM_event_timer_add(wm, wm_job->win, TIMERJOBS, time_step);
      }

      wm_job->start_time = BLI_time_now_seconds();
    }
    else {
      printf("job fails, not initialized\n");
    }
  }
}

static void wm_job_end(wmWindowManager *wm, wmJob *wm_job)
{
  BLI_assert_msg(BLI_thread_is_main(), "wm_job_end should only be called from the main thread");
  if (wm_job->endjob) {
    wm_job->endjob(wm_job->run_customdata);
  }

  /* Do the final callback based on whether the job was run to completion or not.
   * Not all jobs have the same way of signaling cancellation (i.e. rendering stops when
   * `G.is_break == true`, but doesn't set any wm_job properties to cancel the WM job). */
  const bool was_canceled = wm_job->worker_status.stop || G.is_break;
  void (*final_callback)(void *) = (wm_job->ready && !was_canceled) ? wm_job->completed :
                                                                      wm_job->canceled;
  if (final_callback) {
    final_callback(wm_job->run_customdata);
  }

  /* Ensure all reports have been moved to WM. */
  wm_jobs_reports_update(wm, wm_job);
}

static void wm_job_free(wmWindowManager *wm, wmJob *wm_job)
{
  BLI_remlink(&wm->jobs, wm_job);
  WM_job_main_thread_lock_release(wm_job);
  BLI_ticket_mutex_free(wm_job->main_thread_mutex);

  BLI_assert(BLI_listbase_is_empty(&wm_job->worker_status.reports->list));
  BKE_reports_free(wm_job->worker_status.reports);
  MEM_delete(wm_job->worker_status.reports);
  MEM_freeN(wm_job);
}

/* Stop job, end thread, free data completely. */
static void wm_jobs_kill_job(wmWindowManager *wm, wmJob *wm_job)
{
  bool update_progress = (wm_job->flag & WM_JOB_PROGRESS) != 0;

  if (wm_job->running) {
    /* Signal job to end. */
    wm_job->worker_status.stop = true;

    WM_job_main_thread_lock_release(wm_job);
    BLI_threadpool_end(&wm_job->threads);
    WM_job_main_thread_lock_acquire(wm_job);
    wm_job_end(wm, wm_job);
  }

  if (wm_job->wt) {
    WM_event_timer_remove(wm, wm_job->win, wm_job->wt);
  }
  if (wm_job->customdata) {
    wm_job->free(wm_job->customdata);
  }
  if (wm_job->run_customdata) {
    wm_job->run_free(wm_job->run_customdata);
  }

  /* Remove wm_job. */
  wm_job_free(wm, wm_job);

  /* Update progress bars in windows. */
  if (update_progress) {
    wm_jobs_update_progress_bars(wm);
  }
}

void WM_jobs_kill_all(wmWindowManager *wm)
{
  wmJob *wm_job;

  while ((wm_job = static_cast<wmJob *>(wm->jobs.first))) {
    wm_jobs_kill_job(wm, wm_job);
  }

  /* This job will be automatically restarted. */
  SEQ_prefetch_stop_all();
}

void WM_jobs_kill_all_except(wmWindowManager *wm, const void *owner)
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner != owner) {
      wm_jobs_kill_job(wm, wm_job);
    }
  }
}

void WM_jobs_kill_type(wmWindowManager *wm, const void *owner, int job_type)
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (owner && wm_job->owner != owner) {
      continue;
    }

    if (ELEM(job_type, WM_JOB_TYPE_ANY, wm_job->job_type)) {
      wm_jobs_kill_job(wm, wm_job);
    }
  }
}

void WM_jobs_stop(wmWindowManager *wm, const void *owner, wm_jobs_start_callback startjob)
{
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner == owner || wm_job->startjob == startjob) {
      if (wm_job->running) {
        wm_job->worker_status.stop = true;
      }
    }
  }
}

void WM_jobs_kill(wmWindowManager *wm, void *owner, wm_jobs_start_callback startjob)
{
  LISTBASE_FOREACH_MUTABLE (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->owner == owner || wm_job->startjob == startjob) {
      wm_jobs_kill_job(wm, wm_job);
    }
  }
}

void wm_jobs_timer_end(wmWindowManager *wm, wmTimer *wt)
{
  wmJob *wm_job = static_cast<wmJob *>(BLI_findptr(&wm->jobs, wt, offsetof(wmJob, wt)));
  if (wm_job) {
    wm_jobs_kill_job(wm, wm_job);
  }
}

void wm_jobs_timer(wmWindowManager *wm, wmTimer *wt)
{
  wmJob *wm_job = static_cast<wmJob *>(BLI_findptr(&wm->jobs, wt, offsetof(wmJob, wt)));

  if (wm_job) {
    /* Running threads. */
    if (wm_job->threads.first) {
      /* Let threads get temporary lock over main thread if needed. */
      wm_job_main_thread_yield(wm_job);

      /* Always call note and update when ready. */
      if (wm_job->worker_status.do_update || wm_job->ready) {
        if (wm_job->update) {
          wm_job->update(wm_job->run_customdata);
        }
        if (wm_job->note) {
          WM_event_add_notifier_ex(wm, wm_job->win, wm_job->note, nullptr);
        }

        if (wm_job->flag & WM_JOB_PROGRESS) {
          WM_event_add_notifier_ex(wm, wm_job->win, NC_WM | ND_JOB, nullptr);
        }
        wm_job->worker_status.do_update = false;
      }

      if (wm_job->ready) {
        wm_job_end(wm, wm_job);

        /* Free owned data. */
        wm_job->run_free(wm_job->run_customdata);
        wm_job->run_customdata = nullptr;
        wm_job->run_free = nullptr;

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
                 BLI_time_now_seconds() - wm_job->start_time);
        }

        wm_job->running = false;

        WM_job_main_thread_lock_release(wm_job);
        BLI_threadpool_end(&wm_job->threads);
        WM_job_main_thread_lock_acquire(wm_job);

        if (wm_job->endnote) {
          WM_event_add_notifier_ex(wm, wm_job->win, wm_job->endnote, nullptr);
        }

        WM_event_add_notifier_ex(wm, wm_job->win, NC_WM | ND_JOB, nullptr);

        /* New job added for wm_job? */
        if (wm_job->customdata) {
          // printf("job restarted with new data %s\n", wm_job->name);
          WM_jobs_start(wm, wm_job);
        }
        else {
          WM_event_timer_remove(wm, wm_job->win, wm_job->wt);
          wm_job->wt = nullptr;

          /* Remove wm_job. */
          wm_job_free(wm, wm_job);
          wm_job = nullptr;
        }
      }
    }
    else if (wm_job->suspended) {
      WM_jobs_start(wm, wm_job);
    }

    /* Move pending reports generated by the worker thread to the WM main list. */
    if (wm_job) {
      wm_jobs_reports_update(wm, wm_job);
    }
  }

  /* Update progress bars in windows. */
  wm_jobs_update_progress_bars(wm);
}

bool WM_jobs_has_running(const wmWindowManager *wm)
{
  LISTBASE_FOREACH (const wmJob *, wm_job, &wm->jobs) {
    if (wm_job->running) {
      return true;
    }
  }

  return false;
}

bool WM_jobs_has_running_type(const wmWindowManager *wm, int job_type)
{
  LISTBASE_FOREACH (wmJob *, wm_job, &wm->jobs) {
    if (wm_job->running && wm_job->job_type == job_type) {
      return true;
    }
  }
  return false;
}
