/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_library.hh"

#include "asset_shelf.hh"

#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "ED_asset_filter.hh"
#include "ED_asset_list.hh"
#include "ED_asset_shelf.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

namespace blender::ed::asset::shelf {

class StaticPopupShelves {
 public:
  Vector<AssetShelf *> popup_shelves;

  ~StaticPopupShelves()
  {
    for (AssetShelf *shelf : popup_shelves) {
      MEM_delete(shelf);
    }
  }

  static Vector<AssetShelf *> &shelves()
  {
    static StaticPopupShelves storage;
    return storage.popup_shelves;
  }
};

void type_popup_unlink(const AssetShelfType &shelf_type)
{
  for (AssetShelf *shelf : StaticPopupShelves::shelves()) {
    if (shelf->type == &shelf_type) {
      shelf->type = nullptr;
    }
  }
}

static AssetShelf *lookup_shelf_for_popup(const bContext &C, const AssetShelfType &shelf_type)
{
  Vector<AssetShelf *> &popup_shelves = StaticPopupShelves::shelves();

  for (AssetShelf *shelf : popup_shelves) {
    if (STREQ(shelf->idname, shelf_type.idname)) {
      if (type_poll_for_popup(C, ensure_shelf_has_type(*shelf))) {
        return shelf;
      }
      break;
    }
  }

  return nullptr;
}

static AssetShelf *get_shelf_for_popup(const bContext &C, AssetShelfType &shelf_type)
{
  Vector<AssetShelf *> &popup_shelves = StaticPopupShelves::shelves();

  if (AssetShelf *shelf = lookup_shelf_for_popup(C, shelf_type)) {
    return shelf;
  }

  if (type_poll_for_popup(C, &shelf_type)) {
    AssetShelf *new_shelf = create_shelf_from_type(shelf_type);
    new_shelf->settings.display_flag |= ASSETSHELF_SHOW_NAMES;
    /* Increased size of previews, to leave more space for the name. */
    new_shelf->settings.preview_size = ASSET_SHELF_PREVIEW_SIZE_DEFAULT;
    popup_shelves.append(new_shelf);
    return new_shelf;
  }

  return nullptr;
}

void ensure_asset_library_fetched(const bContext &C, const AssetShelfType &shelf_type)
{
  if (AssetShelf *shelf = lookup_shelf_for_popup(C, shelf_type)) {
    list::storage_fetch(&shelf->settings.asset_library_reference, &C);
  }
  else {
    AssetLibraryReference library_ref = asset_system::all_library_reference();
    list::storage_fetch(&library_ref, &C);
  }
}

class AssetCatalogTreeView : public ui::AbstractTreeView {
  AssetShelf &shelf_;
  asset_system::AssetCatalogTree catalog_tree_;

 public:
  AssetCatalogTreeView(const asset_system::AssetLibrary &library, AssetShelf &shelf)
      : shelf_(shelf)
  {
    catalog_tree_ = build_filtered_catalog_tree(
        library,
        shelf_.settings.asset_library_reference,
        [this](const asset_system::AssetRepresentation &asset) {
          return (!shelf_.type->asset_poll || shelf_.type->asset_poll(shelf_.type, &asset));
        });

    /* Keep the popup open when clicking to activate a catalog. */
    this->set_popup_keep_open();
  }

  void build_tree() override
  {
    if (catalog_tree_.is_empty()) {
      auto &item = this->add_tree_item<ui::BasicTreeViewItem>(RPT_("No applicable assets found"),
                                                              ICON_INFO);
      item.disable_interaction();
      return;
    }

    auto &all_item = this->add_tree_item<ui::BasicTreeViewItem>(IFACE_("All"));
    all_item.set_on_activate_fn([this](bContext &C, ui::BasicTreeViewItem &) {
      settings_set_all_catalog_active(shelf_.settings);
      send_redraw_notifier(C);
    });
    all_item.set_is_active_fn(
        [this]() { return settings_is_all_catalog_active(shelf_.settings); });
    all_item.uncollapse_by_default();

    catalog_tree_.foreach_root_item([&, this](
                                        const asset_system::AssetCatalogTreeItem &catalog_item) {
      ui::BasicTreeViewItem &item = this->build_catalog_items_recursive(all_item, catalog_item);
      item.uncollapse_by_default();
    });
  }

  ui::BasicTreeViewItem &build_catalog_items_recursive(
      ui::TreeViewOrItem &parent_view_item,
      const asset_system::AssetCatalogTreeItem &catalog_item) const
  {
    ui::BasicTreeViewItem &view_item = parent_view_item.add_tree_item<ui::BasicTreeViewItem>(
        catalog_item.get_name());

    std::string catalog_path = catalog_item.catalog_path().str();
    view_item.set_on_activate_fn([this, catalog_path](bContext &C, ui::BasicTreeViewItem &) {
      settings_set_active_catalog(shelf_.settings, catalog_path);
      send_redraw_notifier(C);
    });
    view_item.set_is_active_fn([this, catalog_path]() {
      return settings_is_active_catalog(shelf_.settings, catalog_path);
    });

    const int parent_count = view_item.count_parents() + 1;

    catalog_item.foreach_child([&, this](const asset_system::AssetCatalogTreeItem &child) {
      ui::BasicTreeViewItem &child_item = build_catalog_items_recursive(view_item, child);

      /* Uncollapse to some level (gives quick access, but don't let the tree get too big). */
      if (parent_count < 3) {
        child_item.uncollapse_by_default();
      }
    });

    return view_item;
  }
};

static void catalog_tree_draw(const bContext &C, uiLayout &layout, AssetShelf &shelf)
{
  const asset_system::AssetLibrary *library = list::library_get_once_available(
      shelf.settings.asset_library_reference);
  if (!library) {
    return;
  }

  uiBlock *block = layout.block();
  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "asset shelf catalog tree view",
      std::make_unique<AssetCatalogTreeView>(*library, shelf));

  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout);
}

