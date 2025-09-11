/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Abstractions to manage runtime asset lists with a global cache for multiple UI elements to
 * access.
 * Internally this uses the #FileList API and structures from `filelist.cc`.
 * This is just because it contains most necessary logic already and
 * there's not much time for a more long-term solution.
 */

#include <optional>
#include <string>

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_utility_mixins.hh"

#include "DNA_space_types.h"

#include "WM_api.hh"

/* XXX uses private header of file-space. */
#include "../space_file/file_indexer.hh"
#include "../space_file/filelist.hh"

#include "ED_asset_indexer.hh"
#include "ED_asset_list.hh"
#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "asset_library_reference.hh"

namespace blender::ed::asset::list {

/* -------------------------------------------------------------------- */
/** \name Asset list API
 *
 * Internally re-uses #FileList from the File Browser. It does all the heavy lifting already.
 * \{ */

/**
 * RAII wrapper for `FileList`
 */
class FileListWrapper {
  static void filelist_free_fn(FileList *list)
  {
    filelist_free(list);
  }

  std::unique_ptr<FileList, decltype(&filelist_free_fn)> file_list_;

 public:
  explicit FileListWrapper(eFileSelectType filesel_type)
      : file_list_(filelist_new(filesel_type), filelist_free_fn)
  {
  }
  FileListWrapper(FileListWrapper &&other) = default;
  FileListWrapper &operator=(FileListWrapper &&other) = default;
  ~FileListWrapper()
  {
    /* Destructs the owned pointer. */
    file_list_ = nullptr;
  }

  operator FileList *() const
  {
    return file_list_.get();
  }
};

class AssetList : NonCopyable {
  FileListWrapper filelist_;
  AssetLibraryReference library_ref_;

 public:
  AssetList() = delete;
  AssetList(eFileSelectType filesel_type, const AssetLibraryReference &asset_library_ref);
  AssetList(AssetList &&other) = default;
  ~AssetList() = default;

  static bool listen(const wmNotifier &notifier);

  void setup();
  void fetch(const bContext &C);
  void ensure_blocking(const bContext &C);
  void clear(wmWindowManager *wm);
  void clear_current_file_assets(wmWindowManager *wm);

