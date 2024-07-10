/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "asset_shelf.hh"

#include "BKE_screen.hh"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "UI_interface_c.hh"
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

static AssetShelf *get_shelf_for_popup(const bContext &C, AssetShelfType &shelf_type)
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

  if (type_poll_for_popup(C, &shelf_type)) {
    AssetShelf *new_shelf = create_shelf_from_type(shelf_type);
    new_shelf->settings.display_flag |= ASSETSHELF_SHOW_NAMES;
    popup_shelves.append(new_shelf);
    return new_shelf;
  }

  return nullptr;
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

static void catalog_tree_draw(uiLayout &layout, AssetShelf &shelf)
{
  const asset_system::AssetLibrary *library = list::library_get_once_available(
      shelf.settings.asset_library_reference);
  if (!library) {
    return;
  }

  uiBlock *block = uiLayoutGetBlock(&layout);
  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "asset shelf catalog tree view",
      std::make_unique<AssetCatalogTreeView>(*library, shelf));

  ui::TreeViewBuilder::build_tree_view(*tree_view, layout);
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
constexpr int RIGHT_COL_WIDTH_UNITS = 30;
constexpr int LAYOUT_WIDTH_UNITS = LEFT_COL_WIDTH_UNITS + RIGHT_COL_WIDTH_UNITS;

static void popover_panel_draw(const bContext *C, Panel *panel)
{
  AssetShelfType *shelf_type = lookup_type_from_idname_in_context(C);
  BLI_assert_msg(shelf_type != nullptr, "couldn't find asset shelf type from context");

  const ARegion *region = CTX_wm_region_popup(C) ? CTX_wm_region_popup(C) : CTX_wm_region(C);

  uiLayout *layout = panel->layout;
  uiLayoutSetUnitsX(layout, LAYOUT_WIDTH_UNITS);

  AssetShelf *shelf = get_shelf_for_popup(*C, *shelf_type);
  if (!shelf) {
    BLI_assert_unreachable();
    return;
  }

  bScreen *screen = CTX_wm_screen(C);
  PointerRNA library_ref_ptr = RNA_pointer_create(
      &screen->id, &RNA_AssetLibraryReference, &shelf->settings.asset_library_reference);
  uiLayoutSetContextPointer(layout, "asset_library_reference", &library_ref_ptr);

  uiLayout *row = uiLayoutRow(layout, false);
  uiLayout *catalogs_col = uiLayoutColumn(row, false);
  uiLayoutSetUnitsX(catalogs_col, LEFT_COL_WIDTH_UNITS);
  uiLayoutSetFixedSize(catalogs_col, true);
  library_selector_draw(C, catalogs_col, *shelf);
  catalog_tree_draw(*catalogs_col, *shelf);

  uiLayout *right_col = uiLayoutColumn(row, false);
  uiLayout *sub = uiLayoutRow(right_col, false);
  /* Same as file/asset browser header. */
  PointerRNA shelf_ptr = RNA_pointer_create(&screen->id, &RNA_AssetShelf, shelf);
  uiItemR(sub,
          &shelf_ptr,
          "search_filter",
          /* Force the button to be active in a semi-modal state. */
          UI_ITEM_R_TEXT_BUT_FORCE_SEMI_MODAL_ACTIVE,
          "",
          ICON_VIEWZOOM);

  uiLayout *asset_view_col = uiLayoutColumn(right_col, false);
  uiLayoutSetUnitsX(asset_view_col, RIGHT_COL_WIDTH_UNITS);
  uiLayoutSetFixedSize(asset_view_col, true);

  build_asset_view(*asset_view_col, shelf->settings.asset_library_reference, *shelf, *C, *region);
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

  PanelType *pt = MEM_cnew<PanelType>(__func__);
  STRNCPY(pt->idname, "ASSETSHELF_PT_popover_panel");
  STRNCPY(pt->label, N_("Asset Shelf Panel"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
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
