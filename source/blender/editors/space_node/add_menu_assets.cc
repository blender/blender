/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_multi_value_map.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.h"
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
  asset_system::AssetRepresentation &asset;
};

struct AssetItemTree {
  asset_system::AssetCatalogTree catalogs;
  MultiValueMap<asset_system::AssetCatalogPath, LibraryAsset> assets_per_path;
};

static AssetLibraryReference all_library_reference()
{
  AssetLibraryReference all_library_ref{};
  all_library_ref.custom_library_index = -1;
  all_library_ref.type = ASSET_LIBRARY_ALL;
  return all_library_ref;
}

static bool all_loading_finished()
{
  AssetLibraryReference all_library_ref = all_library_reference();
  return ED_assetlist_is_loaded(&all_library_ref);
}

static asset_system::AssetLibrary *get_all_library_once_available()
{
  const AssetLibraryReference all_library_ref = all_library_reference();
  return ED_assetlist_library_get_once_available(all_library_ref);
}

/**
 * The menus want to pass catalog paths to context and for this they need persistent pointers to
 * the paths. Rather than keeping some local path storage, get a pointer into the asset system
 * directly, which is persistent until the library is reloaded and can safely be held by context.
 */
static PointerRNA persistent_catalog_path_rna_pointer(
    bScreen &owner_screen,
    const asset_system::AssetLibrary &library,
    const asset_system::AssetCatalogTreeItem &item)
{
  const asset_system::AssetCatalog *catalog = library.catalog_service->find_catalog_by_path(
      item.catalog_path());
  if (!catalog) {
    return PointerRNA_NULL;
  }

  const asset_system::AssetCatalogPath &path = catalog->path;
  return {&owner_screen.id,
          &RNA_AssetCatalogPath,
          const_cast<asset_system::AssetCatalogPath *>(&path)};
}

static AssetItemTree build_catalog_tree(const bContext &C, const bNodeTree *node_tree)
{
  if (!node_tree) {
    return {};
  }

  /* Find all the matching node group assets for every catalog path. */
  MultiValueMap<asset_system::AssetCatalogPath, LibraryAsset> assets_per_path;

  AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_NT;

  const AssetLibraryReference all_library_ref = all_library_reference();

  ED_assetlist_storage_fetch(&all_library_ref, &C);
  ED_assetlist_ensure_previews_job(&all_library_ref, &C);

  asset_system::AssetLibrary *all_library = get_all_library_once_available();
  if (!all_library) {
    return {};
  }

  ED_assetlist_iterate(all_library_ref, [&](asset_system::AssetRepresentation &asset) {
    if (!ED_asset_filter_matches_asset(&type_filter, asset)) {
      return true;
    }
    const AssetMetaData &meta_data = asset.get_metadata();
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
    if (tree_type == nullptr || IDP_Int(tree_type) != node_tree->type) {
      return true;
    }
    if (BLI_uuid_is_nil(meta_data.catalog_id)) {
      return true;
    }

    const asset_system::AssetCatalog *catalog = all_library->catalog_service->find_catalog(
        meta_data.catalog_id);
    if (catalog == nullptr) {
      return true;
    }
    assets_per_path.add(catalog->path, LibraryAsset{all_library_ref, asset});
    return true;
  });

  /* Build an own tree without any of the catalogs that don't have proper node group assets. */
  asset_system::AssetCatalogTree catalogs_with_node_assets;
  asset_system::AssetCatalogTree &catalog_tree = *all_library->catalog_service->get_catalog_tree();
  catalog_tree.foreach_item([&](asset_system::AssetCatalogTreeItem &item) {
    if (assets_per_path.lookup(item.catalog_path()).is_empty()) {
      return;
    }
    asset_system::AssetCatalog *catalog = all_library->catalog_service->find_catalog(
        item.get_catalog_id());
    if (catalog == nullptr) {
      return;
    }
    catalogs_with_node_assets.insert_item(*catalog);
  });

  return {std::move(catalogs_with_node_assets), std::move(assets_per_path)};
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
  const asset_system::AssetCatalogPath &menu_path =
      *static_cast<const asset_system::AssetCatalogPath *>(menu_path_ptr.data);

  const Span<LibraryAsset> asset_items = tree.assets_per_path.lookup(menu_path);
  asset_system::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(menu_path);
  BLI_assert(catalog_item != nullptr);

  if (asset_items.is_empty() && !catalog_item->has_children()) {
    return;
  }

  uiLayout *layout = menu->layout;
  uiItemS(layout);

  for (const LibraryAsset &item : asset_items) {
    uiLayout *col = uiLayoutColumn(layout, false);

    PointerRNA asset_ptr{nullptr, &RNA_AssetRepresentation, &item.asset};
    uiLayoutSetContextPointer(col, "asset", &asset_ptr);

    PointerRNA library_ptr{&screen.id,
                           &RNA_AssetLibraryReference,
                           const_cast<AssetLibraryReference *>(&item.library_ref)};
    uiLayoutSetContextPointer(col, "asset_library_ref", &library_ptr);

    uiItemO(col, IFACE_(item.asset.get_name().c_str()), ICON_NONE, "NODE_OT_add_group_asset");
  }

  asset_system::AssetLibrary *all_library = get_all_library_once_available();
  if (!all_library) {
    return;
  }

  catalog_item->foreach_child([&](asset_system::AssetCatalogTreeItem &child_item) {
    PointerRNA path_ptr = persistent_catalog_path_rna_pointer(screen, *all_library, child_item);
    if (path_ptr.data == nullptr) {
      return;
    }

    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
    uiItemM(
        col, "NODE_MT_node_add_catalog_assets", IFACE_(child_item.get_name().c_str()), ICON_NONE);
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

  asset_system::AssetLibrary *all_library = get_all_library_once_available();
  if (!all_library) {
    return;
  }

  tree.catalogs.foreach_root_item([&](asset_system::AssetCatalogTreeItem &item) {
    if (all_builtin_menus.contains(item.get_name())) {
      return;
    }
    PointerRNA path_ptr = persistent_catalog_path_rna_pointer(screen, *all_library, item);
    if (path_ptr.data == nullptr) {
      return;
    }
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
    uiItemM(col, "NODE_MT_node_add_catalog_assets", IFACE_(item.get_name().c_str()), ICON_NONE);
  });
}

MenuType add_catalog_assets_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "NODE_MT_node_add_catalog_assets");
  type.poll = node_add_menu_poll;
  type.draw = node_add_catalog_assets_draw;
  type.listener = node_add_menu_assets_listen_fn;
  return type;
}

MenuType add_root_catalogs_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "NODE_MT_node_add_root_catalogs");
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

  if (snode.runtime->assets_for_menu == nullptr) {
    return;
  }

  AssetItemTree &tree = *snode.runtime->assets_for_menu;
  const asset_system::AssetCatalogTreeItem *item = tree.catalogs.find_root_item(catalog_path);
  if (!item) {
    return;
  }

  asset_system::AssetLibrary *all_library = get_all_library_once_available();
  if (!all_library) {
    return;
  }

  PointerRNA path_ptr = persistent_catalog_path_rna_pointer(screen, *all_library, *item);
  if (path_ptr.data == nullptr) {
    return;
  }

  uiItemS(layout);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
  uiItemMContents(col, "NODE_MT_node_add_catalog_assets");
}
