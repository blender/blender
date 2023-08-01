/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "DNA_space_types.h"

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"

#include "BKE_asset.h"

#include "BLI_string_ref.hh"

#include "BLT_translation.h"

#include "ED_asset.h"
#include "ED_fileselect.h"
#include "ED_undo.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"
#include "UI_tree_view.hh"

#include "WM_api.h"
#include "WM_types.h"

#include "file_intern.h"
#include "filelist.h"

#include <fmt/format.h>

using namespace blender;
using namespace blender::asset_system;

namespace blender::ed::asset_browser {

class AssetCatalogTreeViewAllItem;

class AssetCatalogTreeView : public ui::AbstractTreeView {
  ::AssetLibrary *asset_library_;
  /** The asset catalog tree this tree-view represents. */
  asset_system::AssetCatalogTree *catalog_tree_;
  FileAssetSelectParams *params_;
  SpaceFile &space_file_;

  friend class AssetCatalogTreeViewItem;
  friend class AssetCatalogDropTarget;
  friend class AssetCatalogTreeViewAllItem;

 public:
  AssetCatalogTreeView(::AssetLibrary *library,
                       FileAssetSelectParams *params,
                       SpaceFile &space_file);

  void build_tree() override;

  void activate_catalog_by_id(CatalogID catalog_id);

 private:
  ui::BasicTreeViewItem &build_catalog_items_recursive(ui::TreeViewOrItem &view_parent_item,
                                                       AssetCatalogTreeItem &catalog);

  AssetCatalogTreeViewAllItem &add_all_item();
  void add_unassigned_item();
  bool is_active_catalog(CatalogID catalog_id) const;
};

/* ---------------------------------------------------------------------- */

class AssetCatalogTreeViewItem : public ui::BasicTreeViewItem {
  /** The catalog tree item this tree view item represents. */
  AssetCatalogTreeItem &catalog_item_;

 public:
  AssetCatalogTreeViewItem(AssetCatalogTreeItem *catalog_item);

  void on_activate(bContext &C) override;

  void build_row(uiLayout &row) override;
  void build_context_menu(bContext &C, uiLayout &column) const override;

  bool supports_renaming() const override;
  bool rename(StringRefNull new_name) override;

  /** Add drag support for catalog items. */
  std::unique_ptr<ui::AbstractViewItemDragController> create_drag_controller() const override;
  /** Add dropping support for catalog items. */
  std::unique_ptr<ui::TreeViewItemDropTarget> create_drop_target() override;
};

class AssetCatalogDragController : public ui::AbstractViewItemDragController {
  AssetCatalogTreeItem &catalog_item_;

 public:
  explicit AssetCatalogDragController(AssetCatalogTreeView &tree_view,
                                      AssetCatalogTreeItem &catalog_item);

  eWM_DragDataType get_drag_type() const override;
  void *create_drag_data() const override;
  void on_drag_start() override;
};

class AssetCatalogDropTarget : public ui::TreeViewItemDropTarget {
  AssetCatalogTreeItem &catalog_item_;

 public:
  AssetCatalogDropTarget(AssetCatalogTreeView &tree_view, AssetCatalogTreeItem &catalog_item);

  bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
  std::string drop_tooltip(const ui::DragInfo &drag_info) const override;
  bool on_drop(bContext *C, const ui::DragInfo &drag_info) const override;

  ::AssetLibrary &get_asset_library() const;

  static AssetCatalog *get_drag_catalog(const wmDrag &drag, const ::AssetLibrary &asset_library);
  static bool has_droppable_asset(const wmDrag &drag, const char **r_disabled_hint);
  static bool can_modify_catalogs(const ::AssetLibrary &asset_library,
                                  const char **r_disabled_hint);
  static bool drop_assets_into_catalog(bContext *C,
                                       const AssetCatalogTreeView &tree_view,
                                       const wmDrag &drag,
                                       CatalogID catalog_id,
                                       StringRefNull simple_name = "");
  /**
   * \param drop_catalog_id: Can be unset to drop into the root level of the tree.
   */
  static bool drop_asset_catalog_into_catalog(
      const wmDrag &drag,
      AssetCatalogTreeView &tree_view,
      const std::optional<CatalogID> drop_catalog_id = std::nullopt);

