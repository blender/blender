/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_multi_value_map.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.hh"
#include "BKE_idprop.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "ED_asset.h"
#include "ED_screen.h"

#include "node_intern.hh"

namespace blender::ed::space_node {

static bool node_add_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  return CTX_wm_space_node(C);
}

static void node_add_menu_assets_listen_fn(const wmRegionListenerParams *params)
{
  const wmNotifier *wmn = params->notifier;
  ARegion *region = params->region;

  switch (wmn->category) {
    case NC_ASSET:
      if (wmn->data == ND_ASSET_LIST_READING) {
        ED_region_tag_refresh_ui(region);
      }
      break;
  }
}

struct LibraryAsset {
  AssetLibraryReference library_ref;
  AssetHandle handle;
};

struct LibraryCatalog {
  bke::AssetLibrary *library;
  /* Catalog pointers are not save to store. Use the catalog ID instead and lookup the catalog when
   * needed. */
  const bke::CatalogID catalog_id;
};

struct AssetItemTree {
  bke::AssetCatalogTree catalogs;
  MultiValueMap<bke::AssetCatalogPath, LibraryAsset> assets_per_path;
  Map<const bke::AssetCatalogTreeItem *, bke::AssetCatalogPath> full_catalog_per_tree_item;
};

static bool all_loading_finished()
{
  for (const AssetLibraryReference &library : bke::all_valid_asset_library_refs()) {
    if (!ED_assetlist_is_loaded(&library)) {
      return false;
    }
  }
  return true;
}

static AssetItemTree build_catalog_tree(const bContext &C, const bNodeTree *node_tree)
{
  if (!node_tree) {
    return {};
  }
  const Main &bmain = *CTX_data_main(&C);
  const Vector<AssetLibraryReference> all_libraries = bke::all_valid_asset_library_refs();

  /* Merge catalogs from all libraries to deduplicate menu items. Also store the catalog and
   * library for each asset ID in order to use them later when retrieving assets and removing
   * empty catalogs.  */
  Map<bke::CatalogID, LibraryCatalog> id_to_catalog_map;
  bke::AssetCatalogTree catalogs_from_all_libraries;
  for (const AssetLibraryReference &library_ref : all_libraries) {
    if (bke::AssetLibrary *library = BKE_asset_library_load(&bmain, library_ref)) {
      if (bke::AssetCatalogTree *tree = library->catalog_service->get_catalog_tree()) {
        tree->foreach_item([&](bke::AssetCatalogTreeItem &item) {
          const bke::CatalogID &id = item.get_catalog_id();
          bke::AssetCatalog *catalog = library->catalog_service->find_catalog(id);
          catalogs_from_all_libraries.insert_item(*catalog);
          id_to_catalog_map.add(item.get_catalog_id(), LibraryCatalog{library, id});
        });
      }
    }
  }

  /* Find all the matching node group assets for every catalog path. */
  MultiValueMap<bke::AssetCatalogPath, LibraryAsset> assets_per_path;
  for (const AssetLibraryReference &library_ref : all_libraries) {
    AssetFilterSettings type_filter{};
    type_filter.id_types = FILTER_ID_NT;

    ED_assetlist_storage_fetch(&library_ref, &C);
    ED_assetlist_ensure_previews_job(&library_ref, &C);
    ED_assetlist_iterate(library_ref, [&](AssetHandle asset) {
      if (!ED_asset_filter_matches_asset(&type_filter, &asset)) {
        return true;
      }
      const AssetMetaData &meta_data = *ED_asset_handle_get_metadata(&asset);
      const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
      if (tree_type == nullptr || IDP_Int(tree_type) != node_tree->type) {
        return true;
      }
      if (BLI_uuid_is_nil(meta_data.catalog_id)) {
        return true;
      }
      const LibraryCatalog *library_catalog = id_to_catalog_map.lookup_ptr(meta_data.catalog_id);
      if (library_catalog == nullptr) {
        return true;
      }
      const bke::AssetCatalog *catalog = library_catalog->library->catalog_service->find_catalog(
          library_catalog->catalog_id);
      assets_per_path.add(catalog->path, LibraryAsset{library_ref, asset});
      return true;
    });
  }

  /* Build the final tree without any of the catalogs that don't have proper node group assets. */
  bke::AssetCatalogTree catalogs_with_node_assets;
  catalogs_from_all_libraries.foreach_item([&](bke::AssetCatalogTreeItem &item) {
    if (!assets_per_path.lookup(item.catalog_path()).is_empty()) {
      const bke::CatalogID &id = item.get_catalog_id();
      const LibraryCatalog &library_catalog = id_to_catalog_map.lookup(id);
      bke::AssetCatalog *catalog = library_catalog.library->catalog_service->find_catalog(id);
      catalogs_with_node_assets.insert_item(*catalog);
    }
  });

  /* Build another map storing full asset paths for each tree item, in order to have stable
   * pointers to asset catalog paths to use for context pointers. This is necessary because
   * #bke::AssetCatalogTreeItem doesn't store its full path directly. */
  Map<const bke::AssetCatalogTreeItem *, bke::AssetCatalogPath> full_catalog_per_tree_item;
  catalogs_with_node_assets.foreach_item([&](bke::AssetCatalogTreeItem &item) {
    full_catalog_per_tree_item.add_new(&item, item.catalog_path());
  });

  return {std::move(catalogs_with_node_assets),
          std::move(assets_per_path),
          std::move(full_catalog_per_tree_item)};
}

