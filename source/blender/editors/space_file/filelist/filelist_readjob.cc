/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "BKE_context.hh"
#include "BKE_main.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "../filelist.hh"
#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

static void filelist_readjob_initjob(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);
  if (flrj->filelist->start_job_fn) {
    flrj->filelist->start_job_fn(flrj);
  }
}

/**
 * Check if the read-job is requesting a partial reread of the file list only.
 */
static bool filelist_readjob_is_partial_read(const FileListReadJob *read_job)
{
  return read_job->only_main_data;
}

/**
 * \note This may trigger partial filelist reading. If the #FL_FORCE_RESET_MAIN_FILES flag is set,
 *       some current entries are kept and we just call the readjob to update the main files (see
 *       #FileListReadJob.only_main_data).
 */
static void filelist_readjob_startjob(void *flrjv, wmJobWorkerStatus *worker_status)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  //  printf("START filelist reading (%d files, main thread: %d)\n",
  //         flrj->filelist->filelist.entries_num, BLI_thread_is_main());

  {
    std::scoped_lock lock(flrj->lock);
    BLI_assert((flrj->tmp_filelist == nullptr) && flrj->filelist);

    flrj->tmp_filelist = MEM_dupalloc(flrj->filelist);

    BLI_listbase_clear(&flrj->tmp_filelist->filelist.entries);
    flrj->tmp_filelist->filelist.entries_num = FILEDIR_NBR_ENTRIES_UNSET;

    flrj->tmp_filelist->filelist_intern.filtered = nullptr;
    BLI_listbase_clear(&flrj->tmp_filelist->filelist_intern.entries);
    if (filelist_readjob_is_partial_read(flrj)) {
      /* Don't unset the current UID on partial read, would give duplicates otherwise. */
    }
    else {
      filelist_uid_unset(&flrj->tmp_filelist->filelist_intern.curr_uid);
    }

    flrj->tmp_filelist->libfiledata = nullptr;
    flrj->tmp_filelist->filelist_cache = nullptr;
    flrj->tmp_filelist->selection_state = nullptr;
    flrj->tmp_filelist->asset_library_ref = nullptr;
    flrj->tmp_filelist->filter_data.asset_catalog_filter = nullptr;
  }

  flrj->tmp_filelist->read_job_fn(
      flrj, &worker_status->stop, &worker_status->do_update, &worker_status->progress);
}

/**
 * \note This may update for a partial filelist reading job. If the #FL_FORCE_RESET_MAIN_FILES flag
 *       is set, some current entries are kept and we just call the readjob to update the main
 *       files (see #FileListReadJob.only_main_data).
 */
static void filelist_readjob_update(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);
  FileListIntern *fl_intern = &flrj->filelist->filelist_intern;
  ListBaseT<FileListInternEntry> new_entries = {nullptr};
  int entries_num, new_entries_num = 0;

  BLI_movelisttolist(&new_entries, &fl_intern->entries);
  entries_num = flrj->filelist->filelist.entries_num;

  {
    std::scoped_lock lock(flrj->lock);
    if (flrj->tmp_filelist->filelist.entries_num > 0) {
      /* We just move everything out of 'thread context' into final list. */
      new_entries_num = flrj->tmp_filelist->filelist.entries_num;
      BLI_movelisttolist(&new_entries, &flrj->tmp_filelist->filelist.entries);
      flrj->tmp_filelist->filelist.entries_num = 0;
    }

    if (flrj->tmp_filelist->asset_library) {
      flrj->filelist->asset_library = flrj->tmp_filelist->asset_library;
    }

    /* Important for partial reads: Copy increased UID counter back to the real list. */
    fl_intern->curr_uid = std::max(flrj->tmp_filelist->filelist_intern.curr_uid,
                                   fl_intern->curr_uid);
  }

  if (new_entries_num) {
    /* Do not clear selection cache, we can assume already 'selected' UIDs are still valid! Keep
     * the asset library data we just read. */
    filelist_clear_ex(flrj->filelist, false, true, false);

    flrj->filelist->flags |= (FL_NEED_SORTING | FL_NEED_FILTERING);
  }

  /* if no new_entries_num, this is NOP */
  BLI_movelisttolist(&fl_intern->entries, &new_entries);
  flrj->filelist->filelist.entries_num = std::max(entries_num, 0) + new_entries_num;
}

static void filelist_readjob_timer_step(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  if (flrj->filelist->timer_step_fn) {
    flrj->filelist->timer_step_fn(flrj);
  }
}

static void filelist_readjob_endjob(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  /* In case there would be some dangling update... */
  filelist_readjob_update(flrjv);

  flrj->filelist->flags &= ~FL_IS_PENDING;
  flrj->filelist->flags |= FL_IS_READY;

  WM_reports_from_reports_move(flrj->wm, &flrj->reports);
  BKE_reports_free(&flrj->reports);
}

