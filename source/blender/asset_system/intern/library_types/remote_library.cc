/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <filesystem>

#include "BLI_fileops.h"
#include "BLI_listbase.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

#include "DNA_userdef_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "AS_asset_representation.hh"
#include "AS_remote_library.hh"
#include "remote_library.hh"

namespace blender::asset_system {

RemoteAssetLibrary::RemoteAssetLibrary(const StringRef remote_url, StringRef cache_root_path)
    : AssetLibrary(ASSET_LIBRARY_CUSTOM, "", cache_root_path)
{
  import_method_ = ASSET_IMPORT_APPEND_REUSE;
  may_override_import_method_ = false;
  remote_url_ = remote_url;
}

std::optional<AssetLibraryReference> RemoteAssetLibrary::library_reference() const
{
  int i;
  LISTBASE_FOREACH_INDEX (const bUserAssetLibrary *, asset_library, &U.asset_libraries, i) {
    if ((asset_library->flag & ASSET_LIBRARY_USE_REMOTE_URL) == 0) {
      continue;
    }

    if (asset_library->remote_url == this->remote_url_) {
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
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return;
  }

  if (status->status_ == RemoteLibraryLoadingStatus::Loading) {
    status->reset_timeout();
  }
}

void RemoteLibraryLoadingStatus::ping_new_pages(const StringRef url)
{
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return;
  }

  if (status->status_ == RemoteLibraryLoadingStatus::Loading) {
    status->reset_timeout();
    status->last_new_pages_time_point_ = std::chrono::steady_clock::now();
  }
}

void RemoteLibraryLoadingStatus::ping_new_previews(const StringRef url)
{
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return;
  }

  if (status->status_ == RemoteLibraryLoadingStatus::Loading) {
    status->reset_timeout();
    status->last_new_previews_time_point_ = std::chrono::steady_clock::now();
  }
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
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return;
  }

  status->metafiles_in_place_ = true;
}

std::optional<RemoteLibraryLoadingStatus::Status> RemoteLibraryLoadingStatus::status(
    const StringRef url)
{
  const RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return {};
  }

  return status->status_;
}

std::optional<bool> RemoteLibraryLoadingStatus::metafiles_in_place(const StringRef url)
{
  const RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return {};
  }

  return status->metafiles_in_place_;
}

std::optional<RemoteLibraryLoadingStatus::FileSystemTimePoint> RemoteLibraryLoadingStatus::
    loading_start_time(const StringRef url)
{
  const RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return {};
  }
  if (status->status_ != Loading) {
    return {};
  }

  return status->loading_start_time_point_;
}

std::optional<RemoteLibraryLoadingStatus::TimePoint> RemoteLibraryLoadingStatus::
    last_new_pages_time(const StringRef url)
{
  const RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return {};
  }

  return status->last_new_pages_time_point_;
}

std::optional<RemoteLibraryLoadingStatus::TimePoint> RemoteLibraryLoadingStatus::
    last_new_previews_time(const StringRef url)
{
  const RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return {};
  }

  return status->last_new_previews_time_point_;
}

void RemoteLibraryLoadingStatus::set_finished(const StringRef url)
{
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return;
  }

  if (status->status_ == RemoteLibraryLoadingStatus::Loading) {
    status->status_ = RemoteLibraryLoadingStatus::Finished;
    status->reset_timeout();
  }
}

void RemoteLibraryLoadingStatus::set_failure(const StringRef url,
                                             const std::optional<StringRefNull> failure_message)
{
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return;
  }

  if (status->status_ == RemoteLibraryLoadingStatus::Loading) {
    status->status_ = RemoteLibraryLoadingStatus::Failure;
    status->failure_message_ = failure_message;
    status->reset_timeout();
  }
}

std::optional<StringRefNull> RemoteLibraryLoadingStatus::failure_message(const StringRef url)
{
  const RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status) {
    return {};
  }

  if (status->status_ == RemoteLibraryLoadingStatus::Failure) {
    return status->failure_message_;
  }
  return {};
}

bool RemoteLibraryLoadingStatus::handle_timeout(const StringRef url)
{
  RemoteLibraryLoadingStatus *status = library_to_status_map().lookup_ptr(url);
  if (!status || status->status_ != RemoteLibraryLoadingStatus::Loading) {
    /* Only handle timeouts while loading. */
    return false;
  }

  const TimePoint now = std::chrono::steady_clock::now();
  /* Keep track of how long ago the timeout was checked last. This is to avoid blocking processes
   * from interfering with the timeout handling. If the timeout wasn't checked for a longer period
   * of time (more than `0.9 * timeout_`), we skip timeout handling. */
  if ((now - status->last_timeout_handled_time_point_).count() > (0.9f * status->timeout_)) {
    return false;
  }
  status->last_timeout_handled_time_point_ = now;

  const std::chrono::duration<float> elapsed = now - status->last_updated_time_point_;
  if (elapsed.count() < status->timeout_) {
    return false;
  }

  status->status_ = RemoteLibraryLoadingStatus::Failure;
  status->failure_message_ = RPT_("Asset system lost connection to downloader (timed out).");
  return true;
}

/** \} */

void remote_library_request_download(Main &bmain, bUserAssetLibrary &library_definition)
{
  BLI_assert(library_definition.flag & ASSET_LIBRARY_USE_REMOTE_URL);
  /* Ensure we don't attempt to download anything when online access is disabled. */
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    return;
  }

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

  PointerRNA lib_ptr = RNA_pointer_create_discrete(
      nullptr, &RNA_UserAssetLibrary, &library_definition);
  PointerRNA *lib_ptr_arr[] = {&lib_ptr};
  BKE_callback_exec(&bmain, lib_ptr_arr, 1, BKE_CB_EVT_REMOTE_ASSET_LIBRARIES_SYNC);
}

void remote_library_request_asset_download(bContext &C,
                                           const AssetRepresentation &asset,
                                           ReportList *reports)
{
  /* Ensure we don't attempt to download anything when online access is disabled. */
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    BKE_report(reports, RPT_ERROR, "Internet access is disabled");
    return;
  }

#ifdef WITH_PYTHON
  const std::optional<StringRef> dst_filepath = asset.download_dst_filepath();
  if (!dst_filepath) {
    return;
  }
  const asset_system::AssetLibrary &library = asset.owner_asset_library();
  const std::optional<StringRefNull> library_url = library.remote_url();
  if (!library_url) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Could not find asset library URL for asset '%s'",
                asset.get_name().c_str());
    return;
  }

  {
    std::string script =
        "import _bpy_internal.assets.remote_library_listing.asset_downloader as asset_dl\n"
        "from pathlib import Path\n"
        "\n"
        "asset_dl.download_asset(\n"
        "    library_url, Path(library_path),\n"
        "    dst_filepath, Path(dst_filepath),\n"
        ")\n";

    /* Construct local variables for the above script. */
    std::unique_ptr locals = bke::idprop::create_group("locals");
    IDP_AddToGroup(locals.get(), IDP_NewString(*library_url, "library_url"));
    IDP_AddToGroup(locals.get(), IDP_NewString(library.root_path(), "library_path"));
    IDP_AddToGroup(locals.get(), IDP_NewString(*dst_filepath, "dst_filepath"));

    /* TODO: report errors in the UI somehow. */
    BPY_run_string_with_locals(&C, script, *locals);
  }
#else
  UNUSED_VARS(C, asset);
  BKE_report(
      reports, RPT_ERROR, "Downloading assets requires Python, and this Blender is built without");
#endif
}

}  // namespace blender::asset_system
