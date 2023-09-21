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

#include "BKE_context.h"
#include "BKE_preview_image.hh"
#include "BKE_screen.h"

#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_utility_mixins.hh"

#include "DNA_space_types.h"

#include "BKE_icons.h"
#include "BKE_preferences.h"

#include "WM_api.hh"

/* XXX uses private header of file-space. */
#include "../space_file/file_indexer.hh"
#include "../space_file/filelist.hh"

#include "ED_asset_handle.h"
#include "ED_asset_indexer.h"
#include "ED_asset_list.h"
#include "ED_asset_list.hh"
#include "ED_screen.hh"
#include "asset_library_reference.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"

namespace blender::ed::asset {

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
    MEM_freeN(list);
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
  /** Storage for asset handles, items are lazy-created on request.
   *  Asset handles are stored as a pointer here, to ensure a consistent memory address (address
   *  inside the map changes as the map changes). */
  mutable Map<uint32_t, std::unique_ptr<AssetHandle>> asset_handle_map_;
  AssetLibraryReference library_ref_;

 public:
  AssetList() = delete;
  AssetList(eFileSelectType filesel_type, const AssetLibraryReference &asset_library_ref);
  AssetList(AssetList &&other) = default;
  ~AssetList() = default;

  bool listen(const wmNotifier &notifier);

  void setup();
  void fetch(const bContext &C);
  void setCatalogFilterSettings(const AssetCatalogFilterSettings &settings);
  void clear(bContext *C);

  AssetHandle *asset_get_by_index(int index) const;

  bool needsRefetch() const;
  bool isLoaded() const;
  bool isAssetPreviewLoading(const AssetHandle &asset) const;
  asset_system::AssetLibrary *asset_library() const;
  AssetHandle &asset_handle_from_file(const FileDirEntry &) const;
  void iterate(AssetListHandleIterFn fn) const;
  void iterate(AssetListIterFn fn) const;
  int size() const;
  void tagMainDataDirty() const;
  void remapID(ID *id_old, ID *id_new) const;
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
  filelist_setsorting(files, FILE_SORT_ALPHA, false);
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

  const bool use_asset_indexer = !USER_EXPERIMENTAL_TEST(&U, no_asset_indexing);
  filelist_setindexer(files, use_asset_indexer ? &file_indexer_asset : &file_indexer_noop);

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
    asset_handle_map_.clear();
  }

  if (filelist_needs_reading(files)) {
    if (!filelist_pending(files)) {
      filelist_readjob_start(files, NC_ASSET | ND_ASSET_LIST_READING, &C);
    }
  }
  filelist_sort(files);
  filelist_filter(files);
}

void AssetList::setCatalogFilterSettings(const AssetCatalogFilterSettings &settings)
{
  filelist_set_asset_catalog_filter_options(
      filelist_, (AssetCatalogFilterMode)settings.filter_mode, &settings.active_catalog_id);
}

bool AssetList::needsRefetch() const
{
  return filelist_needs_force_reset(filelist_) || filelist_needs_reading(filelist_);
}

bool AssetList::isLoaded() const
{
  return filelist_is_ready(filelist_);
}

bool AssetList::isAssetPreviewLoading(const AssetHandle &asset) const
{
  return filelist_file_is_preview_pending(filelist_, asset.file_data);
}

asset_system::AssetLibrary *AssetList::asset_library() const
{
  return reinterpret_cast<asset_system::AssetLibrary *>(filelist_asset_library(filelist_));
}

AssetHandle &AssetList::asset_handle_from_file(const FileDirEntry &file) const
{
  AssetHandle &asset = *asset_handle_map_.lookup_or_add(
      file.uid, std::make_unique<AssetHandle>(AssetHandle{&file}));
  /* The file is recreated while loading, update the pointer here. */
  asset.file_data = &file;
  return asset;
}

void AssetList::iterate(AssetListHandleIterFn fn) const
{
  FileList *files = filelist_;
  int numfiles = filelist_files_ensure(files);

  for (int i = 0; i < numfiles; i++) {
    FileDirEntry *file = filelist_file(files, i);
    if ((file->typeflag & FILE_TYPE_ASSET) == 0) {
      continue;
    }

    AssetHandle &asset_handle = asset_handle_from_file(*file);
    if (!fn(asset_handle)) {
      /* If the callback returns false, we stop iterating. */
      break;
    }
  }
}

void AssetList::iterate(AssetListIterFn fn) const
{
  iterate([&fn](AssetHandle handle) {
    asset_system::AssetRepresentation &asset =
        reinterpret_cast<blender::asset_system::AssetRepresentation &>(*handle.file_data->asset);

    return fn(asset);
  });
}

void AssetList::clear(bContext *C)
{
  /* Based on #ED_fileselect_clear() */

  FileList *files = filelist_;
  filelist_readjob_stop(files, CTX_wm_manager(C));
  filelist_freelib(files);
  filelist_clear(files);
  asset_handle_map_.clear();

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST, nullptr);
}

