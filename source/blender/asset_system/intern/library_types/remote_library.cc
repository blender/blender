/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <fmt/format.h>

#include "BLI_fileops.h"
#include "BLI_hash_md5.hh"
#include "BLI_listbase.h"
#include "BLI_memory_utils.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

#include "DNA_space_enums.h"
#include "DNA_userdef_types.h"

#include "ED_fileselect.hh"
#include "ED_render.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "AS_asset_representation.hh"
#include "AS_remote_library.hh"
#include "remote_library.hh"

namespace blender::asset_system {

RemoteAssetLibrary::RemoteAssetLibrary(const StringRef remote_url,
                                       const StringRef name,
                                       const StringRef cache_root_path)
    : AssetLibrary(ASSET_LIBRARY_CUSTOM, name, cache_root_path)
{
  import_method_ = ASSET_IMPORT_APPEND_REUSE;
  may_override_import_method_ = false;
  remote_url_ = remote_url;
}

std::optional<AssetLibraryReference> RemoteAssetLibrary::library_reference() const
{
  for (auto [i, asset_library] : U.asset_libraries.enumerate()) {
    if ((asset_library.flag & ASSET_LIBRARY_USE_REMOTE_URL) == 0) {
      continue;
    }

    if (asset_library.remote_url == this->remote_url_) {
      AssetLibraryReference library_ref{};
      library_ref.type = ASSET_LIBRARY_CUSTOM;
      library_ref.custom_library_index = i;
      return library_ref;
    }
  }

  return {};
}

std::optional<StringRefNull> RemoteAssetLibrary::remote_url() const
{
  return remote_url_;
}

void RemoteAssetLibrary::refresh_catalogs()
{
  this->catalog_service().reload_catalogs();
}

/* -------------------------------------------------------------------- */
/** \name Remote Library Loading Status
 * \{ */

/*
 * Note: Some of the status setters here only modify status if the current status is
 * #RemoteLibraryLoadingStatus::Loading. That is done to avoid status changes after loading ended,
 * mostly after a timeout. E.g. after a timeout of the C++ status because Python didn't send status
 * updates, Python might eventually resume sending updates. These shouldn't affect the C++ status
 * anymore, since the earlier failure aborted the C++ side asset library loading. Plus, the UI
 * might show an error, and would suddenly switch to showing an incompletly loaded asset library.
 */

using UrlToLibraryStatusMap = Map<std::string /*url*/, asset_system::RemoteLibraryLoadingStatus>;

static UrlToLibraryStatusMap &library_to_status_map()
{
  static UrlToLibraryStatusMap map = UrlToLibraryStatusMap{};
  return map;
}

void RemoteLibraryLoadingStatus::reset_timeout()
{
  this->last_updated_time_point_ = std::chrono::steady_clock::now();
}

void RemoteLibraryLoadingStatus::begin_loading(const StringRef url, const float timeout)
{
  BLI_assert(timeout > 0.0f);

  RemoteLibraryLoadingStatus new_status{};
  new_status.timeout_ = timeout;
  new_status.status_ = RemoteLibraryLoadingStatus::Loading;
  new_status.reset_timeout();
  new_status.loading_start_time_point_ = FileSystemTimePoint::clock::now();
  new_status.last_new_pages_time_point_ = std::chrono::steady_clock::now();
  library_to_status_map().add_overwrite(url, new_status);
}

void RemoteLibraryLoadingStatus::ping_still_loading(const StringRef url)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return;
  }

  if (this_->status_ == RemoteLibraryLoadingStatus::Loading) {
    this_->reset_timeout();
  }
}

void RemoteLibraryLoadingStatus::ping_new_pages(const StringRef url)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return;
  }

  if (this_->status_ == RemoteLibraryLoadingStatus::Loading) {
    this_->reset_timeout();
    this_->last_new_pages_time_point_ = std::chrono::steady_clock::now();
  }
}

void RemoteLibraryLoadingStatus::ping_new_preview(const bContext &C,
                                                  const StringRef preview_full_filepath)
{
  ED_preview_online_download_finished(CTX_wm_manager(&C), preview_full_filepath);
}

void RemoteLibraryLoadingStatus::ping_new_assets(const bContext &C, const StringRef url)
{
  WM_msg_publish_remote_io(CTX_wm_message_bus(&C), url);

  /* Redraw drags, they may show some "asset being downloaded" info. */
  const wmWindowManager *wm = CTX_wm_manager(&C);
  if (!BLI_listbase_is_empty(&wm->runtime->drags)) {
    WM_event_add_mousemove(CTX_wm_window(&C));
  }
}