  bool needs_refetch() const;
  bool is_loaded() const;
  asset_system::AssetLibrary *asset_library() const;
  void iterate(AssetListIterFn fn) const;
  int size() const;
  void tag_main_data_dirty() const;
  void remap_id(ID *id_old, ID *id_new) const;
};

AssetList::AssetList(eFileSelectType filesel_type, const AssetLibraryReference &asset_library_ref)
    : filelist_(filesel_type), library_ref_(asset_library_ref)
{
}

void AssetList::setup()
{
  FileList *files = filelist_;
  std::string asset_lib_path = AS_asset_library_root_path_from_library_ref(library_ref_);

  /* Relevant bits from file_refresh(). */
  /* TODO pass options properly. */
  filelist_setrecursion(files, FILE_SELECT_MAX_RECURSIONS);
  filelist_setsorting(files, FILE_SORT_ASSET_CATALOG, false);
  filelist_setlibrary(files, &library_ref_);
  filelist_setfilter_options(
      files,
      true,
      true,
      true, /* Just always hide parent, prefer to not add an extra user option for this. */
      FILE_TYPE_BLENDERLIB,
      FILTER_ID_ALL,
      true,
      "",
      "");

  const bool use_asset_indexer = !USER_DEVELOPER_TOOL_TEST(&U, no_asset_indexing);
  filelist_setindexer(files, use_asset_indexer ? &index::file_indexer_asset : &file_indexer_noop);

  char dirpath[FILE_MAX_LIBEXTRA] = "";
  if (!asset_lib_path.empty()) {
    STRNCPY(dirpath, asset_lib_path.c_str());
  }
  filelist_setdir(files, dirpath);
}

void AssetList::fetch(const bContext &C)
{
  FileList *files = filelist_;

  if (filelist_needs_force_reset(files)) {
    filelist_readjob_stop(files, CTX_wm_manager(&C));
    filelist_clear_from_reset_tag(files);
  }

  if (filelist_needs_reading(files)) {
    if (!filelist_pending(files)) {
      filelist_readjob_start(files, NC_ASSET | ND_ASSET_LIST_READING, &C);
    }
  }
  filelist_sort(files);
  filelist_filter(files);
}

void AssetList::ensure_blocking(const bContext &C)
{
  FileList *files = filelist_;

  if (filelist_needs_force_reset(files)) {
    filelist_clear_from_reset_tag(files);
  }

  if (filelist_needs_reading(files)) {
    filelist_readjob_blocking_run(files, NC_ASSET | ND_ASSET_LIST_READING, &C);
  }

  filelist_sort(files);
  filelist_filter(files);
}

bool AssetList::needs_refetch() const
{
  return filelist_needs_force_reset(filelist_) || filelist_needs_reading(filelist_);
}

bool AssetList::is_loaded() const
{
  return filelist_is_ready(filelist_);
}

asset_system::AssetLibrary *AssetList::asset_library() const
{
  return reinterpret_cast<asset_system::AssetLibrary *>(filelist_asset_library(filelist_));
}

void AssetList::iterate(AssetListIterFn fn) const
{
  FileList *files = filelist_;
  const int numfiles = filelist_files_ensure(files);

  for (int i = 0; i < numfiles; i++) {
    asset_system::AssetRepresentation *asset = filelist_entry_get_asset_representation(files, i);
    if (!asset) {
      continue;
    }

    if (!fn(*asset)) {
      break;
    }
  }
}

void AssetList::clear(wmWindowManager *wm)
{
  /* Based on #ED_fileselect_clear() */

  FileList *files = filelist_;
  filelist_readjob_stop(files, wm);
  filelist_freelib(files);
  filelist_clear(files);
  filelist_tag_force_reset(files);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST, nullptr);
}

void AssetList::clear_current_file_assets(wmWindowManager *wm)
{
  /* Based on #ED_fileselect_clear_main_assets() */

  FileList *files = filelist_;
  filelist_readjob_stop(files, wm);
  filelist_freelib(files);
  filelist_tag_force_reset_mainfiles(files);
  filelist_tag_reload_asset_library(files);
  filelist_clear_from_reset_tag(files);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST, nullptr);
}

/**
 * \return True if the asset-list needs a UI redraw.
 */
bool AssetList::listen(const wmNotifier &notifier)
{
  switch (notifier.category) {
    case NC_ID: {
      if (ELEM(notifier.action, NA_RENAME)) {
        return true;
      }
      break;
    }
    case NC_ASSET:
      if (ELEM(notifier.data, ND_ASSET_LIST, ND_ASSET_LIST_READING, ND_ASSET_LIST_PREVIEW)) {
        return true;
      }
      if (ELEM(notifier.action, NA_ADDED, NA_REMOVED, NA_EDITED)) {
        return true;
      }
      break;
  }

  return false;
}

/**
 * \return The number of assets in the list.
 */
int AssetList::size() const
{
  return filelist_files_ensure(filelist_);
}

void AssetList::tag_main_data_dirty() const
{
  if (filelist_needs_reset_on_main_changes(filelist_)) {
    filelist_tag_force_reset_mainfiles(filelist_);
  }
}