AssetHandle *AssetList::asset_get_by_index(int index) const
{
  FileDirEntry *file = filelist_file(filelist_, index);
  if (!file) {
    return nullptr;
  }
  return &asset_handle_from_file(*file);
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
      if (ELEM(notifier.data, ND_ASSET_LIST)) {
        filelist_tag_needs_filtering(filelist_);
        return true;
      }
      if (ELEM(notifier.data, ND_ASSET_LIST_READING)) {
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

void AssetList::tagMainDataDirty() const
{
  if (filelist_needs_reset_on_main_changes(filelist_)) {
    filelist_tag_force_reset_mainfiles(filelist_);
  }
}

void AssetList::remapID(ID * /*id_old*/, ID * /*id_new*/) const
{
  /* Trigger full re-fetch of the file list if main data was changed, don't even attempt remap
   * pointers. We could give file list types a id-remap callback, but it's probably not worth it.
   * Refreshing local file lists is relatively cheap. */
  tagMainDataDirty();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Runtime asset list cache
 * \{ */

/**
 * Class managing a global asset list map, each entry being a list for a specific asset library.
 */
class AssetListStorage {
  using AssetListMap = Map<AssetLibraryReferenceWrapper, AssetList>;

 public:
  /* Purely static class, can't instantiate this. */
  AssetListStorage() = delete;

  static void fetch_library(const AssetLibraryReference &library_reference, const bContext &C);
  static void destruct();
  static AssetList *lookup_list(const AssetLibraryReference &library_ref);
  static void tagMainDataDirty();
  static void remapID(ID *id_new, ID *id_old);

 private:
  static std::optional<eFileSelectType> asset_library_reference_to_fileselect_type(
      const AssetLibraryReference &library_reference);

  using is_new_t = bool;
  static std::tuple<AssetList &, is_new_t> ensure_list_storage(
      const AssetLibraryReference &library_reference, eFileSelectType filesel_type);

  static AssetListMap &global_storage();
};

void AssetListStorage::fetch_library(const AssetLibraryReference &library_reference,
                                     const bContext &C)
{
  std::optional filesel_type = asset_library_reference_to_fileselect_type(library_reference);
  if (!filesel_type) {
    return;
  }

  auto [list, is_new] = ensure_list_storage(library_reference, *filesel_type);
  if (is_new || list.needsRefetch()) {
    list.setup();
    list.fetch(C);
  }
}

void AssetListStorage::destruct()
{
  global_storage().clear();
}

AssetList *AssetListStorage::lookup_list(const AssetLibraryReference &library_ref)
{
  return global_storage().lookup_ptr(library_ref);
}

void AssetListStorage::tagMainDataDirty()
{
  for (AssetList &list : global_storage().values()) {
    list.tagMainDataDirty();
  }
}

void AssetListStorage::remapID(ID *id_new, ID *id_old)
{
  for (AssetList &list : global_storage().values()) {
    list.remapID(id_new, id_old);
  }
}

std::optional<eFileSelectType> AssetListStorage::asset_library_reference_to_fileselect_type(
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

std::tuple<AssetList &, AssetListStorage::is_new_t> AssetListStorage::ensure_list_storage(
    const AssetLibraryReference &library_reference, eFileSelectType filesel_type)
{
  AssetListMap &storage = global_storage();

  if (AssetList *list = storage.lookup_ptr(library_reference)) {
    return {*list, false};
  }
  storage.add(library_reference, AssetList(filesel_type, library_reference));
  return {storage.lookup(library_reference), true};
}

/**
 * Wrapper for Construct on First Use idiom, to avoid the Static Initialization Fiasco.
 */
AssetListStorage::AssetListMap &AssetListStorage::global_storage()
{
  static AssetListMap global_storage_;
  return global_storage_;
}

/** \} */

void asset_reading_region_listen_fn(const wmRegionListenerParams *params)
{
  const wmNotifier *wmn = params->notifier;
  ARegion *region = params->region;

  switch (wmn->category) {
    case NC_ASSET:
      if (wmn->data == ND_ASSET_LIST_READING) {
        ED_region_tag_refresh_ui(region);
      }
      break;
  }
}

}  // namespace blender::ed::asset

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender;
using namespace blender::ed::asset;

void ED_assetlist_storage_fetch(const AssetLibraryReference *library_reference, const bContext *C)
{
  AssetListStorage::fetch_library(*library_reference, *C);
}

bool ED_assetlist_is_loaded(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (!list) {
    return false;
  }
  if (list->needsRefetch()) {
    return false;
  }
  return list->isLoaded();
}

void ED_assetlist_catalog_filter_set(const struct AssetLibraryReference *library_reference,
                                     const struct AssetCatalogFilterSettings *settings)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    list->setCatalogFilterSettings(*settings);
  }
}

void ED_assetlist_clear(const AssetLibraryReference *library_reference, bContext *C)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    list->clear(C);
  }
}

