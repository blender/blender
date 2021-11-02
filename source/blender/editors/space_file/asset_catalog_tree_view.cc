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

#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.hh"

#include "BLI_string_ref.hh"

#include "BLT_translation.h"

#include "ED_asset.h"
#include "ED_fileselect.h"

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
  ::AssetLibrary *asset_library_;
  /** The asset catalog tree this tree-view represents. */
  bke::AssetCatalogTree *catalog_tree_;
  FileAssetSelectParams *params_;
  SpaceFile &space_file_;

  friend class AssetCatalogTreeViewItem;
  friend class AssetCatalogDropController;

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
  void build_context_menu(bContext &C, uiLayout &column) const override;

  bool can_rename() const override;
  bool rename(StringRefNull new_name) override;

  /** Add drag support for catalog items. */
  std::unique_ptr<ui::AbstractTreeViewItemDragController> create_drag_controller() const override;
  /** Add dropping support for catalog items. */
  std::unique_ptr<ui::AbstractTreeViewItemDropController> create_drop_controller() const override;
};

class AssetCatalogDragController : public ui::AbstractTreeViewItemDragController {
  AssetCatalogTreeItem &catalog_item_;

 public:
  explicit AssetCatalogDragController(AssetCatalogTreeItem &catalog_item);

  int get_drag_type() const override;
  void *create_drag_data() const override;
};

class AssetCatalogDropController : public ui::AbstractTreeViewItemDropController {
  AssetCatalogTreeItem &catalog_item_;

 public:
  AssetCatalogDropController(AssetCatalogTreeView &tree_view, AssetCatalogTreeItem &catalog_item);

  bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
  std::string drop_tooltip(const wmDrag &drag) const override;
  bool on_drop(const wmDrag &drag) override;

  ::AssetLibrary &get_asset_library() const;

  static bool has_droppable_asset(const wmDrag &drag, const char **r_disabled_hint);
  static bool drop_assets_into_catalog(const AssetCatalogTreeView &tree_view,
                                       const wmDrag &drag,
                                       CatalogID catalog_id,
                                       StringRefNull simple_name = "");

 private:
  bool drop_asset_catalog_into_catalog(const wmDrag &drag);
  std::string drop_tooltip_asset_list(const wmDrag &drag) const;
  std::string drop_tooltip_asset_catalog(const wmDrag &drag) const;
};

/** Only reason this isn't just `BasicTreeViewItem` is to add a '+' icon for adding a root level
 * catalog. */
class AssetCatalogTreeViewAllItem : public ui::BasicTreeViewItem {
  using BasicTreeViewItem::BasicTreeViewItem;

  void build_row(uiLayout &row) override;
};

class AssetCatalogTreeViewUnassignedItem : public ui::BasicTreeViewItem {
  using BasicTreeViewItem::BasicTreeViewItem;

  struct DropController : public ui::AbstractTreeViewItemDropController {
    DropController(AssetCatalogTreeView &tree_view);

    bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
    std::string drop_tooltip(const wmDrag &drag) const override;
    bool on_drop(const wmDrag &drag) override;
  };

  std::unique_ptr<ui::AbstractTreeViewItemDropController> create_drop_controller() const override;
};

/* ---------------------------------------------------------------------- */

AssetCatalogTreeView::AssetCatalogTreeView(::AssetLibrary *library,
                                           FileAssetSelectParams *params,
                                           SpaceFile &space_file)
    : asset_library_(library),
      catalog_tree_(BKE_asset_library_get_catalog_tree(library)),
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
  view_item.is_active([this, &catalog]() { return is_active_catalog(catalog.get_catalog_id()); });

  catalog.foreach_child([&view_item, this](AssetCatalogTreeItem &child) {
    build_catalog_items_recursive(view_item, child);
  });
  return view_item;
}