 private:
  std::string drop_tooltip_asset_list(const wmDrag &drag) const;
  std::string drop_tooltip_asset_catalog(const wmDrag &drag) const;
};

/** Only reason this isn't just `BasicTreeViewItem` is to add a '+' icon for adding a root level
 * catalog. */
class AssetCatalogTreeViewAllItem : public ui::BasicTreeViewItem {
  using BasicTreeViewItem::BasicTreeViewItem;

  void build_row(uiLayout &row) override;

  struct DropTarget : public ui::TreeViewItemDropTarget {
    DropTarget(AssetCatalogTreeView &tree_view);

    bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
    std::string drop_tooltip(const ui::DragInfo &drag_info) const override;
    bool on_drop(bContext *C, const ui::DragInfo &drag_info) const override;
  };

  std::unique_ptr<ui::TreeViewItemDropTarget> create_drop_target() override;
};

class AssetCatalogTreeViewUnassignedItem : public ui::BasicTreeViewItem {
  using BasicTreeViewItem::BasicTreeViewItem;

  struct DropTarget : public ui::TreeViewItemDropTarget {
    DropTarget(AssetCatalogTreeView &tree_view);

    bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
    std::string drop_tooltip(const ui::DragInfo &drag_info) const override;
    bool on_drop(bContext *C, const ui::DragInfo &drag_info) const override;
  };

