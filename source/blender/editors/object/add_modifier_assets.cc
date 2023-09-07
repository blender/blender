/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_multi_value_map.hh"
#include "BLI_string.h"

#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_lib_id.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "RNA_access.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "MOD_nodes.hh"

#include "UI_interface.hh"

#include "WM_api.hh"

#include "object_intern.h"

namespace blender::ed::object {

static bool all_loading_finished()
{
  AssetLibraryReference all_library_ref = asset_system::all_library_reference();
  return ED_assetlist_is_loaded(&all_library_ref);
}

static asset::AssetItemTree build_catalog_tree(const bContext &C)
{
  AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_NT;
  auto meta_data_filter = [&](const AssetMetaData &meta_data) {
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
    if (tree_type == nullptr || IDP_Int(tree_type) != NTREE_GEOMETRY) {
      return false;
    }
    const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
        &meta_data, "geometry_node_asset_traits_flag");
    if (traits_flag == nullptr || !(IDP_Int(traits_flag) & GEO_NODE_ASSET_MODIFIER)) {
      return false;
    }
    return true;
  };
  const AssetLibraryReference library = asset_system::all_library_reference();
  return asset::build_filtered_all_catalog_tree(library, C, type_filter, meta_data_filter);
}

static asset::AssetItemTree *get_static_item_tree()
{
  static asset::AssetItemTree tree;
  return &tree;
}

static void catalog_assets_draw(const bContext *C, Menu *menu)
{
  bScreen &screen = *CTX_wm_screen(C);
  asset::AssetItemTree &tree = *get_static_item_tree();

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
    wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_modifier_add_asset", true);
    PointerRNA props_ptr;
    uiItemFullO_ptr(col,
                    ot,
                    IFACE_(asset->get_name().c_str()),
                    ICON_NONE,
                    nullptr,
                    WM_OP_INVOKE_DEFAULT,
                    UI_ITEM_NONE,
                    &props_ptr);
    asset::operator_asset_reference_props_set(*asset, props_ptr);
  }

  asset_system::AssetLibrary *all_library = ED_assetlist_library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }

  catalog_item->foreach_child([&](asset_system::AssetCatalogTreeItem &item) {
    asset::draw_menu_for_catalog(
        screen, *all_library, item, "OBJECT_MT_add_modifier_catalog_assets", *layout);
  });
}

static void root_catalogs_draw(const bContext *C, Menu *menu)
{
  const Object *object = ED_object_active_context(C);
  if (!object) {
    return;
  }
  bScreen &screen = *CTX_wm_screen(C);
  uiLayout *layout = menu->layout;

  const bool loading_finished = all_loading_finished();

  asset::AssetItemTree &tree = *get_static_item_tree();
  tree = build_catalog_tree(*C);
  if (tree.catalogs.is_empty() && loading_finished) {
    return;
  }

  uiItemS(layout);

  if (!loading_finished) {
    uiItemL(layout, IFACE_("Loading Asset Libraries"), ICON_INFO);
  }

  static Set<std::string> all_builtin_menus = [&]() {
    Set<std::string> menus;
    if (ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_LATTICE)) {
      menus.add_new("Edit");
    }
    if (ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_VOLUME)) {
      menus.add_new("Generate");
    }
    if (ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_LATTICE, OB_VOLUME)) {
      menus.add_new("Deform");
    }
    if (ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_LATTICE)) {
      menus.add_new("Physics");
    }
    return menus;
  }();

  asset_system::AssetLibrary *all_library = ED_assetlist_library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }

  tree.catalogs.foreach_root_item([&](asset_system::AssetCatalogTreeItem &item) {
    if (!all_builtin_menus.contains(item.get_name())) {
      asset::draw_menu_for_catalog(
          screen, *all_library, item, "OBJECT_MT_add_modifier_catalog_assets", *layout);
    }
  });
}

static bNodeTree *get_node_group(const bContext &C, PointerRNA &ptr, ReportList *reports)
{
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(C, ptr, reports);
  if (!asset) {
    return nullptr;
  }
  Main &bmain = *CTX_data_main(&C);
  bNodeTree *node_group = reinterpret_cast<bNodeTree *>(
      asset::asset_local_id_ensure_imported(bmain, *asset));
  if (!node_group) {
    return nullptr;
  }
  if (node_group->type != NTREE_GEOMETRY) {
    if (reports) {
      BKE_report(reports, RPT_ERROR, "Asset is not a geometry node group");
    }
    return nullptr;
  }
  return node_group;
}

static int modifier_add_asset_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *object = ED_object_active_context(C);

  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(
      ED_object_modifier_add(op->reports, bmain, scene, object, nullptr, eModifierType_Nodes));
  if (!nmd) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree *node_group = get_node_group(*C, *op->ptr, op->reports);
  if (!node_group) {
    return OPERATOR_CANCELLED;
  }

  nmd->node_group = node_group;
  id_us_plus(&node_group->id);
  MOD_nodes_update_interface(object, nmd);

  /* By default, don't show the data-block selector since it's not usually necessary for assets. */
  nmd->flag |= NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR;

  STRNCPY(nmd->modifier.name, DATA_(node_group->id.name + 2));

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);

  return OPERATOR_FINISHED;
}

static std::string modifier_add_asset_get_description(bContext *C,
                                                      wmOperatorType * /*ot*/,
                                                      PointerRNA *ptr)
{
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *ptr, nullptr);
  if (!asset) {
    return "";
  }
  if (!asset->get_metadata().description) {
    return "";
  }
  return TIP_(asset->get_metadata().description);
}

static void OBJECT_OT_modifier_add_asset(wmOperatorType *ot)
{
  ot->name = "Add Modifier";
  ot->description = "Add a procedural operation/effect to the active object";
  ot->idname = "OBJECT_OT_modifier_add_asset";

  ot->exec = modifier_add_asset_exec;
  ot->poll = ED_operator_object_active_editable;
  ot->get_description = modifier_add_asset_get_description;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  asset::operator_asset_reference_props_register(*ot->srna);
}

static MenuType modifier_add_catalog_assets_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "OBJECT_MT_add_modifier_catalog_assets");
  type.draw = catalog_assets_draw;
  type.listener = asset::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

static MenuType modifier_add_root_catalogs_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "OBJECT_MT_modifier_add_root_catalogs");
  type.draw = root_catalogs_draw;
  type.listener = asset::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

void object_modifier_add_asset_register()
{
  WM_menutype_add(MEM_new<MenuType>(__func__, modifier_add_catalog_assets_menu_type()));
  WM_menutype_add(MEM_new<MenuType>(__func__, modifier_add_root_catalogs_menu_type()));
  WM_operatortype_append(OBJECT_OT_modifier_add_asset);
}

void ui_template_modifier_asset_menu_items(uiLayout &layout,
                                           bContext &C,
                                           const StringRef catalog_path)
{
  using namespace blender;
  using namespace blender::ed;
  using namespace blender::ed::object;
  bScreen &screen = *CTX_wm_screen(&C);
  asset::AssetItemTree &tree = *get_static_item_tree();
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
  uiItemS(&layout);
  uiLayout *col = uiLayoutColumn(&layout, false);
  uiLayoutSetContextPointer(col, "asset_catalog_path", &path_ptr);
  uiItemMContents(col, "OBJECT_MT_add_modifier_catalog_assets");
}

}  // namespace blender::ed::object