void AssetCatalogTreeView::add_all_item()
{
  FileAssetSelectParams *params = params_;

  AssetCatalogTreeViewAllItem &item = add_tree_item<AssetCatalogTreeViewAllItem>(IFACE_("All"),
                                                                                 ICON_HOME);
  item.on_activate([params](ui::BasicTreeViewItem & /*item*/) {
    params->asset_catalog_visibility = FILE_SHOW_ASSETS_ALL_CATALOGS;
    WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  });
  item.is_active(
      [params]() { return params->asset_catalog_visibility == FILE_SHOW_ASSETS_ALL_CATALOGS; });
}

void AssetCatalogTreeView::add_unassigned_item()
{
  FileAssetSelectParams *params = params_;

  AssetCatalogTreeViewUnassignedItem &item = add_tree_item<AssetCatalogTreeViewUnassignedItem>(
      IFACE_("Unassigned"), ICON_FILE_HIDDEN);

  item.on_activate([params](ui::BasicTreeViewItem & /*item*/) {
    params->asset_catalog_visibility = FILE_SHOW_ASSETS_WITHOUT_CATALOG;
    WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  });
  item.is_active(
      [params]() { return params->asset_catalog_visibility == FILE_SHOW_ASSETS_WITHOUT_CATALOG; });
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
  const std::string label_override = catalog_item_.has_unsaved_changes() ? (label_ + "*") : label_;
  add_label(row, label_override);

  if (!is_hovered()) {
    return;
  }

  uiButTreeRow *tree_row_but = tree_row_button();
  PointerRNA *props;

  props = UI_but_extra_operator_icon_add(
      (uiBut *)tree_row_but, "ASSET_OT_catalog_new", WM_OP_INVOKE_DEFAULT, ICON_ADD);
  RNA_string_set(props, "parent_path", catalog_item_.catalog_path().c_str());
}

void AssetCatalogTreeViewItem::build_context_menu(bContext &C, uiLayout &column) const
{
  PointerRNA props;

  uiItemFullO(&column,
              "ASSET_OT_catalog_new",
              "New Catalog",
              ICON_NONE,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              &props);
  RNA_string_set(&props, "parent_path", catalog_item_.catalog_path().c_str());

  char catalog_id_str_buffer[UUID_STRING_LEN] = "";
  BLI_uuid_format(catalog_id_str_buffer, catalog_item_.get_catalog_id());
  uiItemFullO(&column,
              "ASSET_OT_catalog_delete",
              "Delete Catalog",
              ICON_NONE,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              &props);
  RNA_string_set(&props, "catalog_id", catalog_id_str_buffer);
  uiItemO(&column, "Rename", ICON_NONE, "UI_OT_tree_view_item_rename");

  /* Doesn't actually exist right now, but could be defined in Python. Reason that this isn't done
   * in Python yet is that catalogs are not exposed in BPY, and we'd somehow pass the clicked on
   * catalog to the menu draw callback (via context probably).*/
  MenuType *mt = WM_menutype_find("ASSETBROWSER_MT_catalog_context_menu", true);
  if (!mt) {
    return;
  }
  UI_menutype_draw(&C, mt, &column);
}

bool AssetCatalogTreeViewItem::can_rename() const
{
  return true;
}

bool AssetCatalogTreeViewItem::rename(StringRefNull new_name)
{
  /* Important to keep state. */
  BasicTreeViewItem::rename(new_name);

  const AssetCatalogTreeView &tree_view = static_cast<const AssetCatalogTreeView &>(
      get_tree_view());
  ED_asset_catalog_rename(tree_view.asset_library_, catalog_item_.get_catalog_id(), new_name);
  return true;
}

std::unique_ptr<ui::AbstractTreeViewItemDropController> AssetCatalogTreeViewItem::
    create_drop_controller() const
{
  return std::make_unique<AssetCatalogDropController>(
      static_cast<AssetCatalogTreeView &>(get_tree_view()), catalog_item_);
}

std::unique_ptr<ui::AbstractTreeViewItemDragController> AssetCatalogTreeViewItem::
    create_drag_controller() const
{
  return std::make_unique<AssetCatalogDragController>(catalog_item_);
}

/* ---------------------------------------------------------------------- */