static AssetShelfType *lookup_type_from_idname_in_context(const bContext *C)
{
  const std::optional<StringRefNull> idname = CTX_data_string_get(C, "asset_shelf_idname");
  if (!idname) {
    return nullptr;
  }
  return type_find_from_idname(*idname);
}

constexpr int LEFT_COL_WIDTH_UNITS = 10;
constexpr int RIGHT_COL_WIDTH_UNITS_DEFAULT = 50;

/**
 * Ensure the popover width fits into the window: Clamp width by the window width, minus some
 * padding.
 */
static int layout_width_units_clamped(const wmWindow *win)
{
  const int max_units_x = (WM_window_native_pixel_x(win) / UI_UNIT_X) - 2;
  return std::min(LEFT_COL_WIDTH_UNITS + RIGHT_COL_WIDTH_UNITS_DEFAULT, max_units_x);
}

static void popover_panel_draw(const bContext *C, Panel *panel)
{
  const wmWindow *win = CTX_wm_window(C);
  const int layout_width_units = layout_width_units_clamped(win);
  AssetShelfType *shelf_type = lookup_type_from_idname_in_context(C);
  BLI_assert_msg(shelf_type != nullptr, "couldn't find asset shelf type from context");

  uiLayout *layout = panel->layout;
  layout->ui_units_x_set(layout_width_units);

  AssetShelf *shelf = get_shelf_for_popup(*C, *shelf_type);
  if (!shelf) {
    BLI_assert_unreachable();
    return;
  }

  bScreen *screen = CTX_wm_screen(C);
  PointerRNA library_ref_ptr = RNA_pointer_create_discrete(
      &screen->id, &RNA_AssetLibraryReference, &shelf->settings.asset_library_reference);
  layout->context_ptr_set("asset_library_reference", &library_ref_ptr);

  uiLayout *row = &layout->row(false);
  uiLayout *catalogs_col = &row->column(false);
  catalogs_col->ui_units_x_set(LEFT_COL_WIDTH_UNITS);
  catalogs_col->fixed_size_set(true);
  library_selector_draw(C, catalogs_col, *shelf);
  catalog_tree_draw(*C, *catalogs_col, *shelf);

  uiLayout *right_col = &row->column(false);
  uiLayout *sub = &right_col->row(false);
  /* Same as file/asset browser header. */
  PointerRNA shelf_ptr = RNA_pointer_create_discrete(&screen->id, &RNA_AssetShelf, shelf);
  sub->prop(&shelf_ptr,
            "search_filter",
            /* Force the button to be active in a semi-modal state. */
            UI_ITEM_R_TEXT_BUT_FORCE_SEMI_MODAL_ACTIVE,
            "",
            ICON_VIEWZOOM);

  uiLayout *asset_view_col = &right_col->column(false);
  BLI_assert((layout_width_units - LEFT_COL_WIDTH_UNITS) > 0);
  asset_view_col->ui_units_x_set(layout_width_units - LEFT_COL_WIDTH_UNITS);
  asset_view_col->fixed_size_set(true);

  build_asset_view(*asset_view_col, shelf->settings.asset_library_reference, *shelf, *C);
}

static bool popover_panel_poll(const bContext *C, PanelType * /*panel_type*/)
{
  const AssetShelfType *shelf_type = lookup_type_from_idname_in_context(C);
  if (!shelf_type) {
    return false;
  }

  return type_poll_for_popup(*C, shelf_type);
}

void popover_panel_register(ARegionType *region_type)
{
  /* Uses global paneltype registry to allow usage as popover. So only register this once (may be
   * called from multiple spaces). */
  if (WM_paneltype_find("ASSETSHELF_PT_popover_panel", true)) {
    return;
  }

  PanelType *pt = MEM_callocN<PanelType>(__func__);
  STRNCPY_UTF8(pt->idname, "ASSETSHELF_PT_popover_panel");
  STRNCPY_UTF8(pt->label, N_("Asset Shelf Panel"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->description = N_("Display an asset shelf in a popover panel");
  pt->draw = popover_panel_draw;
  pt->poll = popover_panel_poll;
  pt->listener = asset::list::asset_reading_region_listen_fn;
  /* Move to have first asset item under cursor. */
  pt->offset_units_xy.x = -(LEFT_COL_WIDTH_UNITS + 1.5f);
  /* Offset so mouse is below search button, over the first row of assets. */
  pt->offset_units_xy.y = 2.5f;
  BLI_addtail(&region_type->paneltypes, pt);
  WM_paneltype_add(pt);
}

}  // namespace blender::ed::asset::shelf