void RemoteLibraryLoadingStatus::ping_metafiles_in_place(const StringRef url)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return;
  }

  this_->metafiles_in_place_ = true;
}

std::optional<RemoteLibraryLoadingStatus::Status> RemoteLibraryLoadingStatus::status(
    const StringRef url)
{
  const RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return {};
  }

  return this_->status_;
}

std::optional<bool> RemoteLibraryLoadingStatus::metafiles_in_place(const StringRef url)
{
  const RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return {};
  }

  return this_->metafiles_in_place_;
}

std::optional<RemoteLibraryLoadingStatus::FileSystemTimePoint> RemoteLibraryLoadingStatus::
    loading_start_time(const StringRef url)
{
  const RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return {};
  }
  if (this_->status_ != Loading) {
    return {};
  }

  return this_->loading_start_time_point_;
}

std::optional<RemoteLibraryLoadingStatus::TimePoint> RemoteLibraryLoadingStatus::
    last_new_pages_time(const StringRef url)
{
  const RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return {};
  }

  return this_->last_new_pages_time_point_;
}

void RemoteLibraryLoadingStatus::set_finished(const StringRef url)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return;
  }

  if (this_->status_ == RemoteLibraryLoadingStatus::Loading) {
    this_->status_ = RemoteLibraryLoadingStatus::Finished;
    this_->reset_timeout();
  }
}

void RemoteLibraryLoadingStatus::set_failure(const StringRef url,
                                             const std::optional<StringRefNull> failure_message)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return;
  }

  if (this_->status_ == RemoteLibraryLoadingStatus::Loading) {
    this_->status_ = RemoteLibraryLoadingStatus::Failure;
    this_->failure_message_ = failure_message;
    this_->reset_timeout();
  }
}

std::optional<StringRefNull> RemoteLibraryLoadingStatus::failure_message(const StringRef url)
{
  const RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return {};
  }

  if (this_->status_ == RemoteLibraryLoadingStatus::Failure) {
    return this_->failure_message_;
  }
  return {};
}

bool RemoteLibraryLoadingStatus::handle_timeout(const StringRef url)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_ || this_->status_ != RemoteLibraryLoadingStatus::Loading) {
    /* Only handle timeouts while loading. */
    return false;
  }

  const TimePoint now = std::chrono::steady_clock::now();
  /* Keep track of how long ago the timeout was checked last. This is to avoid blocking processes
   * from interfering with the timeout handling. If the timeout wasn't checked for a longer period
   * of time (more than `0.9 * timeout_`), we skip timeout handling. */
  if ((now - this_->last_timeout_handled_time_point_).count() > (0.9f * this_->timeout_)) {
    return false;
  }
  this_->last_timeout_handled_time_point_ = now;

  const std::chrono::duration<float> elapsed = now - this_->last_updated_time_point_;
  if (elapsed.count() < this_->timeout_) {
    return false;
  }

  this_->status_ = RemoteLibraryLoadingStatus::Failure;
  this_->failure_message_ = RPT_("Asset system lost connection to downloader (timed out).");
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Download Requests
 * \{ */

void remote_library_request_download(const bUserAssetLibrary &library_definition)
{
  BLI_assert(library_definition.flag & ASSET_LIBRARY_USE_REMOTE_URL);
  BLI_assert_msg(BLI_thread_is_main(), "Calling into Python from a thread is not safe");
  /* Ensure we don't attempt to download anything when online access is disabled. */
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    return;
  }

  if (!library_definition.remote_url[0]) {
    return;
  }

#ifdef WITH_PYTHON
  /* Remote library is already downloading. */
  if (RemoteLibraryLoadingStatus::status(library_definition.remote_url) ==
      RemoteLibraryLoadingStatus::Loading)
  {
    return;
  }

  /* Returns true if the directory exists, also if it pre-existed. */
  if (!BLI_dir_create_recursive(library_definition.dirpath)) {
    return;
  }

  {
    std::string script =
        "import bl_pkg\n"
        "from pathlib import Path\n"
        "\n"
        "bl_pkg.remote_asset_library_sync(\n"
        "    library_url, Path(library_path),\n"
        ")\n";

    std::unique_ptr locals = bke::idprop::create_group("locals");
    IDP_AddToGroup(locals.get(), IDP_NewString(library_definition.remote_url, "library_url"));
    IDP_AddToGroup(locals.get(), IDP_NewString(library_definition.dirpath, "library_path"));

    /* TODO: report errors in the UI somehow. */
    BPY_run_string_exec_with_locals(nullptr, script, *locals);
  }
#endif
}

