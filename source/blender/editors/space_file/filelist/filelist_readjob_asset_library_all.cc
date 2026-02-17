/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

static void filelist_readjob_all_asset_library(FileListReadJob *job_params,
                                               bool *stop,
                                               bool *do_update,
                                               float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  filelist_readjob_load_asset_library_data(job_params, do_update);

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  asset_system::AssetLibrary *current_file_library;
  {
    AssetLibraryReference library_ref{};
    library_ref.custom_library_index = -1;
    library_ref.type = ASSET_LIBRARY_LOCAL;

    current_file_library = AS_asset_library_load(job_params->current_main, library_ref);
  }

  job_params->load_asset_library = current_file_library;
  filelist_readjob_main_assets_add_items(job_params, stop, do_update, progress);

  /* When only doing partially reload for main data, we're done. */
  if (job_params->only_main_data) {
    return;
  }

  /* Count how many asset libraries need to be loaded, for progress reporting. Not very precise. */
  int library_count = 0;
  asset_system::AssetLibrary::foreach_loaded([&](const auto &) { library_count++; }, false);

  BLI_assert(filelist->asset_library != nullptr);

  int libraries_done_count = 0;
  /* The "All" asset library was loaded, which means all other asset libraries are also loaded.
   * Load their assets from disk into the "All" library. */
  asset_system::AssetLibrary::foreach_loaded(
      [&](asset_system::AssetLibrary &nested_library) {
        StringRefNull root_path = nested_library.root_path();
        if (root_path.is_empty()) {
          return;
        }
        if (&nested_library == current_file_library) {
          /* Skip the "Current File" library, it's already loaded above. */
          return;
        }

        /* Override library info to read this library. */
        job_params->load_asset_library = &nested_library;
        STRNCPY(filelist->filelist.root, root_path.c_str());

        float progress_this = 0.0f;
        /* Online asset libraries: */
        if (std::optional<std::string> remote_url = nested_library.remote_url()) {
          if (std::unique_ptr<RemoteLibraryRequest> *request =
                  job_params->remote_library_requests.lookup_ptr(*remote_url))
          {
            remote_asset_library_load(job_params, **request, stop, do_update, &progress_this);
          }
          /* When online assets or online access are disabled, there will be no requests. In that
           * case, just list the assets that are downloaded already. */
          else {
            filelist_readjob_recursive_dir_add_items(
                true, job_params, stop, do_update, &progress_this);
          }
        }
        /* Simple directory based reading. */
        else {
          filelist_readjob_recursive_dir_add_items(
              true, job_params, stop, do_update, &progress_this);
        }

        libraries_done_count++;
        *progress = float(libraries_done_count) / library_count;
      },
      false);
}

static void filelist_start_job_all_asset_library(FileListReadJob *job_params)
{
  Set<StringRef> requested_urls;

  asset_system::foreach_registered_remote_library([&](bUserAssetLibrary &library) {
    if (!requested_urls.contains(library.remote_url)) {
      requested_urls.add(library.remote_url);

      remote_asset_library_request(job_params, library);
    }
  });
}

void filelist_set_readjob_all_asset_library(FileList *filelist)
{
  filelist->check_dir_fn = filelist_checkdir_return_always_valid;
  filelist->start_job_fn = filelist_start_job_all_asset_library;
  filelist->timer_step_fn = filelist_timer_step_remote_asset_library;
  filelist->read_job_fn = filelist_readjob_all_asset_library;
  filelist->prepare_filter_fn = prepare_filter_asset_library;
  filelist->filter_fn = is_filtered_asset_library;
  filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA | FILELIST_TAGS_APPLY_FUZZY_SEARCH;
}

}  // namespace blender
