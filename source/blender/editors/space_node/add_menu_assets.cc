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

#include "RNA_access.hh"

#include "ED_asset.hh"
#include "ED_screen.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

static bool node_add_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  return CTX_wm_space_node(C);
}

static bool all_loading_finished()
{
  AssetLibraryReference all_library_ref = asset_system::all_library_reference();
  return ED_assetlist_is_loaded(&all_library_ref);
}

static asset::AssetItemTree build_catalog_tree(const bContext &C, const bNodeTree &node_tree)
{
  AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_NT;
  auto meta_data_filter = [&](const AssetMetaData &meta_data) {
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
    if (tree_type == nullptr || IDP_Int(tree_type) != node_tree.type) {
      return false;
    }
    return true;
  };
  const AssetLibraryReference library = asset_system::all_library_reference();
  return asset::build_filtered_all_catalog_tree(library, C, type_filter, meta_data_filter);
}

static void node_add_catalog_assets_draw(const bContext *C, Menu *menu)
{
  bScreen &screen = *CTX_wm_screen(C);
  const SpaceNode &snode = *CTX_wm_space_node(C);
  if (!snode.runtime->assets_for_menu) {
    BLI_assert_unreachable();
    return;
  }
  asset::AssetItemTree &tree = *snode.runtime->assets_for_menu;
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

  const Span<asset_system::AssetRepresentation *> assets = tree.assets_per_path.lookup(menu_path);
  asset_system::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(menu_path);
  BLI_assert(catalog_item != nullptr);

  if (assets.is_empty() && !catalog_item->has_children()) {
    return;
  }

  uiLayout *layout = menu->layout;
  uiItemS(layout);

  for (const asset_system::AssetRepresentation *asset : assets) {
    uiLayout *col = uiLayoutColumn(layout, false);
    PointerRNA asset_ptr = asset::create_asset_rna_ptr(asset);
    uiLayoutSetContextPointer(col, "asset", &asset_ptr);
    uiItemO(col, IFACE_(asset->get_name().c_str()), ICON_NONE, "NODE_OT_add_group_asset");
  }

  asset_system::AssetLibrary *all_library = ED_assetlist_library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }

  catalog_item->foreach_child([&](asset_system::AssetCatalogTreeItem &child_item) {
    PointerRNA path_ptr = asset::persistent_catalog_path_rna_pointer(
        screen, *all_library, child_item);
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
  uiLayout *layout = menu->layout;
  const bNodeTree *edit_tree = snode.edittree;
  if (!edit_tree) {
    return;
  }

  snode.runtime->assets_for_menu = std::make_shared<asset::AssetItemTree>(
      build_catalog_tree(*C, *edit_tree));

  const bool loading_finished = all_loading_finished();

  asset::AssetItemTree &tree = *snode.runtime->assets_for_menu;
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
    menus.add_new("Tool");
    menus.add_new("Utilities");
    menus.add_new("UV");
    menus.add_new("Vector");
    menus.add_new("Volume");
    menus.add_new("Group");
    menus.add_new("Layout");
    return menus;
  }();

  asset_system::AssetLibrary *all_library = ED_assetlist_library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }

  tree.catalogs.foreach_root_item([&](asset_system::AssetCatalogTreeItem &item) {
    if (all_builtin_menus.contains(item.get_name())) {
      return;
    }
    PointerRNA path_ptr = asset::persistent_catalog_path_rna_pointer(screen, *all_library, item);
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
  type.listener = asset::asset_reading_region_listen_fn;
  return type;
}

MenuType add_root_catalogs_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "NODE_MT_node_add_root_catalogs");
  type.poll = node_add_menu_poll;
  type.draw = add_root_catalogs_draw;
  type.listener = asset::asset_reading_region_listen_fn;
  return type;
}

}  // namespace blender::ed::space_node

void uiTemplateNodeAssetMenuItems(uiLayout *layout, bContext *C, const char *catalog_path)
{
  using namespace blender;
  using namespace blender::ed;
  using namespace blender::ed::space_node;
  bScreen &screen = *CTX_wm_screen(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  if (snode.runtime->assets_for_menu == nullptr) {
    return;
  }
  asset::AssetItemTree &tree = *snode.runtime->assets_for_menu;
  const asset_system::AssetCatalogTreeItem *item = tree.catalogs.find_root_item(catalog_path);
  if (!item) {
    return;
  }
  asset_system::AssetLibrary *all_library = ED_assetlist_library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }
  PointerRNA path_ptr = asset::persistent_catalog_path_rna_pointer(screen, *all_library, *item);
  if (path_ptr.data == nullptr) {
    return;
  }
  uiItemS(layout);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
  uiItemMContents(col, "NODE_MT_node_add_catalog_assets");
}
