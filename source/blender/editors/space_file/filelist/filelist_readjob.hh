/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 *
 * Functions used by multiple read-job types.
 */

#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>

#include "AS_remote_library.hh"

#include "BLI_map.hh"
#include "BLI_mutex.hh"

#include "BKE_report.hh"

namespace blender {

struct BLODataBlockInfo;
struct FileList;
struct FileListInternEntry;
struct Main;
struct wmWindowManager;
namespace asset_system {
class AssetLibrary;
}

struct RemoteLibraryRequest {
  /** Directory the asset library files should be stored in (#bUserAssetLibrary.dirpath). */
  std::string dirpath;

  /** Code requested to cancel the read job. */
  std::atomic<bool> cancel = false;

  /** Is this asset library tagged as loading externally? Used for remote asset libraries to keep
   * the filelist loading running while the library is being downloaded by other code. */
  std::atomic<bool> is_downloading = false;

  /** When downloading remote library pages, ignore pages older than this. They are from a previous
   * download still. Uses the file system clock since others are not fit for file time-stamp
   * comparisons. */
  std::optional<asset_system::RemoteLibraryLoadingStatus::FileSystemTimePoint> request_time =
      std::nullopt;

  std::atomic<bool> metafiles_in_place = false;
  asset_system::RemoteLibraryLoadingStatus::TimePoint last_new_pages_time;
  std::atomic<bool> new_pages_available = false;
};

struct FileListReadJob {
  Mutex lock;
  char main_filepath[1024 /*FILE_MAX*/] = "";
  Main *current_main = nullptr;
  wmWindowManager *wm = nullptr;
  FileList *filelist = nullptr;

  ReportList reports;

  /**
   * The path currently being read, relative to the filelist root directory.
   * Needed for recursive reading. The full file path is then composed like:
   * `<filelist root>/<cur_relbase>/<file name>`.
   * (whereby the file name may also be a library path within a .blend, e.g.
   * `Materials/Material.001`).
   */
  char cur_relbase[1024 + 258 /*FILE_MAX_LIBEXTRA*/] = "";

  /** The current asset library to load. Usually the same as #FileList.asset_library, however
   * sometimes the #FileList one is a combination of multiple other ones ("All" asset library),
   * which need to be loaded individually. Then this can be set to override the #FileList library.
   * Use this in all loading code. */
  asset_system::AssetLibrary *load_asset_library = nullptr;
  /** Set to request a partial read that only adds files representing #Main data (IDs). Used when
   * #Main may have received changes of interest (e.g. asset removed or renamed). */
  bool only_main_data = false;

  /** Trigger a call to #AS_asset_library_load() to update asset catalogs (won't reload the actual
   * assets) */
  std::atomic<bool> reload_asset_library = false;

  Map<std::string, std::unique_ptr<RemoteLibraryRequest>> remote_library_requests;

  std::optional<std::function<void(const asset_system::AssetRepresentation &)>> on_asset_added =
      std::nullopt;

  /** Shallow copy of #filelist for thread-safe access.
   *
   * The job system calls #filelist_readjob_update which moves any read file from #tmp_filelist
   * into #filelist in a thread-safe way.
   *
   * #tmp_filelist also keeps an `AssetLibrary *` so that it can be loaded in the same thread,
   * and moved to #filelist once all categories are loaded.
   *
   * NOTE: #tmp_filelist is freed in #filelist_readjob_free, so any copied pointers need to be
   * set to nullptr to avoid double-freeing them. */
  FileList *tmp_filelist = nullptr;
};

char *current_relpath_append(const FileListReadJob *job_params, const char *filename);

bool filelist_checkdir_return_always_valid(const FileList * /*filelist*/,
                                           char /*dirpath*/[1024 + 258 /*FILE_MAX_LIBEXTRA*/],
                                           const bool /*do_change*/);

bool filelist_readjob_append_entries(FileListReadJob *job_params,
                                     ListBaseT<FileListInternEntry> *from_entries,
                                     int from_entries_num);

/**
 * \warning: This "steals" the asset metadata from \a datablock_info. Not great design but fixing
 *           this requires redesigning things on the caller side for proper ownership management.
 */
void filelist_readjob_list_lib_add_datablock(
    FileListReadJob *job_params,
    ListBaseT<FileListInternEntry> *entries,
    BLODataBlockInfo *datablock_info,
    const bool prefix_relpath_with_group_name,
    const int idcode,
    const char *group_name,
    const std::optional<asset_system::OnlineAssetInfo> online_asset_info = std::nullopt);

void filelist_readjob_recursive_dir_add_items(const bool do_lib,
                                              FileListReadJob *job_params,
                                              const bool *stop,
                                              bool *do_update,
                                              float *progress);
void filelist_readjob_directories_and_libraries(const bool do_lib,
                                                FileListReadJob *job_params,
                                                const bool *stop,
                                                bool *do_update,
                                                float *progress);

void filelist_readjob_dir(FileListReadJob *job_params,
                          bool *stop,
                          bool *do_update,
                          float *progress);

void filelist_readjob_main_assets_add_items(FileListReadJob *job_params,
                                            bool * /*stop*/,
                                            bool *do_update,
                                            float * /*progress*/);

void filelist_readjob_load_asset_library_data(FileListReadJob *job_params, bool *do_update);

void remote_asset_library_request(FileListReadJob *job_params, bUserAssetLibrary &library);
void remote_asset_library_load(FileListReadJob *job_params,
                               RemoteLibraryRequest &request,
                               bool *stop,
                               bool *do_update,
                               float *progress);
void filelist_timer_step_remote_asset_library(FileListReadJob *job_params);

}  // namespace blender