bool ED_assetlist_storage_has_list_for_library(const AssetLibraryReference *library_reference)
{
  return AssetListStorage::lookup_list(*library_reference) != nullptr;
}

void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListHandleIterFn fn)
{
  AssetList *list = AssetListStorage::lookup_list(library_reference);
  if (list) {
    list->iterate(fn);
  }
}

void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn)
{
  AssetList *list = AssetListStorage::lookup_list(library_reference);
  if (list) {
    list->iterate(fn);
  }
}

asset_system::AssetLibrary *ED_assetlist_library_get_once_available(
    const AssetLibraryReference &library_reference)
{
  const AssetList *list = AssetListStorage::lookup_list(library_reference);
  if (!list) {
    return nullptr;
  }
  return list->asset_library();
}

AssetHandle *ED_assetlist_asset_handle_get_by_index(const AssetLibraryReference *library_reference,
                                                    int asset_index)
{
  const AssetList *list = AssetListStorage::lookup_list(*library_reference);
  return list->asset_get_by_index(asset_index);
}

asset_system::AssetRepresentation *ED_assetlist_asset_get_by_index(
    const AssetLibraryReference &library_reference, int asset_index)
{
  AssetHandle *asset_handle = ED_assetlist_asset_handle_get_by_index(&library_reference,
                                                                     asset_index);
  return reinterpret_cast<asset_system::AssetRepresentation *>(asset_handle->file_data->asset);
}

PreviewImage *ED_assetlist_asset_preview_request(AssetHandle *asset_handle)
{
  if (asset_handle->preview) {
    return asset_handle->preview;
  }

  asset_system::AssetRepresentation *asset = ED_asset_handle_get_representation(asset_handle);
  if (ID *local_id = asset->local_id()) {
    asset_handle->preview = BKE_previewimg_id_get(local_id);
  }
  else {
    const char *asset_identifier = asset->get_identifier().library_relative_identifier().c_str();
    const int source = filelist_preview_source_get(asset_handle->file_data->typeflag);
    const std::string asset_path = asset->get_identifier().full_path();

    asset_handle->preview = BKE_previewimg_cached_thumbnail_read(
        asset_identifier, asset_path.c_str(), source, false);
  }

  return asset_handle->preview;
}

static int preview_icon_id_ensure(AssetHandle *asset_handle, PreviewImage *preview)
{
  asset_system::AssetRepresentation *asset = ED_asset_handle_get_representation(asset_handle);
  return BKE_icon_preview_ensure(asset->local_id(), preview);
}

int ED_assetlist_asset_preview_icon_id_request(AssetHandle *asset_handle)
{
  PreviewImage *preview = ED_assetlist_asset_preview_request(asset_handle);
  return preview_icon_id_ensure(asset_handle, preview);
}

int ED_assetlist_asset_preview_or_type_icon_id_request(AssetHandle *asset_handle)
{
  PreviewImage *preview = ED_assetlist_asset_preview_request(asset_handle);

  /* Preview is invalid or still loading. Return an icon ID based on the type. */
  if (preview->tag & (PRV_TAG_LOADING_FAILED | PRV_TAG_DEFFERED_RENDERING)) {
    ID_Type id_type = ED_asset_handle_get_id_type(asset_handle);
    return UI_icon_from_idcode(id_type);
  }

  return preview_icon_id_ensure(asset_handle, preview);
}

bool ED_assetlist_asset_image_is_loading(const AssetLibraryReference *library_reference,
                                         const AssetHandle *asset_handle)
{
  const AssetList *list = AssetListStorage::lookup_list(*library_reference);
  return list->isAssetPreviewLoading(*asset_handle);
}

ImBuf *ED_assetlist_asset_image_get(const AssetHandle *asset_handle)
{
  ImBuf *imbuf = filelist_file_getimage(asset_handle->file_data);
  if (imbuf) {
    return imbuf;
  }

  return filelist_geticon_image_ex(asset_handle->file_data);
}

AssetLibrary *ED_assetlist_library_get(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return reinterpret_cast<AssetLibrary *>(list->asset_library());
  }
  return nullptr;
}

bool ED_assetlist_listen(const AssetLibraryReference *library_reference,
                         const wmNotifier *notifier)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return list->listen(*notifier);
  }
  return false;
}

int ED_assetlist_size(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return list->size();
  }
  return -1;
}

void ED_assetlist_storage_tag_main_data_dirty()
{
  AssetListStorage::tagMainDataDirty();
}

void ED_assetlist_storage_id_remap(ID *id_old, ID *id_new)
{
  AssetListStorage::remapID(id_old, id_new);
}

void ED_assetlist_storage_exit()
{
  AssetListStorage::destruct();
}

/** \} */