AssetCatalogDropController::AssetCatalogDropController(AssetCatalogTreeView &tree_view,
                                                       AssetCatalogTreeItem &catalog_item)
    : ui::AbstractTreeViewItemDropController(tree_view), catalog_item_(catalog_item)
{
}

bool AssetCatalogDropController::can_drop(const wmDrag &drag, const char **r_disabled_hint) const
{
  if (drag.type == WM_DRAG_ASSET_CATALOG) {
    /* Always supported. */
    return true;
  }
  if (drag.type == WM_DRAG_ASSET_LIST) {
    return has_droppable_asset(drag, r_disabled_hint);
  }
  return false;
}

std::string AssetCatalogDropController::drop_tooltip(const wmDrag &drag) const
{
  if (drag.type == WM_DRAG_ASSET_CATALOG) {
    return drop_tooltip_asset_catalog(drag);
  }
  return drop_tooltip_asset_list(drag);
}

std::string AssetCatalogDropController::drop_tooltip_asset_catalog(const wmDrag &drag) const
{
  BLI_assert(drag.type == WM_DRAG_ASSET_CATALOG);

  const ::AssetLibrary *asset_library = tree_view<AssetCatalogTreeView>().asset_library_;
  bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(asset_library);
  wmDragAssetCatalog *catalog_drag = WM_drag_get_asset_catalog_data(&drag);
  AssetCatalog *src_catalog = catalog_service->find_catalog(catalog_drag->drag_catalog_id);

  return std::string(TIP_("Move Catalog")) + " '" + src_catalog->path.name() + "' " +
         TIP_("into") + " '" + catalog_item_.get_name() + "'";
}

std::string AssetCatalogDropController::drop_tooltip_asset_list(const wmDrag &drag) const
{
  BLI_assert(drag.type == WM_DRAG_ASSET_LIST);

  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  const bool is_multiple_assets = !BLI_listbase_is_single(asset_drags);

  /* Don't try to be smart by dynamically adding the 's' for the plural. Just makes translation
   * harder, so use full literals. */
  std::string basic_tip = is_multiple_assets ? TIP_("Move assets to catalog") :
                                               TIP_("Move asset to catalog");

  return basic_tip + ": " + catalog_item_.get_name() + " (" + catalog_item_.catalog_path().str() +
         ")";
}

bool AssetCatalogDropController::on_drop(const wmDrag &drag)
{
  if (drag.type == WM_DRAG_ASSET_CATALOG) {
    return drop_asset_catalog_into_catalog(drag);
  }
  return drop_assets_into_catalog(tree_view<AssetCatalogTreeView>(),
                                  drag,
                                  catalog_item_.get_catalog_id(),
                                  catalog_item_.get_simple_name());
}

bool AssetCatalogDropController::drop_asset_catalog_into_catalog(const wmDrag &drag)
{
  BLI_assert(drag.type == WM_DRAG_ASSET_CATALOG);
  wmDragAssetCatalog *catalog_drag = WM_drag_get_asset_catalog_data(&drag);
  ED_asset_catalog_move(
      &get_asset_library(), catalog_drag->drag_catalog_id, catalog_item_.get_catalog_id());

  WM_main_add_notifier(NC_ASSET | ND_ASSET_CATALOGS, nullptr);
  return true;
}

bool AssetCatalogDropController::drop_assets_into_catalog(const AssetCatalogTreeView &tree_view,
                                                          const wmDrag &drag,
                                                          CatalogID catalog_id,
                                                          StringRefNull simple_name)
{
  BLI_assert(drag.type == WM_DRAG_ASSET_LIST);
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  if (!asset_drags) {
    return false;
  }

  LISTBASE_FOREACH (wmDragAssetListItem *, asset_item, asset_drags) {
    if (asset_item->is_external) {
      /* Only internal assets can be modified! */
      continue;
    }
    BKE_asset_metadata_catalog_id_set(
        asset_item->asset_data.local_id->asset_data, catalog_id, simple_name.c_str());

    /* Trigger re-run of filtering to update visible assets. */
    filelist_tag_needs_filtering(tree_view.space_file_.files);
    file_select_deselect_all(&tree_view.space_file_, FILE_SEL_SELECTED | FILE_SEL_HIGHLIGHTED);
    WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  }

  return true;
}

