/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spfile
 */

#include "ED_fileselect.h"

#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.hh"

#include "BLI_string_ref.hh"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"
#include "UI_tree_view.hh"

#include "WM_api.h"
#include "WM_types.h"

#include "file_intern.h"
#include "filelist.h"

using namespace blender;
using namespace blender::bke;

namespace blender::ed::asset_browser {

class AssetCatalogTreeView : public ui::AbstractTreeView {
  /** The asset catalog tree this tree-view represents. */
  bke::AssetCatalogTree *catalog_tree_;
  FileAssetSelectParams *params_;
  SpaceFile &space_file_;

  friend class AssetCatalogTreeViewItem;

 public:
  AssetCatalogTreeView(::AssetLibrary *library,
                       FileAssetSelectParams *params,
                       SpaceFile &space_file);

  void build_tree() override;

 private:
  ui::BasicTreeViewItem &build_catalog_items_recursive(ui::TreeViewItemContainer &view_parent_item,
                                                       AssetCatalogTreeItem &catalog);

  void add_all_item();
  void add_unassigned_item();
  bool is_active_catalog(CatalogID catalog_id) const;
};

/* ---------------------------------------------------------------------- */

class AssetCatalogTreeViewItem : public ui::BasicTreeViewItem {
  /** The catalog tree item this tree view item represents. */
  AssetCatalogTreeItem &catalog_item_;

 public:
  AssetCatalogTreeViewItem(AssetCatalogTreeItem *catalog_item);

  void on_activate() override;

  void build_row(uiLayout &row) override;

  bool has_droppable_item(const wmDrag &drag) const;

  bool can_drop(const wmDrag &drag) const override;
  std::string drop_tooltip(const bContext &C,
                           const wmDrag &drag,
                           const wmEvent &event) const override;
  bool on_drop(const wmDrag &drag) override;
};

/** Only reason this isn't just `BasicTreeViewItem` is to add a '+' icon for adding a root level
 * catalog. */
class AssetCatalogTreeViewAllItem : public ui::BasicTreeViewItem {
  using BasicTreeViewItem::BasicTreeViewItem;

  void build_row(uiLayout &row) override;
};

/* ---------------------------------------------------------------------- */

AssetCatalogTreeView::AssetCatalogTreeView(::AssetLibrary *library,
                                           FileAssetSelectParams *params,
                                           SpaceFile &space_file)
    : catalog_tree_(BKE_asset_library_get_catalog_tree(library)),
      params_(params),
      space_file_(space_file)
{
}

void AssetCatalogTreeView::build_tree()
{
  add_all_item();

  if (catalog_tree_) {
    catalog_tree_->foreach_root_item([this](AssetCatalogTreeItem &item) {
      ui::BasicTreeViewItem &child_view_item = build_catalog_items_recursive(*this, item);

      /* Open root-level items by default. */
      child_view_item.set_collapsed(false);
    });
  }

  add_unassigned_item();
}

ui::BasicTreeViewItem &AssetCatalogTreeView::build_catalog_items_recursive(
    ui::TreeViewItemContainer &view_parent_item, AssetCatalogTreeItem &catalog)
{
  ui::BasicTreeViewItem &view_item = view_parent_item.add_tree_item<AssetCatalogTreeViewItem>(
      &catalog);
  if (is_active_catalog(catalog.get_catalog_id())) {
    view_item.set_active();
  }

  catalog.foreach_child([&view_item, this](AssetCatalogTreeItem &child) {
    build_catalog_items_recursive(view_item, child);
  });
  return view_item;
}

void AssetCatalogTreeView::add_all_item()
{
  FileAssetSelectParams *params = params_;

  ui::AbstractTreeViewItem &item = add_tree_item<AssetCatalogTreeViewAllItem>(
      IFACE_("All"), ICON_HOME, [params](ui::BasicTreeViewItem & /*item*/) {
        params->asset_catalog_visibility = FILE_SHOW_ASSETS_ALL_CATALOGS;
        WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
      });
  if (params->asset_catalog_visibility == FILE_SHOW_ASSETS_ALL_CATALOGS) {
    item.set_active();
  }
}

void AssetCatalogTreeView::add_unassigned_item()
{
  FileAssetSelectParams *params = params_;

  ui::AbstractTreeViewItem &item = add_tree_item<ui::BasicTreeViewItem>(
      IFACE_("Unassigned"), ICON_FILE_HIDDEN, [params](ui::BasicTreeViewItem & /*item*/) {
        params->asset_catalog_visibility = FILE_SHOW_ASSETS_WITHOUT_CATALOG;
        WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
      });
  if (params->asset_catalog_visibility == FILE_SHOW_ASSETS_WITHOUT_CATALOG) {
    item.set_active();
  }
}

bool AssetCatalogTreeView::is_active_catalog(CatalogID catalog_id) const
{
  return (params_->asset_catalog_visibility == FILE_SHOW_ASSETS_FROM_CATALOG) &&
         (params_->catalog_id == catalog_id);
}

/* ---------------------------------------------------------------------- */

AssetCatalogTreeViewItem::AssetCatalogTreeViewItem(AssetCatalogTreeItem *catalog_item)
    : BasicTreeViewItem(catalog_item->get_name()), catalog_item_(*catalog_item)
{
}

void AssetCatalogTreeViewItem::on_activate()
{
  const AssetCatalogTreeView &tree_view = static_cast<const AssetCatalogTreeView &>(
      get_tree_view());
  tree_view.params_->asset_catalog_visibility = FILE_SHOW_ASSETS_FROM_CATALOG;
  tree_view.params_->catalog_id = catalog_item_.get_catalog_id();
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

void AssetCatalogTreeViewItem::build_row(uiLayout &row)
{
  ui::BasicTreeViewItem::build_row(row);

  if (!is_active()) {
    return;
  }

  PointerRNA *props;
  const CatalogID catalog_id = catalog_item_.get_catalog_id();

  props = UI_but_extra_operator_icon_add(
      button(), "ASSET_OT_catalog_new", WM_OP_INVOKE_DEFAULT, ICON_ADD);
  RNA_string_set(props, "parent_path", catalog_item_.catalog_path().c_str());

  /* Tree items without a catalog ID represent components of catalog paths that are not
   * associated with an actual catalog. They exist merely by the presence of a child catalog, and
   * thus cannot be deleted themselves. */
  if (!BLI_uuid_is_nil(catalog_id)) {
    char catalog_id_str_buffer[UUID_STRING_LEN] = "";
    BLI_uuid_format(catalog_id_str_buffer, catalog_id);

    props = UI_but_extra_operator_icon_add(
        button(), "ASSET_OT_catalog_delete", WM_OP_INVOKE_DEFAULT, ICON_X);
    RNA_string_set(props, "catalog_id", catalog_id_str_buffer);
  }
}

bool AssetCatalogTreeViewItem::has_droppable_item(const wmDrag &drag) const
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);