#ifdef WITH_PYTHON
/**
 * Download a single asset file.
 * \returns an 'ok' flag. If not ok, a report will be added to the report list.
 */
static bool remote_library_request_asset_download_file(const bContext &C,
                                                       ReportList *reports,
                                                       const StringRefNull asset_name,
                                                       const asset_system::AssetLibrary &library,
                                                       const StringRefNull dst_filepath,
                                                       const URLWithHash &asset_url)
{
  BLI_assert(library.remote_url());

  if (dst_filepath.is_empty()) {
    BKE_reportf(
        reports,
        RPT_WARNING,
        "Asset listing does not indicate where the file should be downloaded to, for asset '%s'",
        asset_name.c_str());
    return false;
  }

  /* No need to check the URL. If it's empty, the Python code uses
   * `dst_filepath` as URL, relative to the asset library URL. */

  std::string script =
      "import _bpy_internal.assets.remote_library_listing.asset_downloader as asset_dl\n"
      "from pathlib import Path\n"
      "\n"
      "asset_dl.download_asset_file(\n"
      "    library_url, Path(library_path),\n"
      "    asset_url, asset_hash, Path(dst_filepath),\n"
      ")\n";

  std::unique_ptr locals = bke::idprop::create_group("locals");
  IDP_AddToGroup(locals.get(), IDP_NewString(*library.remote_url(), "library_url"));
  IDP_AddToGroup(locals.get(), IDP_NewString(library.root_path(), "library_path"));
  IDP_AddToGroup(locals.get(), IDP_NewString(dst_filepath, "dst_filepath"));
  IDP_AddToGroup(locals.get(), IDP_NewString(asset_url.url, "asset_url"));
  IDP_AddToGroup(locals.get(), IDP_NewString(asset_url.hash, "asset_hash"));

  /* TODO Casting away const is annoying. Could pass a context copy instead, but `BPY_run_`
   * functions don't handle that well yet. */
  return BPY_run_string_exec_with_locals(const_cast<bContext *>(&C), script, *locals);
}

#endif

void remote_library_request_asset_download(const bContext &C,
                                           const AssetRepresentation &asset,
                                           ReportList *reports)
{
  /* Ensure we don't attempt to download anything when online access is disabled. */
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    BKE_report(reports, RPT_ERROR, "Internet access is disabled");
    return;
  }

  if (!asset.is_online()) {
    BKE_report(reports, RPT_ERROR, "This is not an online asset and thus cannot be downloaded");
    return;
  }

#ifdef WITH_PYTHON
  const asset_system::AssetLibrary &library = asset.owner_asset_library();
  const std::optional<StringRefNull> library_url = library.remote_url();
  if (!library_url || library_url->is_empty()) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Could not find asset library URL for asset '%s'",
                asset.get_name().c_str());
    return;
  }

  /* The main file is listed first, and has to be downloaded last. By reversing the list of files,
   * first the dependencies are downloaded, followed by the asset itself. That way, when the main
   * asset file appears on disk, it is ready for use.
   *
   * NOTE: if in the future the downloader supports parallel downloads, this will break. In that
   * case, we'll have to move to something more "atomic", where all files that make up this asset
   * retain their temporary-because-I'm-being-downloaded name until all downloads are complete. */
  const Span<OnlineAssetFile> asset_files = asset.online_asset_files();
  const StringRefNull asset_name = asset.get_name();
  for (int i = asset_files.size() - 1; i >= 0; i--) {
    const OnlineAssetFile &asset_file = asset_files[i];
    const bool ok = remote_library_request_asset_download_file(
        C, reports, asset_name, library, asset_file.path, asset_file.url);
    if (!ok) {
      /* remote_library_request_asset_download_file() will have reported the error.
       *
       * Better to stop here, because if a dependency download couldn't be triggered, the main file
       * should not be downloaded either. Because, if that would work, we have a half-downloaded
       * asset that Blender's asset browser doesn't know is broken). */
      break;
    }
  }
#else
  UNUSED_VARS(C, asset);
  BKE_report(
      reports, RPT_ERROR, "Downloading assets requires Python, and this Blender is built without");
#endif
}

void remote_library_request_preview_download(const bContext &C,
                                             const AssetRepresentation &asset,
                                             const StringRef dst_filepath,
                                             ReportList *reports)
{
  /* Ensure we don't attempt to download anything when online access is disabled. */
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    BKE_report(reports, RPT_ERROR, "Internet access is disabled");
    return;
  }