bool AssetCatalogDropController::has_droppable_asset(const wmDrag &drag,
                                                     const char **r_disabled_hint)
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);

  *r_disabled_hint = nullptr;
  /* There needs to be at least one asset from the current file. */
  LISTBASE_FOREACH (const wmDragAssetListItem *, asset_item, asset_drags) {
    if (!asset_item->is_external) {
      return true;
    }
  }

  *r_disabled_hint = "Only assets from this current file can be moved between catalogs";
  return false;
}

::AssetLibrary &AssetCatalogDropController::get_asset_library() const
{
  return *tree_view<AssetCatalogTreeView>().asset_library_;
}

/* ---------------------------------------------------------------------- */

AssetCatalogDragController::AssetCatalogDragController(AssetCatalogTreeItem &catalog_item)
    : catalog_item_(catalog_item)
{
}

int AssetCatalogDragController::get_drag_type() const
{
  return WM_DRAG_ASSET_CATALOG;
}

void *AssetCatalogDragController::create_drag_data() const
{
  wmDragAssetCatalog *drag_catalog = (wmDragAssetCatalog *)MEM_callocN(sizeof(*drag_catalog),
                                                                       __func__);
  drag_catalog->drag_catalog_id = catalog_item_.get_catalog_id();
  return drag_catalog;
}

/* ---------------------------------------------------------------------- */

void AssetCatalogTreeViewAllItem::build_row(uiLayout &row)
{
  ui::BasicTreeViewItem::build_row(row);

  PointerRNA *props;

  UI_but_extra_operator_icon_add(
      (uiBut *)tree_row_button(), "ASSET_OT_catalogs_save", WM_OP_INVOKE_DEFAULT, ICON_FILE_TICK);

  props = UI_but_extra_operator_icon_add(
      (uiBut *)tree_row_button(), "ASSET_OT_catalog_new", WM_OP_INVOKE_DEFAULT, ICON_ADD);
  /* No parent path to use the root level. */
  RNA_string_set(props, "parent_path", nullptr);
}

/* ---------------------------------------------------------------------- */

std::unique_ptr<ui::AbstractTreeViewItemDropController> AssetCatalogTreeViewUnassignedItem::
    create_drop_controller() const
{
  return std::make_unique<AssetCatalogTreeViewUnassignedItem::DropController>(
      static_cast<AssetCatalogTreeView &>(get_tree_view()));
}

AssetCatalogTreeViewUnassignedItem::DropController::DropController(AssetCatalogTreeView &tree_view)
    : ui::AbstractTreeViewItemDropController(tree_view)
{
}

bool AssetCatalogTreeViewUnassignedItem::DropController::can_drop(
    const wmDrag &drag, const char **r_disabled_hint) const
{
  if (drag.type != WM_DRAG_ASSET_LIST) {
    return false;
  }
  return AssetCatalogDropController::has_droppable_asset(drag, r_disabled_hint);
}

std::string AssetCatalogTreeViewUnassignedItem::DropController::drop_tooltip(
    const wmDrag &drag) const
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  const bool is_multiple_assets = !BLI_listbase_is_single(asset_drags);

  return is_multiple_assets ? TIP_("Move assets out of any catalog") :
                              TIP_("Move asset out of any catalog");
}

bool AssetCatalogTreeViewUnassignedItem::DropController::on_drop(const wmDrag &drag)
{
  /* Assign to nil catalog ID. */
  return AssetCatalogDropController::drop_assets_into_catalog(
      tree_view<AssetCatalogTreeView>(), drag, CatalogID{});
}

}  // namespace blender::ed::asset_browser

/* ---------------------------------------------------------------------- */