void AssetList::remap_id(ID * /*id_old*/, ID * /*id_new*/) const
{
  /* Trigger full re-fetch of the file list if main data was changed, don't even attempt remap
   * pointers. We could give file list types a id-remap callback, but it's probably not worth it.
   * Refreshing local file lists is relatively cheap. */
  this->tag_main_data_dirty();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Runtime asset list cache
 * \{ */

static void clear(const AssetLibraryReference *library_reference, wmWindowManager *wm);
static void on_save_post(Main *main, PointerRNA **pointers, int num_pointers, void *arg);

/**
 * A global asset list map, each entry being a list for a specific asset library.
 */
using AssetListMap = Map<AssetLibraryReference, AssetList>;

struct GlobalStorage {
  AssetListMap list_map;
  bCallbackFuncStore on_save_callback_store{};

  GlobalStorage()
  {
    on_save_callback_store.alloc = false;

    on_save_callback_store.func = on_save_post;
    BKE_callback_add(&on_save_callback_store, BKE_CB_EVT_SAVE_POST);
  }
};

/**
 * Wrapper for Construct on First Use idiom, to avoid the Static Initialization Fiasco.
 */
static AssetListMap &libraries_map()
{
  static GlobalStorage global_storage;
  return global_storage.list_map;
}

static AssetList *lookup_list(const AssetLibraryReference &library_ref)
{
  return libraries_map().lookup_ptr(library_ref);
}

void storage_tag_main_data_dirty()
{
  for (AssetList &list : libraries_map().values()) {
    list.tag_main_data_dirty();
  }
}

void storage_id_remap(ID *id_old, ID *id_new)
{
  for (AssetList &list : libraries_map().values()) {
    list.remap_id(id_old, id_new);
  }
}

static std::optional<eFileSelectType> asset_library_reference_to_fileselect_type(
    const AssetLibraryReference &library_reference)
{
  switch (eAssetLibraryType(library_reference.type)) {
    case ASSET_LIBRARY_ALL:
      return FILE_ASSET_LIBRARY_ALL;
    case ASSET_LIBRARY_ESSENTIALS:
    case ASSET_LIBRARY_CUSTOM:
      return FILE_ASSET_LIBRARY;
    case ASSET_LIBRARY_LOCAL:
      return FILE_MAIN_ASSET;
  }

  return std::nullopt;
}

using is_new_t = bool;
static std::tuple<AssetList &, is_new_t> ensure_list_storage(
    const AssetLibraryReference &library_reference, eFileSelectType filesel_type)
{
  AssetListMap &storage = libraries_map();

  if (AssetList *list = storage.lookup_ptr(library_reference)) {
    return {*list, false};
  }
  storage.add(library_reference, AssetList(filesel_type, library_reference));
  return {storage.lookup(library_reference), true};
}

/** \} */

void asset_reading_region_listen_fn(const wmRegionListenerParams *params)
{
  const wmNotifier *wmn = params->notifier;
  ARegion *region = params->region;

  switch (wmn->category) {
    case NC_ASSET:
      if (ELEM(wmn->data, ND_ASSET_LIST_READING, ND_ASSET_LIST_PREVIEW)) {
        ED_region_tag_refresh_ui(region);
      }
      break;
  }
}

static void on_save_post(Main *main,
                         PointerRNA ** /*pointers*/,
                         int /*num_pointers*/,
                         void * /*arg*/)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(main->wm.first);
  const AssetLibraryReference current_file_library =
      asset_system::current_file_library_reference();
  clear(&current_file_library, wm);
}

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

void storage_fetch(const AssetLibraryReference *library_reference, const bContext *C)
{
  std::optional filesel_type = asset_library_reference_to_fileselect_type(*library_reference);
  if (!filesel_type) {
    return;
  }

  auto [list, is_new] = ensure_list_storage(*library_reference, *filesel_type);
  if (is_new || list.needs_refetch()) {
    list.setup();
    list.fetch(*C);
  }
}

void storage_fetch_blocking(const AssetLibraryReference &library_reference, const bContext &C)
{
  std::optional filesel_type = asset_library_reference_to_fileselect_type(library_reference);
  if (!filesel_type) {
    /* TODO: Warn? */
    return;
  }

  auto [list, is_new] = ensure_list_storage(library_reference, *filesel_type);
  if (is_new || list.needs_refetch()) {
    list.setup();
    list.ensure_blocking(C);
  }
}

bool is_loaded(const AssetLibraryReference *library_reference)
{
  AssetList *list = lookup_list(*library_reference);
  if (!list) {
    return false;
  }
  if (list->needs_refetch()) {
    return false;
  }
  return list->is_loaded();
}

static void foreach_visible_asset_browser_showing_library(
    const AssetLibraryReference &library_reference,
    const wmWindowManager *wm,
    const FunctionRef<void(SpaceFile &sfile)> fn)
{
  LISTBASE_FOREACH (const wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      /* Only needs to cover visible file/asset browsers, since others are already cleared through
       * area exiting. */
      if (area->spacetype == SPACE_FILE) {
        SpaceFile *sfile = reinterpret_cast<SpaceFile *>(area->spacedata.first);
        if (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS) {
          if (sfile->asset_params && sfile->asset_params->asset_library_ref == library_reference) {
            fn(*sfile);
          }
        }
      }
    }
  }
}

