/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Grid-view showing all assets according to the giving shelf-type and settings.
 */

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BKE_asset.hh"
#include "BKE_screen.hh"

#include "BLI_fnmatch.h"
#include "BLI_string.h"

#include "DNA_asset_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_asset_handle.hh"
#include "ED_asset_list.hh"
#include "ED_asset_shelf.hh"

#include "UI_grid_view.hh"
#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"

#include "asset_shelf.hh"

namespace blender::ed::asset::shelf {

class AssetView : public ui::AbstractGridView {
  const AssetLibraryReference library_ref_;
  const AssetShelf &shelf_;
  AssetWeakReference *active_asset_ = nullptr;
  /** Copy of the filter string from #AssetShelfSettings, with extra '*' added to the beginning and
   * end of the string, for `fnmatch()` to work. */
  char search_string[sizeof(AssetShelfSettings::search_string) + 2] = "";
  std::optional<asset_system::AssetCatalogFilter> catalog_filter_ = std::nullopt;

  friend class AssetViewItem;
  friend class AssetDragController;

 public:
  AssetView(const AssetLibraryReference &library_ref, const AssetShelf &shelf);
  ~AssetView();

  void build_items() override;
  bool begin_filtering(const bContext &C) const override;

  void set_catalog_filter(const std::optional<asset_system::AssetCatalogFilter> &catalog_filter);
};

class AssetViewItem : public ui::PreviewGridItem {
  AssetHandle asset_;
  bool allow_asset_drag_ = true;

 public:
  AssetViewItem(const AssetHandle &asset,
                StringRef identifier,
                StringRef label,
                int preview_icon_id);

  void disable_asset_drag();
  void build_grid_tile(uiLayout &layout) const override;
  void build_context_menu(bContext &C, uiLayout &column) const override;
  std::optional<bool> should_be_active() const override;
  bool is_filtered_visible() const override;

  std::unique_ptr<ui::AbstractViewItemDragController> create_drag_controller() const override;
};

class AssetDragController : public ui::AbstractViewItemDragController {
  asset_system::AssetRepresentation &asset_;

 public:
  AssetDragController(ui::AbstractGridView &view, asset_system::AssetRepresentation &asset);

  eWM_DragDataType get_drag_type() const override;
  void *create_drag_data() const override;
};

AssetView::AssetView(const AssetLibraryReference &library_ref, const AssetShelf &shelf)
    : library_ref_(library_ref), shelf_(shelf)
{
  if (shelf.settings.search_string[0]) {
    BLI_strncpy_ensure_pad(
        search_string, shelf.settings.search_string, '*', sizeof(search_string));
  }
  if (shelf.type->get_active_asset) {
    active_asset_ = BKE_asset_weak_reference_copy(shelf.type->get_active_asset(shelf.type));
  }
}

AssetView::~AssetView()
{
  BKE_asset_weak_reference_free(&active_asset_);
}

void AssetView::build_items()
{
  const asset_system::AssetLibrary *library = list::library_get_once_available(library_ref_);
  if (!library) {
    return;
  }

  list::iterate(library_ref_, [&](AssetHandle asset_handle) {
    const asset_system::AssetRepresentation *asset = handle_get_representation(&asset_handle);

    if (shelf_.type->asset_poll && !shelf_.type->asset_poll(shelf_.type, asset)) {
      return true;
    }

    const AssetMetaData &asset_data = asset->get_metadata();

    if (catalog_filter_ && !catalog_filter_->contains(asset_data.catalog_id)) {
      /* Skip this asset. */
      return true;
    }

    const bool show_names = (shelf_.settings.display_flag & ASSETSHELF_SHOW_NAMES);

    const StringRef identifier = asset->get_identifier().library_relative_identifier();
    const int preview_id = [&]() -> int {
      if (list::asset_image_is_loading(&library_ref_, &asset_handle)) {
        return ICON_TEMP;
      }
      return handle_get_preview_or_type_icon_id(&asset_handle);
    }();

    AssetViewItem &item = add_item<AssetViewItem>(
        asset_handle, identifier, asset->get_name(), preview_id);
    if (!show_names) {
      item.hide_label();
    }
    if (shelf_.type->flag & ASSET_SHELF_TYPE_FLAG_NO_ASSET_DRAG) {
      item.disable_asset_drag();
    }

    return true;
  });
}

bool AssetView::begin_filtering(const bContext &C) const
{
  const ScrArea *area = CTX_wm_area(&C);
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (UI_textbutton_activate_rna(&C, region, &shelf_, "search_filter")) {
      return true;
    }
  }

  return false;
}

void AssetView::set_catalog_filter(
    const std::optional<asset_system::AssetCatalogFilter> &catalog_filter)
{
  if (catalog_filter) {
    catalog_filter_.emplace(*catalog_filter);
  }
  else {
    catalog_filter_ = std::nullopt;
  }
}