  std::unique_ptr<ui::TreeViewItemDropTarget> create_drop_target() override;
};

/* ---------------------------------------------------------------------- */

AssetCatalogTreeView::AssetCatalogTreeView(::AssetLibrary *library,
                                           FileAssetSelectParams *params,
                                           SpaceFile &space_file)
    : asset_library_(library),
      catalog_tree_(AS_asset_library_get_catalog_tree(library)),
      params_(params),
      space_file_(space_file)
{
}

void AssetCatalogTreeView::build_tree()
{
  AssetCatalogTreeViewAllItem &all_item = add_all_item();
  all_item.set_collapsed(false);

  if (catalog_tree_) {
    /* Pass the "All" item on as parent of the actual catalog items. */
    catalog_tree_->foreach_root_item([this, &all_item](AssetCatalogTreeItem &item) {
      build_catalog_items_recursive(all_item, item);
    });
  }

  add_unassigned_item();
}

ui::BasicTreeViewItem &AssetCatalogTreeView::build_catalog_items_recursive(
    ui::TreeViewOrItem &view_parent_item, AssetCatalogTreeItem &catalog)
{
  ui::BasicTreeViewItem &view_item = view_parent_item.add_tree_item<AssetCatalogTreeViewItem>(
      &catalog);
  view_item.set_is_active_fn(
      [this, &catalog]() { return is_active_catalog(catalog.get_catalog_id()); });

  catalog.foreach_child([&view_item, this](AssetCatalogTreeItem &child) {
    build_catalog_items_recursive(view_item, child);
  });
  return view_item;
}

AssetCatalogTreeViewAllItem &AssetCatalogTreeView::add_all_item()
{
  FileAssetSelectParams *params = params_;

  AssetCatalogTreeViewAllItem &item = add_tree_item<AssetCatalogTreeViewAllItem>(IFACE_("All"));
  item.set_on_activate_fn([params](bContext & /*C*/, ui::BasicTreeViewItem & /*item*/) {
    params->asset_catalog_visibility = FILE_SHOW_ASSETS_ALL_CATALOGS;
    WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  });
  item.set_is_active_fn(
      [params]() { return params->asset_catalog_visibility == FILE_SHOW_ASSETS_ALL_CATALOGS; });
  return item;
}

void AssetCatalogTreeView::add_unassigned_item()
{
  FileAssetSelectParams *params = params_;

  AssetCatalogTreeViewUnassignedItem &item = add_tree_item<AssetCatalogTreeViewUnassignedItem>(
      IFACE_("Unassigned"), ICON_FILE_HIDDEN);

  item.set_on_activate_fn([params](bContext & /*C*/, ui::BasicTreeViewItem & /*item*/) {
    params->asset_catalog_visibility = FILE_SHOW_ASSETS_WITHOUT_CATALOG;
    WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
  });
  item.set_is_active_fn(
      [params]() { return params->asset_catalog_visibility == FILE_SHOW_ASSETS_WITHOUT_CATALOG; });
}

void AssetCatalogTreeView::activate_catalog_by_id(CatalogID catalog_id)
{
  params_->asset_catalog_visibility = FILE_SHOW_ASSETS_FROM_CATALOG;
  params_->catalog_id = catalog_id;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
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

void AssetCatalogTreeViewItem::on_activate(bContext & /*C*/)
{
  AssetCatalogTreeView &tree_view = static_cast<AssetCatalogTreeView &>(get_tree_view());
  tree_view.activate_catalog_by_id(catalog_item_.get_catalog_id());
}

void AssetCatalogTreeViewItem::build_row(uiLayout &row)
{
  const std::string label_override = catalog_item_.has_unsaved_changes() ? (label_ + "*") : label_;
  add_label(row, label_override);

  if (!is_hovered()) {
    return;
  }

  uiButViewItem *view_item_but = view_item_button();
  PointerRNA *props;

  props = UI_but_extra_operator_icon_add(
      (uiBut *)view_item_but, "ASSET_OT_catalog_new", WM_OP_INVOKE_DEFAULT, ICON_ADD);
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
              UI_ITEM_NONE,
              &props);
  RNA_string_set(&props, "parent_path", catalog_item_.catalog_path().c_str());

  char catalog_id_str_buffer[UUID_STRING_SIZE] = "";
  BLI_uuid_format(catalog_id_str_buffer, catalog_item_.get_catalog_id());
  uiItemFullO(&column,
              "ASSET_OT_catalog_delete",
              "Delete Catalog",
              ICON_NONE,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &props);
  RNA_string_set(&props, "catalog_id", catalog_id_str_buffer);
  uiItemO(&column, "Rename", ICON_NONE, "UI_OT_view_item_rename");

  /* Doesn't actually exist right now, but could be defined in Python. Reason that this isn't done
   * in Python yet is that catalogs are not exposed in BPY, and we'd somehow pass the clicked on
   * catalog to the menu draw callback (via context probably). */
  MenuType *mt = WM_menutype_find("ASSETBROWSER_MT_catalog_context_menu", true);
  if (!mt) {
    return;
  }
  UI_menutype_draw(&C, mt, &column);
}

bool AssetCatalogTreeViewItem::supports_renaming() const
{
  const AssetCatalogTreeView &tree_view = static_cast<const AssetCatalogTreeView &>(
      get_tree_view());
  return !ED_asset_catalogs_read_only(*tree_view.asset_library_);
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

std::unique_ptr<ui::TreeViewItemDropTarget> AssetCatalogTreeViewItem::create_drop_target()
{
  return std::make_unique<AssetCatalogDropTarget>(
      static_cast<AssetCatalogTreeView &>(get_tree_view()), catalog_item_);
}

std::unique_ptr<ui::AbstractViewItemDragController> AssetCatalogTreeViewItem::
    create_drag_controller() const
{
  return std::make_unique<AssetCatalogDragController>(
      static_cast<AssetCatalogTreeView &>(get_tree_view()), catalog_item_);
}

/* ---------------------------------------------------------------------- */

AssetCatalogDropTarget::AssetCatalogDropTarget(AssetCatalogTreeView &tree_view,
                                               AssetCatalogTreeItem &catalog_item)
    : ui::TreeViewItemDropTarget(tree_view), catalog_item_(catalog_item)
{
}

bool AssetCatalogDropTarget::can_drop(const wmDrag &drag, const char **r_disabled_hint) const
{
  if (drag.type == WM_DRAG_ASSET_CATALOG) {
    const ::AssetLibrary &library = get_asset_library();
    if (!can_modify_catalogs(library, r_disabled_hint)) {
      return false;
    }

    const AssetCatalog *drag_catalog = get_drag_catalog(drag, library);
    /* NOTE: Technically it's not an issue to allow this (the catalog will just receive a new
     * path and the catalog system will generate missing parents from the path). But it does
     * appear broken to users, so disabling entirely. */
    if (catalog_item_.catalog_path().is_contained_in(drag_catalog->path)) {
      *r_disabled_hint = TIP_("Catalog cannot be dropped into itself");
      return false;
    }
    if (catalog_item_.catalog_path() == drag_catalog->path.parent()) {
      *r_disabled_hint = TIP_("Catalog is already placed inside this catalog");
      return false;
    }
    return true;
  }

  if (drag.type == WM_DRAG_ASSET_LIST && has_droppable_asset(drag, r_disabled_hint)) {
    return true;
  }
  return false;
}

std::string AssetCatalogDropTarget::drop_tooltip(const ui::DragInfo &drag_info) const
{
  if (drag_info.drag_data.type == WM_DRAG_ASSET_CATALOG) {
    return drop_tooltip_asset_catalog(drag_info.drag_data);
  }
  return drop_tooltip_asset_list(drag_info.drag_data);
}

std::string AssetCatalogDropTarget::drop_tooltip_asset_catalog(const wmDrag &drag) const
{
  BLI_assert(drag.type == WM_DRAG_ASSET_CATALOG);
  const AssetCatalog *src_catalog = get_drag_catalog(drag, get_asset_library());

  return fmt::format(TIP_("Move catalog {} into {}"),
                     std::string_view(src_catalog->path.name()),
                     std::string_view(catalog_item_.get_name()));
}

std::string AssetCatalogDropTarget::drop_tooltip_asset_list(const wmDrag &drag) const
{
  BLI_assert(drag.type == WM_DRAG_ASSET_LIST);

  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  const bool is_multiple_assets = !BLI_listbase_is_single(asset_drags);

  /* Don't try to be smart by dynamically adding the 's' for the plural. Just makes translation
   * harder, so use full literals. */
  std::string basic_tip = is_multiple_assets ? TIP_("Move assets to catalog") :
                                               TIP_("Move asset to catalog");

  basic_tip += ": " + catalog_item_.get_name();

  /* Display the full catalog path, but only if it's not exactly the same as the already shown name
   * (i.e. not a root level catalog with no parent). */
  if (catalog_item_.get_name() != catalog_item_.catalog_path().str()) {
    basic_tip += " (" + catalog_item_.catalog_path().str() + ")";
  }

  return basic_tip;
}

bool AssetCatalogDropTarget::on_drop(bContext *C, const ui::DragInfo &drag) const
{
  if (drag.drag_data.type == WM_DRAG_ASSET_CATALOG) {
    return drop_asset_catalog_into_catalog(
        drag.drag_data, get_view<AssetCatalogTreeView>(), catalog_item_.get_catalog_id());
  }
  return drop_assets_into_catalog(C,
                                  get_view<AssetCatalogTreeView>(),
                                  drag.drag_data,
                                  catalog_item_.get_catalog_id(),
                                  catalog_item_.get_simple_name());
}

bool AssetCatalogDropTarget::drop_asset_catalog_into_catalog(
    const wmDrag &drag,
    AssetCatalogTreeView &tree_view,
    const std::optional<CatalogID> drop_catalog_id)
{
  BLI_assert(drag.type == WM_DRAG_ASSET_CATALOG);
  wmDragAssetCatalog *catalog_drag = WM_drag_get_asset_catalog_data(&drag);
  ED_asset_catalog_move(tree_view.asset_library_, catalog_drag->drag_catalog_id, drop_catalog_id);
  tree_view.activate_catalog_by_id(catalog_drag->drag_catalog_id);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_CATALOGS, nullptr);
  return true;
}

bool AssetCatalogDropTarget::drop_assets_into_catalog(bContext *C,
                                                      const AssetCatalogTreeView &tree_view,
                                                      const wmDrag &drag,
                                                      CatalogID catalog_id,
                                                      StringRefNull simple_name)
{
  BLI_assert(drag.type == WM_DRAG_ASSET_LIST);
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);
  if (!asset_drags) {
    return false;
  }