static void filelist_readjob_free(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  //  printf("END filelist reading (%d files)\n", flrj->filelist->filelist.entries_num);

  if (flrj->tmp_filelist) {
    /* tmp_filelist shall never ever be filtered! */
    BLI_assert(flrj->tmp_filelist->filelist.entries_num == 0);
    BLI_assert(BLI_listbase_is_empty(&flrj->tmp_filelist->filelist.entries));

    filelist_freelib(flrj->tmp_filelist);
    filelist_free(flrj->tmp_filelist);
  }

  MEM_delete(flrj);
}

static eWM_JobType filelist_jobtype_get(const FileList *filelist)
{
  if (filelist->asset_library_ref) {
    return WM_JOB_TYPE_ASSET_LIBRARY_LOAD;
  }
  return WM_JOB_TYPE_FILESEL_READDIR;
}

/* TODO(Julian): This is temporary, because currently the job system identifies jobs to suspend by
 * the startjob callback, rather than the type. See PR #123033. */
static void assetlibrary_readjob_startjob(void *flrjv, wmJobWorkerStatus *worker_status)
{
  filelist_readjob_startjob(flrjv, worker_status);
}

static void filelist_readjob_start_ex(FileList *filelist,
                                      const int space_notifier,
                                      const bContext *C,
                                      const bool force_blocking_read)
{
  Main *bmain = CTX_data_main(C);
  wmJob *wm_job;
  FileListReadJob *flrj;

  if (!filelist_is_dir(filelist, filelist->filelist.root)) {
    return;
  }

  /* prepare job data */
  flrj = MEM_new<FileListReadJob>(__func__);
  flrj->filelist = filelist;
  flrj->current_main = bmain;
  flrj->wm = CTX_wm_manager(C);
  STRNCPY(flrj->main_filepath, BKE_main_blendfile_path(bmain));
  if ((filelist->flags & FL_FORCE_RESET_MAIN_FILES) && !(filelist->flags & FL_FORCE_RESET) &&
      (filelist->filelist.entries_num != FILEDIR_NBR_ENTRIES_UNSET))
  {
    flrj->only_main_data = true;
  }
  if (filelist->flags & FL_RELOAD_ASSET_LIBRARY) {
    flrj->reload_asset_library = true;
  }
  BKE_reports_init(&flrj->reports, RPT_STORE | RPT_PRINT);
  BKE_report_print_level_set(&flrj->reports, RPT_WARNING);

  filelist->flags &= ~(FL_FORCE_RESET | FL_FORCE_RESET_MAIN_FILES | FL_RELOAD_ASSET_LIBRARY |
                       FL_IS_READY);
  filelist->flags |= FL_IS_PENDING;

  /* The file list type may not support threading so execute immediately. Same when only rereading
   * #Main data (which we do quite often on changes to #Main, since it's the easiest and safest way
   * to ensure the displayed data is up to date), because some operations executing right after
   * main data changed may need access to the ID files (see #93691). */
  const bool no_threads = (filelist->tags & FILELIST_TAGS_NO_THREADS) || flrj->only_main_data;

  if (force_blocking_read || no_threads) {
    /* Single threaded execution. Just directly call the callbacks. */
    wmJobWorkerStatus worker_status = {};
    filelist_readjob_startjob(flrj, &worker_status);
    filelist_readjob_endjob(flrj);
    filelist_readjob_free(flrj);

    WM_event_add_notifier(C, space_notifier | NA_JOB_FINISHED, nullptr);
    return;
  }

  /* setup job */
  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       filelist,
                       filelist->asset_library_ref ? "Loading Asset Library..." :
                                                     "Listing directories...",
                       WM_JOB_PROGRESS,
                       filelist_jobtype_get(filelist));
  WM_jobs_customdata_set(wm_job, flrj, filelist_readjob_free);
  WM_jobs_timer(
      wm_job, 0.01, space_notifier, space_notifier | NA_JOB_FINISHED, filelist_readjob_timer_step);
  WM_jobs_callbacks(wm_job,
                    filelist->asset_library_ref ? assetlibrary_readjob_startjob :
                                                  filelist_readjob_startjob,
                    filelist_readjob_initjob,
                    filelist_readjob_update,
                    filelist_readjob_endjob);

  /* start the job */
  WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void filelist_readjob_start(FileList *filelist, const int space_notifier, const bContext *C)
{
  filelist_readjob_start_ex(filelist, space_notifier, C, false);
}

void filelist_readjob_blocking_run(FileList *filelist, int space_notifier, const bContext *C)
{
  filelist_readjob_start_ex(filelist, space_notifier, C, true);
}

void filelist_readjob_stop(FileList *filelist, wmWindowManager *wm)
{
  WM_jobs_kill_type(wm, filelist, filelist_jobtype_get(filelist));
}

int filelist_readjob_running(FileList *filelist, wmWindowManager *wm)
{
  return WM_jobs_test(wm, filelist, filelist_jobtype_get(filelist));
}

}  // namespace blender