static void node_add_catalog_assets_draw(const bContext *C, Menu *menu)
{
  bScreen &screen = *CTX_wm_screen(C);
  const SpaceNode &snode = *CTX_wm_space_node(C);
  if (!snode.runtime->assets_for_menu) {
    BLI_assert_unreachable();
    return;
  }
  AssetItemTree &tree = *snode.runtime->assets_for_menu;
  const bNodeTree *edit_tree = snode.edittree;
  if (!edit_tree) {
    return;
  }

  const PointerRNA menu_path_ptr = CTX_data_pointer_get(C, "asset_catalog_path");
  if (RNA_pointer_is_null(&menu_path_ptr)) {
    return;
  }
  const bke::AssetCatalogPath &menu_path = *static_cast<const bke::AssetCatalogPath *>(
      menu_path_ptr.data);

  const Span<LibraryAsset> asset_items = tree.assets_per_path.lookup(menu_path);
  bke::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(menu_path);
  BLI_assert(catalog_item != nullptr);

  if (asset_items.is_empty() && !catalog_item->has_children()) {
    return;
  }

  uiLayout *layout = menu->layout;
  uiItemS(layout);

  for (const LibraryAsset &item : asset_items) {
    uiLayout *col = uiLayoutColumn(layout, false);
    PointerRNA file{
        &screen.id, &RNA_FileSelectEntry, const_cast<FileDirEntry *>(item.handle.file_data)};
    uiLayoutSetContextPointer(col, "active_file", &file);

    PointerRNA library_ptr{&screen.id,
                           &RNA_AssetLibraryReference,
                           const_cast<AssetLibraryReference *>(&item.library_ref)};
    uiLayoutSetContextPointer(col, "asset_library_ref", &library_ptr);

    uiItemO(col, ED_asset_handle_get_name(&item.handle), ICON_NONE, "NODE_OT_add_group_asset");
  }

  catalog_item->foreach_child([&](bke::AssetCatalogTreeItem &child_item) {
    const bke::AssetCatalogPath &path = tree.full_catalog_per_tree_item.lookup(&child_item);
    PointerRNA path_ptr{
        &screen.id, &RNA_AssetCatalogPath, const_cast<bke::AssetCatalogPath *>(&path)};
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
    uiItemM(col, "NODE_MT_node_add_catalog_assets", path.name().c_str(), ICON_NONE);
  });
}