  bool did_update = false;
  LISTBASE_FOREACH (wmDragAssetListItem *, asset_item, asset_drags) {
    if (asset_item->is_external) {
      /* Only internal assets can be modified! */
      continue;
    }

    did_update = true;
    BKE_asset_metadata_catalog_id_set(
        asset_item->asset_data.local_id->asset_data, catalog_id, simple_name.c_str());

    /* Trigger re-run of filtering to update visible assets. */
    filelist_tag_needs_filtering(tree_view.space_file_.files);
    file_select_deselect_all(&tree_view.space_file_, FILE_SEL_SELECTED | FILE_SEL_HIGHLIGHTED);
    WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  }

  if (did_update) {
    ED_undo_push(C, "Assign Asset Catalog");
  }
  return true;
}

AssetCatalog *AssetCatalogDropTarget::get_drag_catalog(const wmDrag &drag,
                                                       const ::AssetLibrary &asset_library)
{
  if (drag.type != WM_DRAG_ASSET_CATALOG) {
    return nullptr;
  }
  const AssetCatalogService *catalog_service = AS_asset_library_get_catalog_service(
      &asset_library);
  const wmDragAssetCatalog *catalog_drag = WM_drag_get_asset_catalog_data(&drag);

  return catalog_service->find_catalog(catalog_drag->drag_catalog_id);
}