  /* There needs to be at least one asset from the current file. */
  LISTBASE_FOREACH (const wmDragAssetListItem *, asset_item, asset_drags) {
    if (!asset_item->is_external) {
      return true;
    }
  }
  return false;
}

bool AssetCatalogTreeViewItem::can_drop(const wmDrag &drag) const
{
  if (drag.type != WM_DRAG_ASSET_LIST) {
    return false;
  }
  return has_droppable_item(drag);
}

std::string AssetCatalogTreeViewItem::drop_tooltip(const bContext & /*C*/,
                                                   const wmDrag &drag,
                                                   const wmEvent & /*event*/) const
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  const bool is_multiple_assets = !BLI_listbase_is_single(asset_drags);

  /* Don't try to be smart by dynamically adding the 's' for the plural. Just makes translation
   * harder, so use full literals. */
  std::string basic_tip = is_multiple_assets ? TIP_("Move assets to catalog") :
                                               TIP_("Move asset to catalog");

  return basic_tip + ": " + catalog_item_.get_name() + " (" + catalog_item_.catalog_path().str() +
         ")";
}

bool AssetCatalogTreeViewItem::on_drop(const wmDrag &drag)
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  if (!asset_drags) {
    return false;
  }

  const AssetCatalogTreeView &tree_view = static_cast<const AssetCatalogTreeView &>(
      get_tree_view());

  LISTBASE_FOREACH (wmDragAssetListItem *, asset_item, asset_drags) {
    if (asset_item->is_external) {
      /* Only internal assets can be modified! */
      continue;
    }
    BKE_asset_metadata_catalog_id_set(asset_item->asset_data.local_id->asset_data,
                                      catalog_item_.get_catalog_id(),
                                      catalog_item_.get_simple_name().c_str());

    /* Trigger re-run of filtering to update visible assets. */
    filelist_tag_needs_filtering(tree_view.space_file_.files);
    file_select_deselect_all(&tree_view.space_file_, FILE_SEL_SELECTED | FILE_SEL_HIGHLIGHTED);
  }

  return true;
}

/* ---------------------------------------------------------------------- */

void AssetCatalogTreeViewAllItem::build_row(uiLayout &row)
{
  ui::BasicTreeViewItem::build_row(row);

  if (!is_active()) {
    return;
  }

  PointerRNA *props;
  props = UI_but_extra_operator_icon_add(
      button(), "ASSET_OT_catalog_new", WM_OP_INVOKE_DEFAULT, ICON_ADD);
  /* No parent path to use the root level. */
  RNA_string_set(props, "parent_path", nullptr);
}

}  // namespace blender::ed::asset_browser

/* ---------------------------------------------------------------------- */

void file_create_asset_catalog_tree_view_in_layout(::AssetLibrary *asset_library,
                                                   uiLayout *layout,
                                                   SpaceFile *space_file,
                                                   FileAssetSelectParams *params)
{
  uiBlock *block = uiLayoutGetBlock(layout);

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "asset catalog tree view",
      std::make_unique<ed::asset_browser::AssetCatalogTreeView>(
          asset_library, params, *space_file));

  ui::TreeViewBuilder builder(*block);
  builder.build_tree_view(*tree_view);
}