static std::optional<asset_system::AssetCatalogFilter> catalog_filter_from_shelf_settings(
    const AssetShelfSettings &shelf_settings, const asset_system::AssetLibrary &library)
{
  if (!shelf_settings.active_catalog_path) {
    return {};
  }

  asset_system::AssetCatalog *active_catalog = library.catalog_service->find_catalog_by_path(
      shelf_settings.active_catalog_path);
  if (!active_catalog) {
    return {};
  }

  return library.catalog_service->create_catalog_filter(active_catalog->catalog_id);
}

/* ---------------------------------------------------------------------- */

AssetViewItem::AssetViewItem(const AssetHandle &asset,
                             StringRef identifier,
                             StringRef label,
                             int preview_icon_id)
    : ui::PreviewGridItem(identifier, label, preview_icon_id), asset_(asset)
{
}

void AssetViewItem::disable_asset_drag()
{
  allow_asset_drag_ = false;
}

void AssetViewItem::build_grid_tile(uiLayout &layout) const
{
  PointerRNA file_ptr = RNA_pointer_create(
      nullptr,
      &RNA_FileSelectEntry,
      /* XXX passing file pointer here, should be asset handle or asset representation. */
      const_cast<FileDirEntry *>(asset_.file_data));

  uiBlock *block = uiLayoutGetBlock(&layout);
  UI_but_context_ptr_set(
      block, reinterpret_cast<uiBut *>(view_item_but_), "active_file", &file_ptr);
  ui::PreviewGridItem::build_grid_tile(layout);
}

void AssetViewItem::build_context_menu(bContext &C, uiLayout &column) const
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(get_view());
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;
  if (shelf_type.draw_context_menu) {
    asset_system::AssetRepresentation *asset = handle_get_representation(&asset_);
    shelf_type.draw_context_menu(&C, &shelf_type, asset, &column);
  }
}

std::optional<bool> AssetViewItem::should_be_active() const
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(get_view());
  if (!asset_view.active_asset_) {
    return false;
  }
  const asset_system::AssetRepresentation *asset = handle_get_representation(&asset_);
  AssetWeakReference *weak_ref = asset->make_weak_reference();
  const bool matches = *asset_view.active_asset_ == *weak_ref;

  BKE_asset_weak_reference_free(&weak_ref);
  return matches;
}

bool AssetViewItem::is_filtered_visible() const
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(get_view());
  if (asset_view.search_string[0] == '\0') {
    return true;
  }

  const StringRefNull asset_name = handle_get_representation(&asset_)->get_name();
  return fnmatch(asset_view.search_string, asset_name.c_str(), FNM_CASEFOLD) == 0;
}

std::unique_ptr<ui::AbstractViewItemDragController> AssetViewItem::create_drag_controller() const
{
  if (!allow_asset_drag_) {
    return nullptr;
  }
  asset_system::AssetRepresentation *asset = handle_get_representation(&asset_);
  return std::make_unique<AssetDragController>(get_view(), *asset);
}

/* ---------------------------------------------------------------------- */

void build_asset_view(uiLayout &layout,
                      const AssetLibraryReference &library_ref,
                      const AssetShelf &shelf,
                      const bContext &C,
                      ARegion &region)
{
  list::storage_fetch(&library_ref, &C);
  list::ensure_previews_job(&library_ref, &C);

  const asset_system::AssetLibrary *library = list::library_get_once_available(library_ref);
  if (!library) {
    return;
  }

  const float tile_width = shelf::tile_width(shelf.settings);
  const float tile_height = shelf::tile_height(shelf.settings);
  BLI_assert(tile_width != 0);
  BLI_assert(tile_height != 0);

  std::unique_ptr asset_view = std::make_unique<AssetView>(library_ref, shelf);
  asset_view->set_catalog_filter(catalog_filter_from_shelf_settings(shelf.settings, *library));
  asset_view->set_tile_size(tile_width, tile_height);

  uiBlock *block = uiLayoutGetBlock(&layout);
  ui::AbstractGridView *grid_view = UI_block_add_view(
      *block, "asset shelf asset view", std::move(asset_view));

  ui::GridViewBuilder builder(*block);
  builder.build_grid_view(*grid_view, region.v2d, layout);
}

/* ---------------------------------------------------------------------- */
/* Dragging. */

AssetDragController::AssetDragController(ui::AbstractGridView &view,
                                         asset_system::AssetRepresentation &asset)
    : ui::AbstractViewItemDragController(view), asset_(asset)
{
}

eWM_DragDataType AssetDragController::get_drag_type() const
{
  return asset_.is_local_id() ? WM_DRAG_ID : WM_DRAG_ASSET;
}

void *AssetDragController::create_drag_data() const
{
  ID *local_id = asset_.local_id();
  if (local_id) {
    return static_cast<void *>(local_id);
  }

  const eAssetImportMethod import_method = asset_.get_import_method().value_or(
      ASSET_IMPORT_APPEND_REUSE);

  return WM_drag_create_asset_data(&asset_, import_method);
}

}  // namespace blender::ed::asset::shelf