bool AssetCatalogDropTarget::has_droppable_asset(const wmDrag &drag, const char **r_disabled_hint)
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag);

  *r_disabled_hint = nullptr;
  /* There needs to be at least one asset from the current file. */
  LISTBASE_FOREACH (const wmDragAssetListItem *, asset_item, asset_drags) {
    if (!asset_item->is_external) {
      return true;
    }
  }

  *r_disabled_hint = TIP_("Only assets from this current file can be moved between catalogs");
  return false;
}

bool AssetCatalogDropTarget::can_modify_catalogs(const ::AssetLibrary &library,
                                                 const char **r_disabled_hint)
{
  if (ED_asset_catalogs_read_only(library)) {
    *r_disabled_hint = TIP_("Catalogs cannot be edited in this asset library");
    return false;
  }
  return true;
}

::AssetLibrary &AssetCatalogDropTarget::get_asset_library() const
{
  return *get_view<AssetCatalogTreeView>().asset_library_;
}

/* ---------------------------------------------------------------------- */

AssetCatalogDragController::AssetCatalogDragController(AssetCatalogTreeView &tree_view,
                                                       AssetCatalogTreeItem &catalog_item)
    : ui::AbstractViewItemDragController(tree_view), catalog_item_(catalog_item)
{
}

eWM_DragDataType AssetCatalogDragController::get_drag_type() const
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

void AssetCatalogDragController::on_drag_start()
{
  AssetCatalogTreeView &tree_view_ = get_view<AssetCatalogTreeView>();
  tree_view_.activate_catalog_by_id(catalog_item_.get_catalog_id());
}

/* ---------------------------------------------------------------------- */

void AssetCatalogTreeViewAllItem::build_row(uiLayout &row)
{
  ui::BasicTreeViewItem::build_row(row);

  PointerRNA *props;

  UI_but_extra_operator_icon_add(
      (uiBut *)view_item_button(), "ASSET_OT_catalogs_save", WM_OP_INVOKE_DEFAULT, ICON_FILE_TICK);

  props = UI_but_extra_operator_icon_add(
      (uiBut *)view_item_button(), "ASSET_OT_catalog_new", WM_OP_INVOKE_DEFAULT, ICON_ADD);
  /* No parent path to use the root level. */
  RNA_string_set(props, "parent_path", nullptr);
}

std::unique_ptr<ui::TreeViewItemDropTarget> AssetCatalogTreeViewAllItem::create_drop_target()
{
  return std::make_unique<AssetCatalogTreeViewAllItem::DropTarget>(
      static_cast<AssetCatalogTreeView &>(get_tree_view()));
}

AssetCatalogTreeViewAllItem::DropTarget::DropTarget(AssetCatalogTreeView &tree_view)
    : ui::TreeViewItemDropTarget(tree_view)
{
}

bool AssetCatalogTreeViewAllItem::DropTarget::can_drop(const wmDrag &drag,
                                                       const char **r_disabled_hint) const
{
  if (drag.type != WM_DRAG_ASSET_CATALOG) {
    return false;
  }
  ::AssetLibrary &library = *get_view<AssetCatalogTreeView>().asset_library_;
  if (!AssetCatalogDropTarget::can_modify_catalogs(library, r_disabled_hint)) {
    return false;
  }

  const AssetCatalog *drag_catalog = AssetCatalogDropTarget::get_drag_catalog(drag, library);
  if (drag_catalog->path.parent() == "") {
    *r_disabled_hint = TIP_("Catalog is already placed at the highest level");
    return false;
  }

  return true;
}

