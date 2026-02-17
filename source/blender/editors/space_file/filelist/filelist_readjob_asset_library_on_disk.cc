
/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "BKE_main.hh"

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

/**
 * Check if \a bmain is stored within the root path of \a filelist. This means either directly or
 * in some nested directory. In other words, it checks if the \a filelist root path is contained in
 * the path to \a bmain.
 * This is irrespective of the recursion level displayed, it basically assumes unlimited recursion
 * levels.
 */
static bool filelist_contains_main(const FileList *filelist, const Main *bmain)
{
  if (filelist->asset_library_ref && (filelist->asset_library_ref->type == ASSET_LIBRARY_ALL)) {
    return true;
  }

  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  return blendfile_path[0] && BLI_path_contains(filelist->filelist.root, blendfile_path);
}

static void filelist_readjob_asset_library(FileListReadJob *job_params,
                                           bool *stop,
                                           bool *do_update,
                                           float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  BLI_assert(job_params->filelist->asset_library_ref != nullptr);

  /* NOP if already read. */
  filelist_readjob_load_asset_library_data(job_params, do_update);

  if (filelist_contains_main(filelist, job_params->current_main)) {
    asset_system::AssetLibrary *original_file_library = job_params->load_asset_library;
    job_params->load_asset_library = AS_asset_library_load(
        job_params->current_main, asset_system::current_file_library_reference());
    filelist_readjob_main_assets_add_items(job_params, stop, do_update, progress);
    job_params->load_asset_library = original_file_library;
  }
  if (!job_params->only_main_data) {
    filelist_readjob_recursive_dir_add_items(true, job_params, stop, do_update, progress);
  }
}

void filelist_set_readjob_on_disk_asset_library(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_lib;
  filelist->read_job_fn = filelist_readjob_asset_library;
  filelist->prepare_filter_fn = prepare_filter_asset_library;
  filelist->filter_fn = is_filtered_asset_library;
  filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA | FILELIST_TAGS_APPLY_FUZZY_SEARCH;
}

}  // namespace blender
