/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"
#include "AS_essentials_library.hh"

#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "DNA_asset_types.h"

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

static void filelist_readjob_essentials_asset_library(FileListReadJob *job_params,
                                                      bool *stop,
                                                      bool *do_update,
                                                      float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  BLI_assert(filelist->filelist.entries.is_empty() &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  filelist_readjob_load_asset_library_data(job_params, do_update);

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  /* Read the bundled essentials. */
  if (job_params->filelist->asset_library_ref->type == ASSET_LIBRARY_ESSENTIALS) {
    BLI_assert(BLI_path_cmp_normalized(filelist->filelist.root,
                                       asset_system::essentials_directory_path().c_str()) == 0);
    filelist_readjob_recursive_dir_add_items(true, job_params, stop, do_update, progress);
  }
  else {
    /* Can't actually be selected from the UI. But be nice and support loading just the online
     * essentials. Scripts may want to do that. */
    BLI_assert(job_params->filelist->asset_library_ref->type == ASSET_LIBRARY_ONLINE_ESSENTIALS);
  }

  /* The rest of the function handles online essentials. Can exit early if these are disabled. */
  if (!(U.asset_flag & USER_ASSETS_USE_ONLINE_ESSENTIALS)) {
    return;
  }

  /* Override library info to read online essentials. */
  job_params->load_asset_library = AS_asset_library_load(
      job_params->current_main, asset_system::online_essentials_library_reference());

  STRNCPY(filelist->filelist.root, asset_system::online_essentials_cache_directory_path().c_str());
  BLI_path_slash_ensure(filelist->filelist.root, sizeof(filelist->filelist.root));

  if (job_params->remote_library_requests.is_empty()) {
    filelist_readjob_recursive_dir_add_items(true, job_params, stop, do_update, progress);
    return;
  }
  BLI_assert_msg(job_params->remote_library_requests.size() <= 1,
                 "reading callback for a single remote library should only have a single remote "
                 "library request registered (check what the starting callback is requesting)");
  for (auto [url, request] : job_params->remote_library_requests.items()) {
    /* Will load the already downloaded online essentials. */
    remote_asset_library_load(job_params, *request, stop, do_update, progress);
    break;
  }
}

static void filelist_start_job_essentials_asset_library(FileListReadJob *job_params)
{
  /* Request online essentials library (#remote_asset_library_request() will check if online access
   * is enabled). */
  asset_system::RemoteLibraryDefinitionRef library{
      asset_system::online_essentials_url(),
      asset_system::online_essentials_cache_directory_path()};
  remote_asset_library_request(job_params, library);
}

void filelist_set_readjob_essentials_asset_library(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_return_always_valid;
  filelist->start_job_fn = filelist_start_job_essentials_asset_library;
  filelist->timer_step_fn = filelist_timer_step_remote_asset_library;
  filelist->read_job_fn = filelist_readjob_essentials_asset_library;
  filelist->prepare_filter_fn = prepare_filter_asset_library;
  filelist->filter_fn = is_filtered_asset_library;
  filelist->tags |= FILELIST_TAGS_APPLY_FUZZY_SEARCH;
}

}  // namespace blender
