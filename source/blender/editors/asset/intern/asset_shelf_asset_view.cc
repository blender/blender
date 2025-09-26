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

#include "BKE_screen.hh"

#include "BLI_fnmatch.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_asset_types.h"
#include "DNA_screen_types.h"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_asset_shelf.hh"

#include "UI_grid_view.hh"
#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "asset_shelf.hh"

namespace blender::ed::asset::shelf {

class AssetView : public ui::AbstractGridView {
  const AssetLibraryReference library_ref_;
  const AssetShelf &shelf_;
  std::optional<AssetWeakReference> active_asset_;
  std::optional<asset_system::AssetCatalogFilter> catalog_filter_ = std::nullopt;

  friend class AssetViewItem;
  friend class AssetDragController;

 public:
  AssetView(const AssetLibraryReference &library_ref, const AssetShelf &shelf);

  void build_items() override;
  bool begin_filtering(const bContext &C) const override;

  void set_catalog_filter(const std::optional<asset_system::AssetCatalogFilter> &catalog_filter);
};

class AssetViewItem : public ui::PreviewGridItem {
  asset_system::AssetRepresentation &asset_;
  bool allow_asset_drag_ = true;

 public:
  AssetViewItem(asset_system::AssetRepresentation &asset_, StringRef identifier, StringRef label);

  void disable_asset_drag();
  void build_grid_tile(const bContext &C, uiLayout &layout) const override;
  void build_context_menu(bContext &C, uiLayout &column) const override;
  std::optional<bool> should_be_active() const override;
  void on_activate(bContext &C) override;
  bool should_be_filtered_visible(StringRefNull filter_string) const override;

  std::unique_ptr<ui::AbstractViewItemDragController> create_drag_controller() const override;
};

class AssetDragController : public ui::AbstractViewItemDragController {
  asset_system::AssetRepresentation &asset_;

 public:
  AssetDragController(ui::AbstractGridView &view, asset_system::AssetRepresentation &asset);