void clear(const AssetLibraryReference *library_reference, wmWindowManager *wm)
{
  AssetList *list = lookup_list(*library_reference);
  if (list) {
    list->clear(wm);
  }

  /* Only needs to cover visible file/asset browsers, since others are already cleared through area
   * exiting. */
  foreach_visible_asset_browser_showing_library(
      *library_reference, wm, [&](SpaceFile &sfile) { ED_fileselect_clear(wm, &sfile); });

  /* Always clear the all library when clearing a nested one. */
  if (library_reference->type != ASSET_LIBRARY_ALL) {
    const AssetLibraryReference all_lib_ref = asset_system::all_library_reference();
    AssetList *all_lib_list = lookup_list(all_lib_ref);

    /* If the cleared nested library is the current file one, only clear current file assets. */
    if (library_reference->type == ASSET_LIBRARY_LOCAL) {
      if (all_lib_list) {
        all_lib_list->clear_current_file_assets(wm);
      }

      foreach_visible_asset_browser_showing_library(
          all_lib_ref, wm, [&](SpaceFile &sfile) { ED_fileselect_clear_main_assets(wm, &sfile); });
    }
    else {
      if (all_lib_list) {
        all_lib_list->clear(wm);
      }

      foreach_visible_asset_browser_showing_library(
          all_lib_ref, wm, [&](SpaceFile &sfile) { ED_fileselect_clear(wm, &sfile); });
    }
  }
}

void clear(const AssetLibraryReference *library_reference, const bContext *C)
{
  clear(library_reference, CTX_wm_manager(C));
}

void clear_all_library(const bContext *C)
{
  const AssetLibraryReference all_lib_ref = asset_system::all_library_reference();
  clear(&all_lib_ref, CTX_wm_manager(C));
}

bool has_list_storage_for_library(const AssetLibraryReference *library_reference)
{
  return lookup_list(*library_reference) != nullptr;
}

bool has_asset_browser_storage_for_library(const AssetLibraryReference *library_reference,
                                           const bContext *C)
{
  bool has_asset_browser = false;
  foreach_visible_asset_browser_showing_library(
      *library_reference, CTX_wm_manager(C), [&](SpaceFile & /*sfile*/) {
        has_asset_browser = true;
      });

  return has_asset_browser;
}

void iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn)
{
  AssetList *list = lookup_list(library_reference);
  if (list) {
    list->iterate(fn);
  }
}

asset_system::AssetLibrary *library_get_once_available(
    const AssetLibraryReference &library_reference)
{
  const AssetList *list = lookup_list(library_reference);
  if (!list) {
    return nullptr;
  }
  return list->asset_library();
}

bool listen(const wmNotifier *notifier)
{
  return AssetList::listen(*notifier);
}

int size(const AssetLibraryReference *library_reference)
{
  AssetList *list = lookup_list(*library_reference);
  if (list) {
    return list->size();
  }
  return -1;
}

void storage_exit()
{
  libraries_map().clear();
}

/** \} */

}  // namespace blender::ed::asset::list