static void add_root_catalogs_draw(const bContext *C, Menu *menu)
{
  bScreen &screen = *CTX_wm_screen(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  const bNodeTree *edit_tree = snode.edittree;
  uiLayout *layout = menu->layout;

  snode.runtime->assets_for_menu = std::make_shared<AssetItemTree>(
      build_catalog_tree(*C, edit_tree));

  const bool loading_finished = all_loading_finished();

  AssetItemTree &tree = *snode.runtime->assets_for_menu;
  if (tree.catalogs.is_empty() && loading_finished) {
    return;
  }

  uiItemS(layout);

  if (!loading_finished) {
    uiItemL(layout, IFACE_("Loading Asset Libraries"), ICON_INFO);
  }

  /* Avoid adding a separate root catalog when the assets have already been added to one of the
   * builtin menus.
   * TODO: The need to define the builtin menu labels here is completely non-ideal. We don't have
   * any UI introspection that can do this though. This can be solved in the near future by
   * removing the need to define the add menu completely, instead using a per-node-type path which
   * can be merged with catalog tree.
   */
  static Set<std::string> all_builtin_menus = []() {
    Set<std::string> menus;
    menus.add_new("Attribute");
    menus.add_new("Color");
    menus.add_new("Curve");
    menus.add_new("Curve Primitives");
    menus.add_new("Curve Topology");
    menus.add_new("Geometry");
    menus.add_new("Input");
    menus.add_new("Instances");
    menus.add_new("Material");
    menus.add_new("Mesh");
    menus.add_new("Mesh Primitives");
    menus.add_new("Mesh Topology");
    menus.add_new("Output");
    menus.add_new("Point");
    menus.add_new("Text");
    menus.add_new("Texture");
    menus.add_new("Utilities");
    menus.add_new("UV");
    menus.add_new("Vector");
    menus.add_new("Volume");
    menus.add_new("Group");
    menus.add_new("Layout");
    return menus;
  }();

  tree.catalogs.foreach_root_item([&](bke::AssetCatalogTreeItem &item) {
    if (all_builtin_menus.contains(item.get_name())) {
      return;
    }
    const bke::AssetCatalogPath &path = tree.full_catalog_per_tree_item.lookup(&item);
    PointerRNA path_ptr{
        &screen.id, &RNA_AssetCatalogPath, const_cast<bke::AssetCatalogPath *>(&path)};
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
    uiItemM(col, "NODE_MT_node_add_catalog_assets", path.name().c_str(), ICON_NONE);
  });
}

MenuType add_catalog_assets_menu_type()
{
  MenuType type{};
  BLI_strncpy(type.idname, "NODE_MT_node_add_catalog_assets", sizeof(type.idname));
  type.poll = node_add_menu_poll;
  type.draw = node_add_catalog_assets_draw;
  type.listener = node_add_menu_assets_listen_fn;
  return type;
}

MenuType add_root_catalogs_menu_type()
{
  MenuType type{};
  BLI_strncpy(type.idname, "NODE_MT_node_add_root_catalogs", sizeof(type.idname));
  type.poll = node_add_menu_poll;
  type.draw = add_root_catalogs_draw;
  type.listener = node_add_menu_assets_listen_fn;
  return type;
}

}  // namespace blender::ed::space_node

/* Note: This is only necessary because Python can't set an asset catalog path context item. */
void uiTemplateNodeAssetMenuItems(uiLayout *layout, bContext *C, const char *catalog_path)
{
  using namespace blender;
  using namespace blender::ed::space_node;
  bScreen &screen = *CTX_wm_screen(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  AssetItemTree &tree = *snode.runtime->assets_for_menu;
  const bke::AssetCatalogTreeItem *item = tree.catalogs.find_root_item(catalog_path);
  if (!item) {
    return;
  }
  const bke::AssetCatalogPath &path = tree.full_catalog_per_tree_item.lookup(item);
  PointerRNA path_ptr{
      &screen.id, &RNA_AssetCatalogPath, const_cast<bke::AssetCatalogPath *>(&path)};
  uiItemS(layout);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
  uiItemMContents(col, "NODE_MT_node_add_catalog_assets");
}