std::string AssetCatalogTreeViewAllItem::DropTarget::drop_tooltip(
    const ui::DragInfo &drag_info) const
{
  BLI_assert(drag_info.drag_data.type == WM_DRAG_ASSET_CATALOG);
  const AssetCatalog *drag_catalog = AssetCatalogDropTarget::get_drag_catalog(
      drag_info.drag_data, *get_view<AssetCatalogTreeView>().asset_library_);

  return fmt::format(TIP_("Move catalog {} to the top level of the tree"),
                     std::string_view(drag_catalog->path.name()));
}

bool AssetCatalogTreeViewAllItem::DropTarget::on_drop(bContext * /*C*/,
                                                      const ui::DragInfo &drag) const
{
  BLI_assert(drag.drag_data.type == WM_DRAG_ASSET_CATALOG);
  return AssetCatalogDropTarget::drop_asset_catalog_into_catalog(
      drag.drag_data,
      get_view<AssetCatalogTreeView>(),
      /* No value to drop into the root level. */
      std::nullopt);
}

/* ---------------------------------------------------------------------- */

std::unique_ptr<ui::TreeViewItemDropTarget> AssetCatalogTreeViewUnassignedItem::
    create_drop_target()
{
  return std::make_unique<AssetCatalogTreeViewUnassignedItem::DropTarget>(
      static_cast<AssetCatalogTreeView &>(get_tree_view()));
}

AssetCatalogTreeViewUnassignedItem::DropTarget::DropTarget(AssetCatalogTreeView &tree_view)
    : ui::TreeViewItemDropTarget(tree_view)
{
}

bool AssetCatalogTreeViewUnassignedItem::DropTarget::can_drop(const wmDrag &drag,
                                                              const char **r_disabled_hint) const
{
  if (drag.type != WM_DRAG_ASSET_LIST) {
    return false;
  }
  return AssetCatalogDropTarget::has_droppable_asset(drag, r_disabled_hint);
}

std::string AssetCatalogTreeViewUnassignedItem::DropTarget::drop_tooltip(
    const ui::DragInfo &drag_info) const
{
  const ListBase *asset_drags = WM_drag_asset_list_get(&drag_info.drag_data);
  const bool is_multiple_assets = !BLI_listbase_is_single(asset_drags);

  return is_multiple_assets ? TIP_("Move assets out of any catalog") :
                              TIP_("Move asset out of any catalog");
}

bool AssetCatalogTreeViewUnassignedItem::DropTarget::on_drop(bContext *C,
                                                             const ui::DragInfo &drag) const
{
  /* Assign to nil catalog ID. */
  return AssetCatalogDropTarget::drop_assets_into_catalog(
      C, get_view<AssetCatalogTreeView>(), drag.drag_data, CatalogID{});
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
  AssetCatalogFilterSettings *filter_settings = MEM_new<AssetCatalogFilterSettings>(__func__);
  return reinterpret_cast<FileAssetCatalogFilterSettingsHandle *>(filter_settings);
}

void file_delete_asset_catalog_filter_settings(
    FileAssetCatalogFilterSettingsHandle **filter_settings_handle)
{
  AssetCatalogFilterSettings **filter_settings = reinterpret_cast<AssetCatalogFilterSettings **>(
      filter_settings_handle);
  MEM_delete(*filter_settings);
  *filter_settings = nullptr;
}

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
      !BLI_uuid_equal(filter_settings->asset_catalog_id, catalog_id))
  {
    filter_settings->asset_catalog_id = catalog_id;
    needs_update = true;
  }

  return needs_update;
}

void file_ensure_updated_catalog_filter_data(
    FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    const asset_system::AssetLibrary *asset_library)
{
  AssetCatalogFilterSettings *filter_settings = reinterpret_cast<AssetCatalogFilterSettings *>(
      filter_settings_handle);
  const AssetCatalogService *catalog_service = asset_library->catalog_service.get();

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

  ui::TreeViewBuilder::build_tree_view(*tree_view, *layout);
}
