/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Catalog tree-view to enable/disable catalogs in the asset shelf settings.
 */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"

#include "BLI_string.h"

#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "ED_asset_filter.hh"
#include "ED_asset_list.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_tree_view.hh"

#include "WM_api.hh"

#include "asset_shelf.hh"

namespace blender::ed::asset::shelf {

class AssetCatalogSelectorTree : public ui::AbstractTreeView {
  AssetShelf &shelf_;
  AssetShelfSettings &shelf_settings_;
  asset_system::AssetCatalogTree catalog_tree_;

 public:
  class Item;

  AssetCatalogSelectorTree(asset_system::AssetLibrary &library, AssetShelf &shelf)
      : shelf_(shelf), shelf_settings_(shelf_.settings)
  {
    catalog_tree_ = build_filtered_catalog_tree(
        library,
        shelf_settings_.asset_library_reference,
        [this](const asset_system::AssetRepresentation &asset) {
          return (!shelf_.type->asset_poll || shelf_.type->asset_poll(shelf_.type, &asset));
        });
  }

  void build_tree() override
  {
    if (catalog_tree_.is_empty()) {
      auto &item = add_tree_item<ui::BasicTreeViewItem>(RPT_("No applicable assets found"),
                                                        ICON_INFO);
      item.disable_interaction();
      return;
    }

    catalog_tree_.foreach_root_item(
        [this](const asset_system::AssetCatalogTreeItem &catalog_item) {
          Item &item = build_catalog_items_recursive(*this, catalog_item);
          item.uncollapse_by_default();
        });
  }

  Item &build_catalog_items_recursive(ui::TreeViewOrItem &parent_view_item,
                                      const asset_system::AssetCatalogTreeItem &catalog_item) const
  {
    Item &view_item = parent_view_item.add_tree_item<Item>(catalog_item, shelf_settings_);

    catalog_item.foreach_child(
        [&view_item, this](const asset_system::AssetCatalogTreeItem &child) {
          build_catalog_items_recursive(view_item, child);
        });

    return view_item;
  }

  void update_shelf_settings_from_enabled_catalogs();

  class Item : public ui::BasicTreeViewItem {
    const asset_system::AssetCatalogTreeItem &catalog_item_;
    /* Is the catalog path enabled in this redraw? Set on construction, updated by the UI (which
     * gets a pointer to it). The UI needs it as char. */
    char catalog_path_enabled_ = false;

   public:
    Item(const asset_system::AssetCatalogTreeItem &catalog_item,
         AssetShelfSettings &shelf_settings)
        : ui::BasicTreeViewItem(catalog_item.get_name()),
          catalog_item_(catalog_item),
          catalog_path_enabled_(
              settings_is_catalog_path_enabled(shelf_settings, catalog_item.catalog_path()))
    {
      disable_activatable();
    }

    bool is_catalog_path_enabled() const
    {
      return catalog_path_enabled_ != 0;
    }

    bool has_enabled_in_subtree()
    {
      bool has_enabled = false;

      foreach_item_recursive(
          [&has_enabled](const ui::AbstractTreeViewItem &abstract_item) {
            const Item &item = dynamic_cast<const Item &>(abstract_item);
            if (item.is_catalog_path_enabled()) {
              has_enabled = true;
            }
          },
          IterOptions::SkipFiltered);

      return has_enabled;
    }

    asset_system::AssetCatalogPath catalog_path() const
    {
      return catalog_item_.catalog_path();
    }

    void build_row(uiLayout &row) override
    {
      AssetCatalogSelectorTree &tree = dynamic_cast<AssetCatalogSelectorTree &>(get_tree_view());
      uiBlock *block = uiLayoutGetBlock(&row);

      uiLayoutSetEmboss(&row, UI_EMBOSS);

      uiLayout *subrow = uiLayoutRow(&row, false);
      uiLayoutSetActive(subrow, catalog_path_enabled_);
      uiItemL(subrow, catalog_item_.get_name().c_str(), ICON_NONE);
      UI_block_layout_set_current(block, &row);

      uiBut *toggle_but = uiDefButC(block,
                                    UI_BTYPE_CHECKBOX,
                                    0,
                                    "",
                                    0,
                                    0,
                                    UI_UNIT_X,
                                    UI_UNIT_Y,
                                    (char *)&catalog_path_enabled_,
                                    0,
                                    0,
                                    TIP_("Toggle catalog visibility in the asset shelf"));
      UI_but_func_set(toggle_but, [&tree](bContext &C) {
        tree.update_shelf_settings_from_enabled_catalogs();
        send_redraw_notifier(C);
      });
      if (!is_catalog_path_enabled() && has_enabled_in_subtree()) {
        UI_but_drawflag_enable(toggle_but, UI_BUT_INDETERMINATE);
      }
      UI_but_flag_disable(toggle_but, UI_BUT_UNDO);
    }
  };
};

void AssetCatalogSelectorTree::update_shelf_settings_from_enabled_catalogs()
{
  settings_clear_enabled_catalogs(shelf_settings_);
  foreach_item([this](ui::AbstractTreeViewItem &view_item) {
    const auto &selector_tree_item = dynamic_cast<AssetCatalogSelectorTree::Item &>(view_item);
    if (selector_tree_item.is_catalog_path_enabled()) {
      settings_set_catalog_path_enabled(shelf_settings_, selector_tree_item.catalog_path());
    }
  });
}

static void catalog_selector_panel_draw(const bContext *C, Panel *panel)
{
  const AssetLibraryReference *library_ref = CTX_wm_asset_library_ref(C);
  AssetShelf *shelf = active_shelf_from_context(C);
  if (!shelf) {
    return;
  }

  uiLayout *layout = panel->layout;
  uiBlock *block = uiLayoutGetBlock(layout);

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  PointerRNA shelf_ptr = RNA_pointer_create(&CTX_wm_screen(C)->id, &RNA_AssetShelf, shelf);

  uiLayout *row = uiLayoutRow(layout, true);
  uiItemR(row, &shelf_ptr, "asset_library_reference", UI_ITEM_NONE, "", ICON_NONE);
  if (library_ref->type != ASSET_LIBRARY_LOCAL) {
    uiItemO(row, "", ICON_FILE_REFRESH, "ASSET_OT_library_refresh");
  }

  asset_system::AssetLibrary *library = list::library_get_once_available(*library_ref);
  if (!library) {
    return;
  }

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "asset catalog tree view",
      std::make_unique<AssetCatalogSelectorTree>(*library, *shelf));

  ui::TreeViewBuilder::build_tree_view(*tree_view, *layout);
}

void catalog_selector_panel_register(ARegionType *region_type)
{
  /* Uses global paneltype registry to allow usage as popover. So only register this once (may be
   * called from multiple spaces). */
  if (WM_paneltype_find("ASSETSHELF_PT_catalog_selector", true)) {
    return;
  }

  PanelType *pt = MEM_cnew<PanelType>(__func__);
  STRNCPY(pt->idname, "ASSETSHELF_PT_catalog_selector");
  STRNCPY(pt->label, N_("Catalog Selector"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->description = N_(
      "Select the asset library and the contained catalogs to display in the asset shelf");
  pt->draw = catalog_selector_panel_draw;
  pt->listener = asset::list::asset_reading_region_listen_fn;
  BLI_addtail(&region_type->paneltypes, pt);
  WM_paneltype_add(pt);
}

}  // namespace blender::ed::asset::shelf