#ifdef WITH_PYTHON
  const std::optional<StringRefNull> preview_url = asset.online_asset_preview_url();
  if (!preview_url) {
    return;
  }
  const std::optional<StringRefNull> preview_hash = asset.online_asset_preview_hash();
  if (!preview_hash) {
    return;
  }
  const asset_system::AssetLibrary &library = asset.owner_asset_library();
  const std::optional<StringRef> library_url = library.remote_url();
  if (!library_url) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Could not find asset library URL for asset '%s'",
                asset.get_name().c_str());
    return;
  }

  /* Notify the preview loading UI that a download for this preview is pending. */
  ED_preview_online_download_requested(dst_filepath);

  {
    std::string script =
        "import _bpy_internal.assets.remote_library_listing.asset_downloader as asset_dl\n"
        "from pathlib import Path\n"
        "\n"
        "asset_dl.download_preview(\n"
        "    library_url, Path(library_path),\n"
        "    preview_url, preview_hash, Path(dst_filepath),\n"
        ")\n";

    std::unique_ptr locals = bke::idprop::create_group("locals");
    IDP_AddToGroup(locals.get(), IDP_NewString(*library_url, "library_url"));
    IDP_AddToGroup(locals.get(), IDP_NewString(library.root_path(), "library_path"));
    IDP_AddToGroup(locals.get(), IDP_NewString(*preview_url, "preview_url"));
    IDP_AddToGroup(locals.get(), IDP_NewString(*preview_hash, "preview_hash"));
    IDP_AddToGroup(locals.get(), IDP_NewString(dst_filepath, "dst_filepath"));

    /* TODO Casting away const is annoying. Could pass a context copy instead, but `BPY_run_`
     * functions don't handle that well yet. */
    /* TODO: report errors in the UI somehow. */
    BPY_run_string_exec_with_locals(const_cast<bContext *>(&C), script, *locals);
  }

#else
  UNUSED_VARS(C, asset, dst_filepath);
  /* TODO should we use CLOG here? Otherwise every preview will trigger a report. */
  BKE_report(reports,
             RPT_ERROR,
             "Downloading asset previews requires Python, and this Blender is built without");
#endif
}

/** \} */

StringRefNull OnlineAssetInfo::asset_file() const
{
  if (this->files.is_empty()) {
    return {};
  }
  return this->files[0].path;
}

/* -------------------------------------------------------------------- */
/** \name Preview Images
 * \{ */

std::string remote_library_asset_preview_path(const AssetRepresentation &asset)
{
  const StringRefNull library_cache_dir = asset.owner_asset_library().root_path();

  char thumbs_dir_path[FILE_MAXDIR];
  BLI_path_join(
      thumbs_dir_path, sizeof(thumbs_dir_path), library_cache_dir.c_str(), "_thumbs", "large");

  const std::optional<StringRefNull> preview_url = asset.online_asset_preview_url();
  if (!preview_url) {
    BLI_assert_unreachable();
    return "";
  }

  const std::string asset_path = asset.full_path();

  char thumb_name[40];
  {
    char hexdigest[33];
    uchar digest[16];
    BLI_hash_md5_buffer(asset_path.data(), asset_path.size(), digest);
    BLI_hash_md5_to_hexdigest(digest, hexdigest);

    /* If the download URL has an extension, preserve that for the downloaded file (will be
     * either the period before the last extension, or the null character at the end of the file
     * name). */
    const char *ext = BLI_path_extension_or_end(preview_url->c_str());
    BLI_snprintf(thumb_name, sizeof(thumb_name), "%s%s", hexdigest, ext);
  }

  /* First two letters of the thumbnail name (MD5 hash of the URI) as sub-directory name. */
  char thumb_prefix[3];
  thumb_prefix[0] = thumb_name[0];
  thumb_prefix[1] = thumb_name[1];
  thumb_prefix[2] = '\0';

  /* Finally, the path of the thumbnail itself. */
  char thumb_path[FILE_MAX];
  BLI_path_join(thumb_path, sizeof(thumb_path), thumbs_dir_path, thumb_prefix, thumb_name + 2);

  return thumb_path;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Other Free Functions
 * \{ */

void foreach_registered_remote_library(FunctionRef<void(bUserAssetLibrary &)> fn)
{
  for (bUserAssetLibrary &library : U.asset_libraries) {
    if ((library.flag & ASSET_LIBRARY_USE_REMOTE_URL) && library.remote_url[0]) {
      fn(library);
    }
  }
}

/** \} */

}  // namespace blender::asset_system