  std::optional<eWM_DragDataType> get_drag_type() const override;
  void *create_drag_data() const override;
  void on_drag_start(bContext &C) override;
};

AssetView::AssetView(const AssetLibraryReference &library_ref, const AssetShelf &shelf)
    : library_ref_(library_ref), shelf_(shelf)
{
  if (shelf.type->get_active_asset) {
    if (const AssetWeakReference *weak_ref = shelf.type->get_active_asset(shelf.type)) {
      active_asset_ = *weak_ref;
    }
    else {
      active_asset_.reset();
    }
  }
}

void AssetView::build_items()
{
  const asset_system::AssetLibrary *library = list::library_get_once_available(library_ref_);
  if (!library) {
    return;
  }

  list::iterate(library_ref_, [&](asset_system::AssetRepresentation &asset) {
    if (shelf_.type->asset_poll && !shelf_.type->asset_poll(shelf_.type, &asset)) {
      return true;
    }

    const AssetMetaData &asset_data = asset.get_metadata();
    if (catalog_filter_ && !catalog_filter_->contains(asset_data.catalog_id)) {
      /* Skip this asset. */
      return true;
    }

    const bool show_names = (shelf_.settings.display_flag & ASSETSHELF_SHOW_NAMES);
    const StringRef identifier = asset.library_relative_identifier();

    AssetViewItem &item = this->add_item<AssetViewItem>(asset, identifier, asset.get_name());
    if (!show_names) {
      item.hide_label();
    }
    if (shelf_.type->flag & ASSET_SHELF_TYPE_FLAG_NO_ASSET_DRAG) {
      item.disable_asset_drag();
    }
    if (!shelf_.type->drag_operator.empty()) {
      /* For now always select/activate items on click instead of press when there's a drag
       * operator set. Important for pose library blending. Maybe we want to make this an explicit
       * option of the asset shelf instead. */
      item.select_on_click_set();
    }
    /* Make sure every click calls the #bl_activate_operator. We might want to add a flag to
     * enable/disable this. Or we only call #bl_activate_operator when an item becomes active, and
     * add a #bl_click_operator for repeated execution on every click. So far it seems like every
     * asset shelf use case works with activating on every click though. */
    item.always_reactivate_on_click();
    if (shelf_.type->flag & ASSET_SHELF_TYPE_FLAG_ACTIVATE_FOR_CONTEXT_MENU) {
      item.activate_for_context_menu_set();
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

  asset_system::AssetCatalog *active_catalog = library.catalog_service().find_catalog_by_path(
      shelf_settings.active_catalog_path);
  if (!active_catalog) {
    return {};
  }

  return library.catalog_service().create_catalog_filter(active_catalog->catalog_id);
}

/* ---------------------------------------------------------------------- */

AssetViewItem::AssetViewItem(asset_system::AssetRepresentation &asset,
                             StringRef identifier,
                             StringRef label)
    : ui::PreviewGridItem(identifier, label, ICON_NONE), asset_(asset)
{
}

void AssetViewItem::disable_asset_drag()
{
  allow_asset_drag_ = false;
}

/**
 * Needs freeing with #WM_operator_properties_free() (will be done by button if passed to that) and
 * #MEM_freeN().
 */
static std::optional<wmOperatorCallParams> create_asset_operator_params(
    const StringRefNull op_name, const asset_system::AssetRepresentation &asset)
{
  if (op_name.is_empty()) {
    return {};
  }
  wmOperatorType *ot = WM_operatortype_find(op_name.c_str(), true);
  if (!ot) {
    return {};
  }

  PointerRNA *op_props = MEM_new<PointerRNA>(__func__);
  WM_operator_properties_create_ptr(op_props, ot);
  asset::operator_asset_reference_props_set(asset, *op_props);
  return wmOperatorCallParams{ot, op_props, wm::OpCallContext::InvokeRegionWin};
}

void AssetViewItem::build_grid_tile(const bContext & /*C*/, uiLayout &layout) const
{
  const AssetView &asset_view = reinterpret_cast<const AssetView &>(this->get_view());
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;

  PointerRNA asset_ptr = RNA_pointer_create_discrete(nullptr, &RNA_AssetRepresentation, &asset_);
  UI_but_context_ptr_set(
      layout.block(), reinterpret_cast<uiBut *>(view_item_but_), "asset", &asset_ptr);

  uiBut *item_but = reinterpret_cast<uiBut *>(this->view_item_button());
  if (std::optional<wmOperatorCallParams> activate_op = create_asset_operator_params(
          shelf_type.activate_operator, asset_))
  {
    /* Attach the operator, but don't call it through the button. We call it using
     * #on_activate(). */
    UI_but_operator_set(item_but, activate_op->optype, activate_op->opcontext, activate_op->opptr);
    UI_but_operator_set_never_call(item_but);

    MEM_delete(activate_op->opptr);
  }
  const ui::GridViewStyle &style = this->get_view().get_style();
  /* Increase background draw size slightly, so highlights are well visible behind previews with an
   * opaque background. */
  UI_but_view_item_draw_size_set(
      item_but, style.tile_width + 2 * U.pixelsize, style.tile_height + 2 * U.pixelsize);

  UI_but_func_tooltip_custom_set(
      item_but,
      [](bContext & /*C*/, uiTooltipData &tip, uiBut * /*but*/, void *argN) {
        const asset_system::AssetRepresentation *asset =
            static_cast<const asset_system::AssetRepresentation *>(argN);
        asset_tooltip(*asset, tip);
      },
      (&asset_),
      nullptr);

  /* Request preview when drawing. Grid views have an optimization to only draw items that are
   * actually visible, so only previews scrolled into view will be loaded this way. This reduces
   * total loading time and memory footprint. */
  asset_.ensure_previewable();

  const int preview_id = [&]() -> int {
    /* Show loading icon while list is loading still. Previews might get pushed out of view again
     * while the list grows, which can cause a lot of flickering. Note that this also means the
     * actual loading of previews is delayed, because that only happens when a preview icon-ID is
     * attached to a button. */
    if (!list::is_loaded(&asset_view.library_ref_)) {
      return ICON_PREVIEW_LOADING;
    }
    return asset_preview_or_icon(asset_);
  }();

  ui::PreviewGridItem::build_grid_tile_button(layout, preview_id);
}

void AssetViewItem::build_context_menu(bContext &C, uiLayout &column) const
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(this->get_view());
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;
  if (shelf_type.draw_context_menu) {
    shelf_type.draw_context_menu(&C, &shelf_type, &asset_, &column);
  }
}

std::optional<bool> AssetViewItem::should_be_active() const
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(this->get_view());
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;
  if (!shelf_type.get_active_asset) {
    return {};
  }
  if (!asset_view.active_asset_) {
    return false;
  }
  AssetWeakReference weak_ref = asset_.make_weak_reference();
  const bool matches = *asset_view.active_asset_ == weak_ref;

  return matches;
}

void AssetViewItem::on_activate(bContext &C)
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(this->get_view());
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;

  if (std::optional<wmOperatorCallParams> activate_op = create_asset_operator_params(
          shelf_type.activate_operator, asset_))
  {
    WM_operator_name_call_ptr(
        &C, activate_op->optype, activate_op->opcontext, activate_op->opptr, nullptr);
    WM_operator_properties_free(activate_op->opptr);
    MEM_delete(activate_op->opptr);
  }
}

bool AssetViewItem::should_be_filtered_visible(const StringRefNull filter_string) const
{
  const StringRefNull asset_name = asset_.get_name();
  return fnmatch(filter_string.c_str(), asset_name.c_str(), FNM_CASEFOLD) == 0;
}

std::unique_ptr<ui::AbstractViewItemDragController> AssetViewItem::create_drag_controller() const
{
  const AssetView &asset_view = dynamic_cast<const AssetView &>(this->get_view());
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;

  if (!allow_asset_drag_ && shelf_type.drag_operator.empty()) {
    return nullptr;
  }
  return std::make_unique<AssetDragController>(this->get_view(), asset_);
}

/* ---------------------------------------------------------------------- */

static std::string filter_string_get(const AssetShelf &shelf)
{
  /* Copy of the filter string from #AssetShelfSettings, with extra '*' added to the beginning and
   * end of the string, for `fnmatch()` to work. */
  char search_string[sizeof(AssetShelfSettings::search_string) + 2];
  BLI_strncpy_ensure_pad(search_string, shelf.settings.search_string, '*', sizeof(search_string));
  return search_string;
}

void build_asset_view(uiLayout &layout,
                      const AssetLibraryReference &library_ref,
                      const AssetShelf &shelf,
                      const bContext &C)
{
  list::storage_fetch(&library_ref, &C);

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

  uiBlock *block = layout.block();
  ui::AbstractGridView *grid_view = UI_block_add_view(
      *block, "asset shelf asset view", std::move(asset_view));
  grid_view->set_context_menu_title("Asset Shelf");

  ui::GridViewBuilder builder(*block);
  builder.build_grid_view(C, *grid_view, layout, filter_string_get(shelf));
}

/* ---------------------------------------------------------------------- */
/* Dragging. */

AssetDragController::AssetDragController(ui::AbstractGridView &view,
                                         asset_system::AssetRepresentation &asset)
    : ui::AbstractViewItemDragController(view), asset_(asset)
{
}

std::optional<eWM_DragDataType> AssetDragController::get_drag_type() const
{
  const AssetView &asset_view = this->get_view<AssetView>();
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;

  /* Disable asset dragging, only call #AssetShelfType::drag_operator in #on_drag_start(). */
  if (!shelf_type.drag_operator.empty()) {
    return std::nullopt;
  }
  return asset_.is_local_id() ? WM_DRAG_ID : WM_DRAG_ASSET;
}

void AssetDragController::on_drag_start(bContext &C)
{
  const AssetView &asset_view = this->get_view<AssetView>();
  const AssetShelfType &shelf_type = *asset_view.shelf_.type;

  if (std::optional<wmOperatorCallParams> drag_op = create_asset_operator_params(
          shelf_type.drag_operator, asset_))
  {
    WM_operator_name_call_ptr(&C, drag_op->optype, drag_op->opcontext, drag_op->opptr, nullptr);
    WM_operator_properties_free(drag_op->opptr);
    MEM_delete(drag_op->opptr);
  }
}

void *AssetDragController::create_drag_data() const
{
  ID *local_id = asset_.local_id();
  if (local_id) {
    return static_cast<void *>(local_id);
  }

  eAssetImportMethod import_method = asset_.get_import_method().value_or(ASSET_IMPORT_PACK);
  if (U.experimental.no_data_block_packing && import_method == ASSET_IMPORT_PACK) {
    import_method = ASSET_IMPORT_APPEND_REUSE;
  }

  AssetImportSettings import_settings{};
  import_settings.method = import_method;
  import_settings.use_instance_collections = false;

  return WM_drag_create_asset_data(&asset_, import_settings);
}

}  // namespace blender::ed::asset::shelf
