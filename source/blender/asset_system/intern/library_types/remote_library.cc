/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <fmt/format.h>

#include "BLI_assert.h"
#include "BLI_fileops.h"
#include "BLI_hash_md5.hh"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_threads.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_report.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

#include "CLG_log.h"

#include "DNA_asset_types.h"
#include "DNA_space_enums.h"
#include "DNA_userdef_types.h"

#include "ED_asset.hh"
#include "ED_fileselect.hh"
#include "ED_render.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "AS_asset_representation.hh"
#include "AS_essentials_library.hh"
#include "AS_remote_library.hh"
#include "remote_library.hh"

static CLG_LogRef LOG = {"assets.remote_library"};

namespace blender::asset_system {

RemoteLibraryDefinitionRef::RemoteLibraryDefinitionRef(const bUserAssetLibrary &library_definition)
    : remote_url(library_definition.remote_url), cache_dirpath(library_definition.dirpath)
{
  BLI_assert((library_definition.flag & ASSET_LIBRARY_USE_REMOTE_URL) != 0);
}

/* -------------------------------------------------------------------- */
/** \name Remote Library Base Class
 *
 *  Used by #PreferencesRemoteAssetLibrary and #OnlineEssentialsLibrary.
 * \{ */

RemoteAssetLibrary::RemoteAssetLibrary(const eAssetLibraryType library_type,
                                       const bool is_read_only,
                                       const StringRef remote_url,
                                       const StringRef name,
                                       const StringRef root_path)
    : AssetLibrary(library_type, is_read_only, name, root_path), remote_url_(remote_url)
{
  may_override_import_method_ = false;
}

std::optional<eAssetImportMethod> RemoteAssetLibrary::import_method() const
{
  if (U.experimental.no_data_block_packing) {
    return ASSET_IMPORT_APPEND_REUSE;
  }
  return ASSET_IMPORT_PACK;
}

std::optional<StringRefNull> RemoteAssetLibrary::remote_url() const
{
  return remote_url_;
}

void RemoteAssetLibrary::refresh_catalogs()
{
  this->catalog_service().reload_catalogs();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preferences Remote Library
 * \{ */

PreferencesRemoteAssetLibrary::PreferencesRemoteAssetLibrary(
    const bUserAssetLibrary &custom_library)
    : RemoteAssetLibrary(ASSET_LIBRARY_CUSTOM,
                         /*is_read_only=*/true,
                         /*remote_url=*/custom_library.remote_url,
                         /*name=*/custom_library.name,
                         /*root_path=*/custom_library.dirpath),
      user_library_(custom_library)
{
  BLI_assert(custom_library.flag & ASSET_LIBRARY_USE_REMOTE_URL);
}

std::optional<AssetLibraryReference> PreferencesRemoteAssetLibrary::library_reference() const
{
  const bUserAssetLibrary *library_definition = user_library_.user_asset_library();
  if (library_definition == nullptr) {
    return {};
  }

  const int index = BLI_findindex(&U.asset_libraries, library_definition);
  if (index == -1) {
    /* Should have been caught by the #user_asset_library() call above already. */
    BLI_assert_unreachable();
    return {};
  }

  BLI_assert(library_definition->flag & ASSET_LIBRARY_USE_REMOTE_URL);
  AssetLibraryReference library_ref{};
  library_ref.type = ASSET_LIBRARY_CUSTOM;
  library_ref.custom_library_index = index;
  return library_ref;
}

bool PreferencesRemoteAssetLibrary::is_enabled() const
{
  const bUserAssetLibrary *library_definition = user_library_.user_asset_library();
  if (!library_definition) {
    return false;
  }

  return (library_definition->flag & ASSET_LIBRARY_DISABLED) == 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Progress Tracking
 * \{ */

/** See #ProgressTracker::req_to_full_urls. */
struct RequestIdentifier {
  std::string library_url;
  std::string file_request_url;

  uint64_t hash() const
  {
    return get_default_hash(library_url, file_request_url);
  }
  bool operator==(const RequestIdentifier &other) const
  {
    return library_url == other.library_url && file_request_url == other.file_request_url;
  }
};

struct FileProgress {
  int64_t expected_size_in_bytes;
  int64_t current_size_in_bytes = 0;
};

struct ProgressData {
  bool any_asset_file_loading;

  /**
   * Assets stored in the asset system may only contain a relative URL. The downloader turns this
   * into an absolute URL, and only uses this internally. So all progress reporting uses the
   * absolute URL, while the ones known to the asset system may be relative. The downloader returns
   * the absolute URL in its download function, which we can obtain from the return value of
   * #remote_library_request_asset_download_file().
   *
   * This maps the asset library URL and the potentially relative asset file URL to the absolute
   * URL used by the downloader.
   */
  Map<RequestIdentifier, std::string> req_to_full_urls;

  /** Absolute URLs (see #req_to_full_urls) of all requested files mapped to their expected size on
   * disk. Files that are done downloading (successfully or not) are removed and added to
   * #done_files below. */
  Map<std::string, FileProgress> requested_files;
  /** Absolute URLs of files that are done downloading (successfully or not) mapped to their
   * expected size on disk. Will be cleared once all current requests are done. This way total
   * progress reporting can include "done" assets, and progress bars fill up as expected. */
  Map<std::string, FileProgress> done_files;
};

enum class DownloadOutcome {
  Succeeded,
  Failed,
};

struct ProgressTracker {
  static ProgressData current;

  static wmTimer *notification_timer;

  /** Should be called when a file download is requested. */
  static void file_requested(wmWindowManager &wm,
                             RequestIdentifier &&request,
                             std::string &&abs_url,
                             int64_t size_in_bytes);
  static void file_report_progress(StringRef absolute_file_url, int64_t size_in_bytes);
  /** Should be called when a file download is finished, successfully or not. */
  static void file_finished(StringRef absolute_file_url, DownloadOutcome outcome);

  /** Should be called when all downloads finished, successfully or not. */
  static void on_all_finished(wmWindowManager &wm);

  /**
   * Returns true if any asset files are currently downloading. This information is taken from the
   * downloader every time a file is finished. So we don't rely on keeping track of all in-flight
   * downloads ourselves.
   */
  static bool is_any_loading();
};

ProgressData ProgressTracker::current = {};
wmTimer *ProgressTracker::notification_timer = nullptr;

void ProgressTracker::file_requested(wmWindowManager &wm,
                                     RequestIdentifier &&request,
                                     std::string &&abs_url,
                                     const int64_t size_in_bytes)
{
  ProgressTracker::current.requested_files.add(
      abs_url, FileProgress{.expected_size_in_bytes = size_in_bytes});
  /* Make the absolute URL known to the progress reporting, so we can query it later using the
   * potentially relative URL that is known to the asset system. */
  ProgressTracker::current.req_to_full_urls.add(std::move(request), std::move(abs_url));

  ProgressTracker::current.any_asset_file_loading = true;

  if (!ProgressTracker::notification_timer) {
    ProgressTracker::notification_timer = WM_event_timer_add_notifier(
        &wm, nullptr, NC_WM | ND_JOB, 0.1);
  }
}

void ProgressTracker::file_report_progress(const StringRef absolute_file_url,
                                           const int64_t size_in_bytes)
{
  if (FileProgress *progress = ProgressTracker::current.requested_files.lookup_ptr_as(
          absolute_file_url))
  {
    progress->current_size_in_bytes = size_in_bytes;
  }
}

void ProgressTracker::file_finished(const StringRef absolute_file_url,
                                    const DownloadOutcome outcome)
{
  if (FileProgress *progress = ProgressTracker::current.requested_files.lookup_ptr_as(
          absolute_file_url))
  {
    switch (outcome) {
      case DownloadOutcome::Failed:
        /* The file is 'done', but shouldn't count towards any download progress any more. */
        break;
      case DownloadOutcome::Succeeded:
        ProgressTracker::current.done_files.add(absolute_file_url, *progress);
        break;
    }

    /* Regardless of the outcome, the file is no longer downloading. */
    ProgressTracker::current.requested_files.remove_contained_as(absolute_file_url);
  }
}

void ProgressTracker::on_all_finished(wmWindowManager &wm)
{
  /* Clear all progress data. */
  ProgressTracker::current = {};
  if (ProgressTracker::notification_timer) {
    WM_event_timer_remove(&wm, nullptr, ProgressTracker::notification_timer);
    ProgressTracker::notification_timer = nullptr;
  }
  /* Add notifier so job UIs redraw, and the progress/cancel buttons disappear. */
  WM_event_add_notifier_ex(&wm, nullptr, NC_WM | ND_JOB, nullptr);
}

bool ProgressTracker::is_any_loading()
{
  return ProgressTracker::current.any_asset_file_loading;
}

float remote_library_total_asset_downloads_progress()
{
  int64_t expected_bytes = 0;
  int64_t current_bytes = 0;
  for (const FileProgress &progress : ProgressTracker::current.requested_files.values()) {
    expected_bytes += progress.expected_size_in_bytes;
    current_bytes += progress.current_size_in_bytes;
  }

  for (const FileProgress &finished : ProgressTracker::current.done_files.values()) {
    expected_bytes += finished.expected_size_in_bytes;
    current_bytes += finished.expected_size_in_bytes;
  }

  if (!expected_bytes) {
    return 0.0f;
  }

  const float progress = float(current_bytes) / expected_bytes;
  BLI_assert(progress >= -0.0001f && progress <= 1.0001f);
  return std::clamp(progress, 0.0f, 1.0f);
}

bool remote_library_has_unfinished_asset_downloads()
{
  return ProgressTracker::is_any_loading();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remote Library Loading Status
 * \{ */

/*
 * NOTE: Some of the status setters here only modify status if the current status is
 * #RemoteLibraryLoadingStatus::Loading. That is done to avoid status changes after loading ended,
 * mostly after a timeout. E.g. after a timeout of the C++ status because Python didn't send status
 * updates, Python might eventually resume sending updates. These shouldn't affect the C++ status
 * anymore, since the earlier failure aborted the C++ side asset library loading. Plus, the UI
 * might show an error, and would suddenly switch to showing an incompletely loaded asset library.
 */

using UrlToLibraryStatusMap = Map<std::string /*url*/, asset_system::RemoteLibraryLoadingStatus>;

static UrlToLibraryStatusMap &library_to_status_map()
{
  static UrlToLibraryStatusMap map = UrlToLibraryStatusMap{};
  BLI_assert_msg(
      BLI_thread_is_main(),
      "Remote library status isn't synchronized and should only be accessed from the main thread");
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

void RemoteLibraryLoadingStatus::ping_asset_file_progress(const StringRef absolute_file_url,
                                                          const int64_t size_in_bytes)
{
  ProgressTracker::file_report_progress(absolute_file_url, size_in_bytes);
}

static void ping_asset_file_done_impl(const bContext &C,
                                      const StringRef library_url,
                                      const StringRef absolute_file_url,
                                      const StringRef local_file_abspath,
                                      const DownloadOutcome outcome)
{
  wmWindowManager *wm = CTX_wm_manager(&C);

  ed::asset::list::on_remote_assets_downloaded(*wm, library_url, local_file_abspath);
  ProgressTracker::file_finished(absolute_file_url, outcome);

  /* Redraw drags, they may show some "asset being downloaded" info. */
  if (!wm->runtime->drags.is_empty()) {
    WM_event_add_mousemove(CTX_wm_window(&C));
  }
}

void RemoteLibraryLoadingStatus::ping_asset_file_download_succeeded(
    const bContext &C,
    const StringRef library_url,
    const StringRef absolute_file_url,
    const StringRef local_file_abspath)
{
  ping_asset_file_done_impl(
      C, library_url, absolute_file_url, local_file_abspath, DownloadOutcome::Succeeded);
}

void RemoteLibraryLoadingStatus::ping_asset_file_download_failed(
    const bContext &C,
    const StringRef library_url,
    const StringRef absolute_file_url,
    const StringRef local_file_abspath)
{
  ping_asset_file_done_impl(
      C, library_url, absolute_file_url, local_file_abspath, DownloadOutcome::Failed);
}

void RemoteLibraryLoadingStatus::ping_download_queue_done(const bContext &C)
{
  ProgressTracker::on_all_finished(*CTX_wm_manager(&C));
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

void RemoteLibraryLoadingStatus::set_cancelled(const StringRef url)
{
  RemoteLibraryLoadingStatus *this_ = library_to_status_map().lookup_ptr(url);
  if (!this_) {
    return;
  }

  if (this_->status_ == RemoteLibraryLoadingStatus::Loading) {
    this_->status_ = RemoteLibraryLoadingStatus::Cancelled;
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

void remote_library_request_download(const RemoteLibraryDefinitionRef &library_definition)
{
  BLI_assert_msg(BLI_thread_is_main(), "Calling into Python from a thread is not safe");
  /* Ensure we don't attempt to download anything when online access is disabled. */
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
    return;
  }

  if (!library_definition.remote_url[0]) {
    return;
  }

  BLI_assert_msg(!is_online_essentials_url(library_definition.remote_url) ||
                     library_definition.cache_dirpath == online_essentials_cache_directory_path(),
                 "The online essentials library must be downloaded to "
                 "online_essentials_cache_directory_path()");

#ifdef WITH_PYTHON
  /* Remote library is already downloading. */
  if (RemoteLibraryLoadingStatus::status(library_definition.remote_url) ==
      RemoteLibraryLoadingStatus::Loading)
  {
    return;
  }

  /* Returns true if the directory exists, also if it pre-existed. */
  if (!BLI_dir_create_recursive(library_definition.cache_dirpath.c_str())) {
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
    IDP_AddToGroup(locals.get(), IDP_NewString(library_definition.cache_dirpath, "library_path"));

    /* TODO: report errors in the UI somehow. */
    BPY_run_string_exec_with_locals(nullptr, script, *locals);
  }
#endif
}

void remote_library_cancel_all_listing_downloads(const bContext &C)
{
#ifdef WITH_PYTHON
  constexpr const char *SCRIPT = R"(
import bl_pkg

bl_pkg.remote_asset_library_sync_cancel()
  )";

  std::unique_ptr locals = bke::idprop::create_group("locals");
  BPY_run_string_exec_with_locals(const_cast<bContext *>(&C), SCRIPT, *locals);

  for (StringRef remote_url : library_to_status_map().keys()) {
    RemoteLibraryLoadingStatus::set_cancelled(remote_url);
  }
#else
  UNUSED_VARS(C);
#endif
}

#ifdef WITH_PYTHON
/**
 * Download a single asset file.
 * \returns The absolute URL determined and returned by the downloader. In case of error, this will
 *          be unset and a report will be added to the report list.
 */
static std::optional<std::string> remote_library_request_asset_download_file(
    const bContext &C,
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
    return std::nullopt;
  }

  /* Protect against maliciously constructed file paths. This code can just check & reject, as the
   * actual sanitisation happens when the listing is downloaded (see `listing_downloader.py`). */
  if (BLI_path_is_abs_from_cwd(dst_filepath.c_str())) {
    /* Absolute file paths. */
    BKE_reportf(reports,
                RPT_ERROR,
                "Asset '%s' references a file with an absolute path, which is not allowed",
                asset_name.c_str());
    return std::nullopt;
  }

  /* Check '..' entries, which can be "../" at the start of the path, or "/../" in the middle of
   * the path. */
  std::string path_native(dst_filepath);
  BLI_path_slash_native(path_native.data());

  static constexpr char slash_dot_dot_slash[]{SEP, '.', '.', SEP, '\0'};
  static constexpr char const *dot_dot_slash = slash_dot_dot_slash + 1;

  if (path_native.starts_with(dot_dot_slash) ||
      path_native.find(slash_dot_dot_slash) != std::string::npos)
  {
    BKE_reportf(reports,
                RPT_ERROR,
                "Asset '%s' references a file with '..' in its path, which is not allowed",
                asset_name.c_str());
    return std::nullopt;
  }

  /* No need to check the URL. If it's empty, the Python code uses
   * `dst_filepath` as URL, relative to the asset library URL. */

  std::string script =
      "import _bpy_internal.assets.remote_library.asset_downloader as asset_dl\n"
      "from pathlib import Path\n"
      "\n"
      "_result = asset_dl.download_asset_file(\n"
      "    library_url, Path(library_path),\n"
      "    asset_url, asset_hash, Path(dst_filepath),\n"
      ")\n";

  std::unique_ptr locals = bke::idprop::create_group("locals");
  IDP_AddToGroup(locals.get(), IDP_NewString(*library.remote_url(), "library_url"));
  IDP_AddToGroup(locals.get(), IDP_NewString(library.root_path(), "library_path"));
  IDP_AddToGroup(locals.get(), IDP_NewString(dst_filepath, "dst_filepath"));
  IDP_AddToGroup(locals.get(), IDP_NewString(asset_url.url, "asset_url"));
  IDP_AddToGroup(locals.get(), IDP_NewString(asset_url.hash, "asset_hash"));

  std::optional<IDProperty *> abs_url_idptr =
      /* TODO Casting away const is annoying. Could pass a context copy instead, but `BPY_run_`
       * functions don't handle that well yet. */
      BPY_run_string_exec_with_locals_return_idprop(
          const_cast<bContext *>(&C), script, *locals, "_result");

  if (!abs_url_idptr) {
    CLOG_ERROR(&LOG, "Failed to retrieve URL from downloader - bug in Python script");
    BLI_assert_unreachable();
    return std::nullopt;
  }
  BLI_SCOPED_DEFER([&] { IDP_FreeProperty(*abs_url_idptr); });
  if (!*abs_url_idptr) {
    CLOG_ERROR(&LOG, "Failed to retrieve URL from downloader");
    return std::nullopt;
  }
  if ((*abs_url_idptr)->type != IDP_STRING) {
    CLOG_ERROR(&LOG, "Failed to retrieve URL from downloader - expected string return value");
    return std::nullopt;
  }

  const std::string abs_url(IDP_string_get(*abs_url_idptr));
  return abs_url;
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

  if (!asset.needs_download()) {
    BKE_report(reports, RPT_ERROR, "This asset does not require downloading");
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

  wmWindowManager *wm = CTX_wm_manager(&C);

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
    std::optional<std::string> abs_url = remote_library_request_asset_download_file(
        C, reports, asset_name, library, asset_file.path, asset_file.url);
    if (!abs_url) {
      /* remote_library_request_asset_download_file() will have reported the error.
       *
       * Better to stop here, because if a dependency download couldn't be triggered, the main file
       * should not be downloaded either. Because, if that would work, we have a half-downloaded
       * asset that Blender's asset browser doesn't know is broken. */
      break;
    }

    ProgressTracker::file_requested(*wm,
                                    RequestIdentifier{*library_url, asset_file.url.url},
                                    std::move(*abs_url),
                                    asset_file.size_in_bytes);
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
        "import _bpy_internal.assets.remote_library.asset_downloader as asset_dl\n"
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

/* -------------------------------------------------------------------- */
/** \name Download Cancelling
 * \{ */

void remote_library_cancel_all_asset_downloads(bContext &C)
{
#ifdef WITH_PYTHON
  constexpr const char *SCRIPT = R"(
import _bpy_internal.assets.remote_library.asset_downloader as asset_dl

asset_dl.cancel_download_all_assets()
  )";

  std::unique_ptr locals = bke::idprop::create_group("locals");
  BPY_run_string_exec_with_locals(&C, SCRIPT, *locals);
#else
  UNUSED_VARS(C);
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
/** \name Cache Paths
 * \{ */

/**
 * Maximum length of the remote library directory name. Kept short to avoid path length issues with
 * deeply nested asset libraries.
 *
 * The directory name will be the MD5 hash of the URL.
 */
const int8_t REMOTE_LIBRARY_DIRNAME_LEN = 16;

std::string remote_library_cache_directory_path_from_url(const StringRef remote_url)
{
  BLI_assert_msg(
      remote_url != online_essentials_url(),
      "Online Essentials URL should use asset_system::online_essentials_cache_directory_path()");

  char library_identifier[REMOTE_LIBRARY_DIRNAME_LEN + 1];
  {
    /* MD5 hash part. */
    uchar digest[16];
    BLI_hash_md5_buffer(remote_url.data(), remote_url.size(), digest);
    char hex_digest[33];
    BLI_hash_md5_to_hexdigest(digest, hex_digest);
    /* This adds a null terminator. */
    BLI_strncpy(library_identifier, hex_digest, REMOTE_LIBRARY_DIRNAME_LEN + 1);
  }

  return remote_library_cache_directory_path(library_identifier);
}

std::string remote_library_cache_directory_path(const StringRefNull library_dirname)
{
  char cache_path[FILE_MAXDIR];
  BKE_appdir_folder_caches(cache_path, sizeof(cache_path));

  char library_cache_path[FILE_MAXDIR];
  BLI_path_join(library_cache_path,
                sizeof(library_cache_path),
                cache_path,
                "remote-assets",
                library_dirname.c_str());

  return library_cache_path;
}

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
    SNPRINTF(thumb_name, "%s%s", hexdigest, ext);
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

bool remote_library_url_ends_with_top_meta_file_name(const StringRef url)
{
  if (url.size() < REMOTE_LIBRARY_TOP_META_FILE_NAME_LEADING_SLASH.size()) {
    return false;
  }
  return url.endswith(REMOTE_LIBRARY_TOP_META_FILE_NAME_LEADING_SLASH);
}

void foreach_registered_user_remote_library(FunctionRef<void(bUserAssetLibrary &)> fn)
{
  for (bUserAssetLibrary &library : U.asset_libraries) {
    if ((library.flag & ASSET_LIBRARY_USE_REMOTE_URL) && library.remote_url[0]) {
      fn(library);
    }
  }
}

/** \} */

}  // namespace blender::asset_system
