/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_library.hh"
#include "AS_remote_library.hh"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_preferences.h"

#include "ED_asset_indexer.hh"

#include "filelist_intern.hh"
#include "filelist_readjob.hh"

namespace blender {

using RemoteLibraryLoadingStatus = asset_system::RemoteLibraryLoadingStatus;

/* TODO handle \a progress. */
static void filelist_readjob_remote_asset_library_index_read(
    FileListReadJob *job_params,
    RemoteLibraryRequest &request,
    bool *stop,
    bool *do_update,
    float * /*progress*/,
    const Set<StringRef> already_downloaded_asset_identifiers)
{
  using namespace ed::asset;

  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  char dirpath[FILE_MAX];
  StringRef(request.dirpath).copy_utf8_truncated(dirpath);

  BLI_path_normalize_dir(dirpath, sizeof(dirpath));
  if (!BLI_is_dir(dirpath)) {
    return;
  }

  /* #index::read_remote_listing() below calls this for every asset entry it finished reading from
   * the asset listing pages. */
  const auto process_asset_fn = [&](index::RemoteListingAssetEntry &entry) {
    if (*stop || request.cancel) {
      /* Cancel reading when requested. */
      return false;
    }

    const char *group_name = BKE_idtype_idcode_to_name(entry.idcode);

    /* Skip assets that are already listed with the downloaded assets. */
    const StringRefNull asset_file = entry.online_info.asset_file();
    {
      BLI_assert(asset_file.endswith(".blend"));

      /* Matches #asset_system::AssetRepresentation.library_relative_identifier(). */
      char asset_identifier[FILE_MAX_LIBEXTRA];
      BLI_string_join(asset_identifier,
                      sizeof(asset_identifier),
                      asset_file.c_str(),
                      SEP_STR,
                      group_name,
                      SEP_STR,
                      entry.datablock_info.name);
      if (already_downloaded_asset_identifiers.contains(asset_identifier)) {
        return true;
      }
    }

    ListBaseT<FileListInternEntry> entries = {nullptr};

    BLI_strncpy(job_params->cur_relbase, asset_file.c_str(), sizeof(job_params->cur_relbase));
    filelist_readjob_list_lib_add_datablock(job_params,
                                            &entries,
                                            &entry.datablock_info,
                                            true,
                                            entry.idcode,
                                            group_name,
                                            entry.online_info);

    int entries_num = 0;
    for (FileListInternEntry &entry : entries) {
      entry.uid = filelist_uid_generate(filelist);
      char dir[FILE_MAX_LIBEXTRA];
      entry.name = fileentry_uiname(dirpath, &entry, dir);
      entry.free_name = true;
      entries_num++;
    }

    if (filelist_readjob_append_entries(job_params, &entries, entries_num)) {
      *do_update = true;
    }
    return true;
  };
  /* A busy wait function for while asset listing pages are being downloaded.
   * #index::read_remote_listing() calls this every time it's done looking for new pages, until all
   * pages are there (or until this returns false). */
  const auto wait_for_pages_fn = [&]() {
    while (true) {
      if (*stop || request.cancel) {
        return false;
      }

      /* Atomically test and reset the new pages flag. */
      if (request.new_pages_available.exchange(false) || !request.is_downloading) {
        /* New pages available or loading ended. Done waiting. */
        return true;
      }

      /* Busy waiting for new files, with some sleeping to avoid wasting a lot of CPU
       * cycles. */
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };

  if (!index::read_remote_listing(dirpath,
                                  job_params->load_asset_library->name(),
                                  job_params->reports,
                                  process_asset_fn,
                                  wait_for_pages_fn,
                                  request.request_time))
  {
    return;
  }
}

/* Used by the remote library loading job and the "All" library. */
void remote_asset_library_load(FileListReadJob *job_params,
                               RemoteLibraryRequest &request,
                               bool *stop,
                               bool *do_update,
                               float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  Set<StringRef> already_downloaded_asset_identifiers;
  /* Get assets that were downloaded already. */
  {
    job_params->on_asset_added =
        [&already_downloaded_asset_identifiers](const asset_system::AssetRepresentation &asset) {
          already_downloaded_asset_identifiers.add(asset.library_relative_identifier());
        };

    float progress_on_disk = 0.0;

    filelist_readjob_recursive_dir_add_items(true, job_params, stop, do_update, &progress_on_disk);
    job_params->on_asset_added = std::nullopt;

    /* A bit arbitrary: Let on-disk reading only take up to 10% of the total progress. We don't
     * have enough data here to make a more informed choice. But practically the downloading is
     * probably the bigger bottleneck than the listing of already downloaded assets directly from
     * disk. For assets on disk there's the local asset index anyway, so listing them should be
     * fast. Plus, giving 90% to the remaining work can make it feel like there's more steady
     * progress towards the end, which is nicer for users. */
    *progress = progress_on_disk * 0.1f;
  }

  BLI_assert(job_params->load_asset_library &&
             (job_params->load_asset_library->library_type() != ASSET_LIBRARY_ALL));

  while (request.is_downloading && !request.metafiles_in_place) {
    /* Busy waiting for the metafiles, with some sleeping to avoid wasting a lot of CPU
     * cycles. */
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    if (*stop || request.cancel) {
      return;
    }
  }

  if ((filelist->flags & FL_ASSETS_INCLUDE_ONLINE) == 0) {
    return;
  }

  /* Enforce latest catalogs from the downloader to be used. */
  job_params->load_asset_library->load_or_reload_catalogs();

  if (*stop || request.cancel) {
    return;
  }

  filelist_readjob_remote_asset_library_index_read(
      job_params, request, stop, do_update, progress, already_downloaded_asset_identifiers);
}

static void filelist_remote_asset_library_update_loading_flags(RemoteLibraryRequest &request,
                                                               StringRef remote_url)
{
  /* On timeout the loading status will be set to cancelled. */
  if (RemoteLibraryLoadingStatus::handle_timeout(remote_url)) {
    request.cancel = true;
  }

  const auto last_new_pages_time = RemoteLibraryLoadingStatus::last_new_pages_time(remote_url);
  if (last_new_pages_time && *last_new_pages_time != request.last_new_pages_time) {
    request.new_pages_available = true;
    request.last_new_pages_time = *last_new_pages_time;
  }
  request.is_downloading = RemoteLibraryLoadingStatus::status(remote_url) ==
                           RemoteLibraryLoadingStatus::Loading;
  request.metafiles_in_place =
      RemoteLibraryLoadingStatus::metafiles_in_place(remote_url).value_or(false);
}

/* Called when starting the job (from the main thread). */
void remote_asset_library_request(FileListReadJob *job_params, bUserAssetLibrary &library)
{
  if (!USER_EXPERIMENTAL_TEST(&U, use_remote_asset_libraries)) {
    return;
  }
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    return;
  }
  if ((job_params->filelist->flags & FL_ASSETS_INCLUDE_ONLINE) == 0) {
    return;
  }

  /* Check if the library's cache directory exists, otherwise, request download. */
  if (!BLI_is_dir(library.dirpath)) {
    blender::asset_system::remote_library_request_download(library);
  }

  std::unique_ptr<RemoteLibraryRequest> request = std::make_unique<RemoteLibraryRequest>();
  request->dirpath = library.dirpath;
  request->request_time = RemoteLibraryLoadingStatus::loading_start_time(library.remote_url);

  filelist_remote_asset_library_update_loading_flags(*request, library.remote_url);

  job_params->remote_library_requests.add(library.remote_url, std::move(request));
}

static bool filelist_checkdir_remote_asset_library(const FileList * /*filelist*/,
                                                   char /*dirpath*/[FILE_MAX_LIBEXTRA],
                                                   const bool /*do_change*/)
{
  return (G.f & G_FLAG_INTERNET_ALLOW) != 0;
}

static bUserAssetLibrary *lookup_remote_library(const FileListReadJob *job_params)
{
  bUserAssetLibrary *library = BKE_preferences_asset_library_find_index(
      &U, job_params->filelist->asset_library_ref->custom_library_index);
  if (!library && !(library->flag & ASSET_LIBRARY_USE_REMOTE_URL)) {
    return nullptr;
  }

  return library;
}

static void filelist_start_job_remote_asset_library(FileListReadJob *job_params)
{
  if (bUserAssetLibrary *library = lookup_remote_library(job_params)) {
    remote_asset_library_request(job_params, *library);
  }
}

static void filelist_readjob_remote_asset_library(FileListReadJob *job_params,
                                                  bool *stop,
                                                  bool *do_update,
                                                  float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  filelist_readjob_load_asset_library_data(job_params, do_update);

  BLI_assert_msg(job_params->remote_library_requests.size() == 1,
                 "reading callback for a single remote library should only have a single remote "
                 "library request registered (check what the starting callback is requesting)");
  for (auto [url, request] : job_params->remote_library_requests.items()) {
    remote_asset_library_load(job_params, *request, stop, do_update, progress);
    break;
  }
}

/* This may also be called for the "All" asset library. */
void filelist_timer_step_remote_asset_library(FileListReadJob *job_params)
{
  for (auto [url, request] : job_params->remote_library_requests.items()) {
    filelist_remote_asset_library_update_loading_flags(*request, url);
  }
}

void filelist_set_readjob_remote_asset_library(FileList *filelist)
{
  /* TODO rename to something like #is_valid_fn(). */
  filelist->check_dir_fn = filelist_checkdir_remote_asset_library;
  filelist->start_job_fn = filelist_start_job_remote_asset_library;
  filelist->timer_step_fn = filelist_timer_step_remote_asset_library;
  filelist->read_job_fn = filelist_readjob_remote_asset_library;
  filelist->prepare_filter_fn = prepare_filter_asset_library;
  filelist->filter_fn = is_filtered_asset_library;
  filelist->tags |= FILELIST_TAGS_APPLY_FUZZY_SEARCH;
}

}  // namespace blender
