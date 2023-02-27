/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Catalog tree-view to enable/disable catalogs in the asset shelf settings.
 */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"

#include "BKE_context.h"

#include "BLT_translation.h"

#include "ED_asset_list.hh"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_tree_view.hh"

#include "asset_shelf.hh"

using namespace blender;
using namespace blender::ed::asset;

namespace blender::ed::asset::shelf {

class AssetCatalogSelectorTree : public ui::AbstractTreeView {
  asset_system::AssetLibrary &library_;
  asset_system::AssetCatalogTree *catalog_tree_;
  AssetShelfSettings &shelf_settings_;

 public:
  class Item;

  AssetCatalogSelectorTree(asset_system::AssetLibrary &library, AssetShelfSettings &shelf_settings)
      : library_(library), shelf_settings_(shelf_settings)
  {
    asset_system::AssetCatalogService *catalog_service = library_.catalog_service.get();
    catalog_tree_ = catalog_service->get_catalog_tree();
  }

  void build_tree() override
  {
    if (!catalog_tree_) {
      return;
    }

    catalog_tree_->foreach_root_item([this](asset_system::AssetCatalogTreeItem &catalog_item) {
      build_catalog_items_recursive(*this, catalog_item);
    });
  }

  Item &build_catalog_items_recursive(ui::TreeViewOrItem &parent_view_item,
                                      asset_system::AssetCatalogTreeItem &catalog_item) const
  {
    Item &view_item = parent_view_item.add_tree_item<Item>(catalog_item, shelf_settings_);

    catalog_item.foreach_child([&view_item, this](asset_system::AssetCatalogTreeItem &child) {
      build_catalog_items_recursive(view_item, child);
    });

    return view_item;
  }

  void update_shelf_settings_from_enabled_catalogs();

  class Item : public ui::BasicTreeViewItem {
    asset_system::AssetCatalogTreeItem catalog_item_;
    /* Is the catalog path enabled in this redraw? Set on construction, updated by the UI (which
     * gets a pointer to it). The UI needs it as char. */
    char catalog_path_enabled_ = false;

   public:
    Item(asset_system::AssetCatalogTreeItem &catalog_item, AssetShelfSettings &shelf_settings)
        : ui::BasicTreeViewItem(catalog_item.get_name()),
          catalog_item_(catalog_item),
          catalog_path_enabled_(
              settings_is_catalog_path_enabled(shelf_settings, catalog_item.catalog_path()))
    {
    }

    bool is_catalog_path_enabled() const
    {
      return catalog_path_enabled_ != 0;
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

      if (!is_collapsible()) {
        uiItemL(&row, nullptr, ICON_BLANK1);
      }

      uiBut *but = uiDefButC(block,
                             UI_BTYPE_CHECKBOX,
                             0,
                             catalog_item_.get_name().c_str(),
                             0,
                             0,
                             UI_UNIT_X * 10,
                             UI_UNIT_Y,
                             (char *)&catalog_path_enabled_,
                             0,
                             0,
                             0,
                             0,
                             TIP_("Toggle catalog visibility in the asset shelf"));
      UI_but_func_set(but, [&tree](bContext &C) {
        tree.update_shelf_settings_from_enabled_catalogs();
        send_redraw_notifier(C);
      });
      UI_but_flag_disable(but, UI_BUT_UNDO);
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

uiBlock *catalog_selector_block_draw(bContext *C, ARegion *region, void * /*arg1*/)
{
  const AssetLibraryReference *library_ref = CTX_wm_asset_library_ref(C);
  asset_system::AssetLibrary *library = ED_assetlist_library_get_once_available(*library_ref);
  AssetShelfSettings *shelf_settings = settings_from_context(C);

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block,
                       UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPUP_CAN_REFRESH);

  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     0,
                                     0,
                                     UI_UNIT_X * 12,
                                     UI_UNIT_Y,
                                     0,
                                     UI_style_get());

  uiItemL(layout, "Enable Catalogs", ICON_NONE);
  uiItemS(layout);

  uiLayoutSetEmboss(layout, UI_EMBOSS_NONE);
  if (library && shelf_settings) {
    ui::AbstractTreeView *tree_view = UI_block_add_view(
        *block,
        "asset catalog tree view",
        std::make_unique<AssetCatalogSelectorTree>(*library, *shelf_settings));

    ui::TreeViewBuilder builder(*block);
    builder.build_tree_view(*tree_view);
  }

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_UP);

  return block;
}

}  // namespace blender::ed::asset::shelf