namespace blender::ed::asset_browser {

class AssetCatalogFilterSettings {
 public:
  eFileSel_Params_AssetCatalogVisibility asset_catalog_visibility;
  bUUID asset_catalog_id;

  std::unique_ptr<AssetCatalogFilter> catalog_filter;
};

}  // namespace blender::ed::asset_browser

using namespace blender::ed::asset_browser;

FileAssetCatalogFilterSettingsHandle *file_create_asset_catalog_filter_settings()
{
  AssetCatalogFilterSettings *filter_settings = OBJECT_GUARDED_NEW(AssetCatalogFilterSettings);
  return reinterpret_cast<FileAssetCatalogFilterSettingsHandle *>(filter_settings);
}

void file_delete_asset_catalog_filter_settings(
    FileAssetCatalogFilterSettingsHandle **filter_settings_handle)
{
  AssetCatalogFilterSettings **filter_settings = reinterpret_cast<AssetCatalogFilterSettings **>(
      filter_settings_handle);
  OBJECT_GUARDED_SAFE_DELETE(*filter_settings, AssetCatalogFilterSettings);
}

/**
 * \return True if the file list should update its filtered results (e.g. because filtering
 *         parameters changed).
 */
bool file_set_asset_catalog_filter_settings(
    FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    eFileSel_Params_AssetCatalogVisibility catalog_visibility,
    ::bUUID catalog_id)
{
  AssetCatalogFilterSettings *filter_settings = reinterpret_cast<AssetCatalogFilterSettings *>(
      filter_settings_handle);
  bool needs_update = false;

  if (filter_settings->asset_catalog_visibility != catalog_visibility) {
    filter_settings->asset_catalog_visibility = catalog_visibility;
    needs_update = true;
  }

  if (filter_settings->asset_catalog_visibility == FILE_SHOW_ASSETS_FROM_CATALOG &&
      !BLI_uuid_equal(filter_settings->asset_catalog_id, catalog_id)) {
    filter_settings->asset_catalog_id = catalog_id;
    needs_update = true;
  }

  return needs_update;
}

void file_ensure_updated_catalog_filter_data(
    FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    const ::AssetLibrary *asset_library)
{
  AssetCatalogFilterSettings *filter_settings = reinterpret_cast<AssetCatalogFilterSettings *>(
      filter_settings_handle);
  const AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(
      asset_library);

  if (filter_settings->asset_catalog_visibility != FILE_SHOW_ASSETS_ALL_CATALOGS) {
    filter_settings->catalog_filter = std::make_unique<AssetCatalogFilter>(
        catalog_service->create_catalog_filter(filter_settings->asset_catalog_id));
  }
}

bool file_is_asset_visible_in_catalog_filter_settings(
    const FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    const AssetMetaData *asset_data)
{
  const AssetCatalogFilterSettings *filter_settings =
      reinterpret_cast<const AssetCatalogFilterSettings *>(filter_settings_handle);

  switch (filter_settings->asset_catalog_visibility) {
    case FILE_SHOW_ASSETS_WITHOUT_CATALOG:
      return !filter_settings->catalog_filter->is_known(asset_data->catalog_id);
    case FILE_SHOW_ASSETS_FROM_CATALOG:
      return filter_settings->catalog_filter->contains(asset_data->catalog_id);
    case FILE_SHOW_ASSETS_ALL_CATALOGS:
      /* All asset files should be visible. */
      return true;
  }

  BLI_assert_unreachable();
  return false;
}

/* ---------------------------------------------------------------------- */

void file_create_asset_catalog_tree_view_in_layout(::AssetLibrary *asset_library,
                                                   uiLayout *layout,
                                                   SpaceFile *space_file,
                                                   FileAssetSelectParams *params)
{
  uiBlock *block = uiLayoutGetBlock(layout);

  UI_block_layout_set_current(block, layout);

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "asset catalog tree view",
      std::make_unique<ed::asset_browser::AssetCatalogTreeView>(
          asset_library, params, *space_file));

  ui::TreeViewBuilder builder(*block);
  builder.build_tree_view(*tree_view);
}
