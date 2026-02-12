/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender {
struct bContext;
struct bUserAssetLibrary;
struct Main;
struct ReportList;

namespace asset_system {

/**
 * Iterates all libraries registers in the Preferences and calls the given function with the URL
 * of the library.
 */
void foreach_registered_remote_library(FunctionRef<void(bUserAssetLibrary &)> fn);

/**
 * Combination of a URL of a remote resource, and its hash.
 */
struct URLWithHash {
  std::string url;
  /** String in the form `{HASH_TYPE}:{HASH_VALUE}`. */
  std::string hash;
};

/** Information of a single file of an online asset. */
struct OnlineAssetFile {
  /**
   * The path within the asset library this file should be downloaded to.
   * Relative to the library root.
   */
  std::string path;
  /** The URL the asset should be downloaded from. */
  URLWithHash url;
};

/**
 * Information specific to online assets.
 *
 * This is constructed from the remote asset listing and contains all data needed to download and
 * verify related fragments. #AssetRepresentation stores this for online assets.
 */
struct OnlineAssetInfo {
  /**
   * The files for this asset.
   * The first one contains the asset data-blocks, and subsequent files are dependencies.
   */
  Vector<OnlineAssetFile> files;
  std::optional<URLWithHash> preview_url;

  /**
   * Return the asset's main file, i.e. the file containing the asset data-block.
   *
   * This can only return an empty string in error cases, i.e. when the `files` vector (see above)
   * is empty. This should never happen; file-less assets should be rejected when loading the
   * listing.
   *
   * NOTE: Blender currently only has preliminary support for multi-file assets (it downloads them
   * correctly, but there's little in place to check for conflicting versions, or to handle things
   * like copying non-blend files to the project directory). Even though the 'files' list will
   * likely only have one element (at least that is the case at the time of writing), this function
   * should not be used as a shortcut when trying to obtain "the asset's files".
   */
  StringRefNull asset_file() const;
};

class AssetRepresentation;

/**
 * Ensures the remote library cache directory exists, and calls the Python downloader. Doesn't do
 * anything if a download with the library's URL is already ongoing.
 */
void remote_library_request_download(const bUserAssetLibrary &library_definition);

void remote_library_request_asset_download(const bContext &C,
                                           const AssetRepresentation &asset,
                                           ReportList *reports);
void remote_library_request_preview_download(const bContext &C,
                                             const AssetRepresentation &asset,
                                             const StringRef dst_filepath,
                                             ReportList *reports);

/**
 * Get the absolute file path the preview for \a asset is expected at once downloaded.
 *
 * The path is built like this:
 * - Online library cache directory (e.g.
 *   `$HOME/.cache/blender/remote-assets/1a2b3c-my.assets.com/`)
 * - `_thumbs/large/`
 * - The first two characters of the MD5 hash of the full asset path
 *   (#AssetRepresentation.full_path()).
 * - The next 30 characters of the MD5 hash.
 * - If the download URL of the preview has an extension (some string after a period), up to 6
 *   characters of that extension. (Previews load fine regardless of the extension. But the
 *   extension is still a useful indicator, and some file browsers can display previews that way.)
 *
 * The reason hashes are used within `_thumbs/large/` instead of the relative path of the asset (or
 * another relative path derived from the preview URL) is to keep paths short enough to not violate
 * path length limitations.
 */
std::string remote_library_asset_preview_path(const AssetRepresentation &asset);

/**
 * Status information about an externally loaded asset library listing, stored globally.
 *
 * Remote asset library downloading is handled in Python. This API allows storing status
 * information globally per URL. Asset UIs can then query the status and reflect it accordingly.
 *
 * Another important use is coordinating the Python side downloading with the C++ side loading.
 * The C++ asset library loading might have to wait for Python to be done downloading and
 * validating individual asset listing pages, and load in these new pages as they become ready.
 *
 * All functions must be called on the same thread.
 */
class RemoteLibraryLoadingStatus {
 public:
  enum Status {
    Loading,
    Finished,
    Failure,
    Cancelled,
  };
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
  using FileSystemTimePoint = std::filesystem::file_time_type;

 private:
  float timeout_ = 0.0f;
  FileSystemTimePoint loading_start_time_point_ = {};
  TimePoint last_updated_time_point_ = {};
  /* See #RemoteLibraryLoadingStatus::handle_timeout(). */
  TimePoint last_timeout_handled_time_point_ = {};
  TimePoint last_new_pages_time_point_ = {};

  std::optional<Status> status_ = std::nullopt;
  std::optional<StringRefNull> failure_message_ = std::nullopt;
  bool metafiles_in_place_ = false;

 public:
  static void begin_loading(StringRef url, float timeout);
  /** Let the state know that the loading is still ongoing, resetting the timeout. */
  static void ping_still_loading(StringRef url);
  static void ping_new_pages(StringRef url);
  static void ping_new_preview(const bContext &C, StringRef preview_full_filepath);
  static void ping_new_assets(const bContext &C, StringRef url);
  static void ping_metafiles_in_place(StringRef url);
  static void set_finished(StringRef url);
  static void set_failure(StringRef url, std::optional<StringRefNull> failure_message);

  static std::optional<StringRefNull> failure_message(StringRef url);
  static std::optional<RemoteLibraryLoadingStatus::Status> status(StringRef url);
  static std::optional<bool> metafiles_in_place(StringRef url);
  static std::optional<FileSystemTimePoint> loading_start_time(const StringRef url);
  static std::optional<TimePoint> last_new_pages_time(StringRef url);

  /**
   * Checks if the status storage timed out, because it hasn't received status updates for the
   * given timeout duration. Changes the status to failure in that case.
   *
   * Note that this function doesn't do more than check if the timeout is reached, and changing
   * state to failure if so. It's meant to be called in regular, short intervalls to make the whole
   * timeout handling work. Current remote asset library loading takes care of this.
   *
   * \return True if the loading status switched to #Status::Failure due to timing out.
   */
  static bool handle_timeout(StringRef url);

 private:
  /** Update the last update time point, effectively resetting the timout timer. */
  void reset_timeout();
};

}  // namespace asset_system
}  // namespace blender
