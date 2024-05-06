/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "asset_shelf.hh"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "UI_interface_c.hh"
#include "UI_tree_view.hh"

#include "ED_asset_filter.hh"
#include "ED_asset_list.hh"
#include "ED_asset_shelf.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

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

static AssetShelf *get_shelf_for_popup(const bContext *C, AssetShelfType &shelf_type)
{
  Vector<AssetShelf *> &popup_shelves = StaticPopupShelves::shelves();

  for (AssetShelf *shelf : popup_shelves) {
    if (STREQ(shelf->idname, shelf_type.idname)) {
      if (type_poll_for_popup(*C, ensure_shelf_has_type(*shelf))) {
        return shelf;
      }
      break;
    }
  }

  if (type_poll_for_popup(*C, &shelf_type)) {
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

    catalog_item.foreach_child(
        [&view_item, this](const asset_system::AssetCatalogTreeItem &child) {
          build_catalog_items_recursive(view_item, child);
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

uiBlock *popup_block_create(const bContext *C, ARegion *region, AssetShelfType *shelf_type)
{
  bScreen *screen = CTX_wm_screen(C);
  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_POPOVER);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  AssetShelf *shelf = get_shelf_for_popup(C, *shelf_type);
  if (!shelf) {
    BLI_assert_unreachable();
    return block;
  }

  const uiStyle *style = UI_style_get_dpi();

  const int layout_width = UI_UNIT_X * 40;
  const int left_col_width = 10 * UI_UNIT_X;
  const int right_col_width = layout_width - left_col_width;
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, layout_width, 0, 0, style);

  PointerRNA library_ref_ptr = RNA_pointer_create(
      &screen->id, &RNA_AssetLibraryReference, &shelf->settings.asset_library_reference);
  uiLayoutSetContextPointer(layout, "asset_library_reference", &library_ref_ptr);

  uiLayout *row = uiLayoutRow(layout, false);
  uiLayout *catalogs_col = uiLayoutColumn(row, false);
  uiLayoutSetUnitsX(catalogs_col, left_col_width / UI_UNIT_X);
  uiLayoutSetFixedSize(catalogs_col, true);
  library_selector_draw(C, catalogs_col, *shelf);
  catalog_tree_draw(*catalogs_col, *shelf);

  uiLayout *right_col = uiLayoutColumn(row, false);
  uiLayout *sub = uiLayoutRow(right_col, false);
  /* Same as file/asset browser header. */
  PointerRNA shelf_ptr = RNA_pointer_create(&screen->id, &RNA_AssetShelf, shelf);
  uiItemR(sub, &shelf_ptr, "search_filter", UI_ITEM_R_IMMEDIATE, "", ICON_VIEWZOOM);

  uiLayout *asset_view_col = uiLayoutColumn(right_col, false);
  uiLayoutSetUnitsX(asset_view_col, right_col_width / UI_UNIT_X);
  uiLayoutSetFixedSize(asset_view_col, true);
  build_asset_view(*asset_view_col, shelf->settings.asset_library_reference, *shelf, *C, *region);

  return block;
}

}  // namespace blender::ed::asset::shelf
