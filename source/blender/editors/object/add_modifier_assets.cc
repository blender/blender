/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_listbase.h"
#include "BLI_multi_value_map.hh"
#include "BLI_string_utf8.h"

#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "MOD_nodes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"

#include "object_intern.hh"

namespace blender::ed::object {

static bool all_loading_finished()
{
  AssetLibraryReference all_library_ref = asset_system::all_library_reference();
  return asset::list::is_loaded(&all_library_ref);
}

static asset::AssetItemTree build_catalog_tree(const bContext &C)
{
  asset::AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_NT;
  auto meta_data_filter = [&](const AssetMetaData &meta_data) {
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
    if (tree_type == nullptr || IDP_int_get(tree_type) != NTREE_GEOMETRY) {
      return false;
    }
    const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
        &meta_data, "geometry_node_asset_traits_flag");
    if (traits_flag == nullptr || !(IDP_int_get(traits_flag) & GEO_NODE_ASSET_MODIFIER)) {
      return false;
    }
    return true;
  };
  const AssetLibraryReference library = asset_system::all_library_reference();
  asset_system::all_library_reload_catalogs_if_dirty();
  return asset::build_filtered_all_catalog_tree(library, C, type_filter, meta_data_filter);
}

static asset::AssetItemTree *get_static_item_tree()
{
  static asset::AssetItemTree tree;
  return &tree;
}

static void catalog_assets_draw(const bContext *C, Menu *menu)
{
  asset::AssetItemTree &tree = *get_static_item_tree();

  const std::optional<StringRefNull> menu_path = CTX_data_string_get(C, "asset_catalog_path");
  if (!menu_path) {
    return;
  }
  const int skip_essentials = CTX_data_int_get(C, "skip_essentials").value_or(0);
  const Span<asset_system::AssetRepresentation *> assets = tree.assets_per_path.lookup(
      menu_path->data());
  const asset_system::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(
      menu_path->data());
  BLI_assert(catalog_item != nullptr);

  if (assets.is_empty() && !catalog_item->has_children()) {
    return;
  }

  uiLayout *layout = menu->layout;

  bool first = true;
  const auto ensure_separator = [&]() {
    if (first) {
      layout->separator();
      first = false;
    }
  };

  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_modifier_add_node_group", true);
  for (const asset_system::AssetRepresentation *asset : assets) {
    if (skip_essentials) {
      if (asset->owner_asset_library().library_reference()->type == ASSET_LIBRARY_ESSENTIALS) {
        continue;
      }
    }
    ensure_separator();
    PointerRNA props_ptr = layout->op(
        ot, IFACE_(asset->get_name()), ICON_NONE, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
    asset::operator_asset_reference_props_set(*asset, props_ptr);
  }

  catalog_item->foreach_child([&](const asset_system::AssetCatalogTreeItem &item) {
    ensure_separator();
    asset::draw_menu_for_catalog(item, "OBJECT_MT_add_modifier_catalog_assets", *layout);
  });
}

static bool unassigned_local_poll(const Main &bmain)
{
  LISTBASE_FOREACH (const bNodeTree *, group, &bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (group->id.library_weak_reference || group->id.asset_data) {
      continue;
    }
    if (!group->geometry_node_asset_traits ||
        !(group->geometry_node_asset_traits->flag & GEO_NODE_ASSET_MODIFIER))
    {
      continue;
    }
    return true;
  }
  return false;
}

static void unassigned_assets_draw(const bContext *C, Menu *menu)
{
  Main &bmain = *CTX_data_main(C);
  asset::AssetItemTree &tree = *get_static_item_tree();
  uiLayout *layout = menu->layout;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_modifier_add_node_group", true);
  for (const asset_system::AssetRepresentation *asset : tree.unassigned_assets) {
    PointerRNA props_ptr = layout->op(
        ot, IFACE_(asset->get_name()), ICON_NONE, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
    asset::operator_asset_reference_props_set(*asset, props_ptr);
  }

  bool first = true;
  bool add_separator = !tree.unassigned_assets.is_empty();
  LISTBASE_FOREACH (const bNodeTree *, group, &bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (group->id.library_weak_reference || group->id.asset_data) {
      continue;
    }
    if (!group->geometry_node_asset_traits ||
        !(group->geometry_node_asset_traits->flag & GEO_NODE_ASSET_MODIFIER))
    {
      continue;
    }

    if (add_separator) {
      layout->separator();
      add_separator = false;
    }
    if (first) {
      layout->label(IFACE_("Non-Assets"), ICON_NONE);
      first = false;
    }

    PointerRNA props_ptr = layout->op(
        ot, group->id.name + 2, ICON_NONE, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
    WM_operator_properties_id_lookup_set_from_id(&props_ptr, &group->id);
  }
}

static void root_catalogs_draw(const bContext *C, Menu *menu)
{
  const Object *object = context_active_object(C);
  if (!object) {
    return;
  }
  uiLayout *layout = menu->layout;

  const bool loading_finished = all_loading_finished();

  asset::AssetItemTree &tree = *get_static_item_tree();
  tree = build_catalog_tree(*C);
  if (tree.catalogs.is_empty() && loading_finished) {
    return;
  }

  layout->separator();

  if (!loading_finished) {
    layout->label(IFACE_("Loading Asset Libraries"), ICON_INFO);
  }

  Set<std::string> all_builtin_menus = [&]() {
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
    if (ELEM(object->type, OB_MESH)) {
      menus.add_new("Normals");
    }
    if (ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_LATTICE)) {
      menus.add_new("Physics");
    }
    return menus;
  }();

  tree.catalogs.foreach_root_item([&](const asset_system::AssetCatalogTreeItem &item) {
    if (!all_builtin_menus.contains(item.get_name())) {
      asset::draw_menu_for_catalog(item, "OBJECT_MT_add_modifier_catalog_assets", *layout);
    }
  });

  if (!tree.unassigned_assets.is_empty() || unassigned_local_poll(*CTX_data_main(C))) {
    layout->separator();
    layout->menu(
        "OBJECT_MT_add_modifier_unassigned_assets", IFACE_("Unassigned"), ICON_FILE_HIDDEN);
  }
}

static bNodeTree *get_asset_or_local_node_group(const bContext &C,
                                                PointerRNA &ptr,
                                                ReportList *reports)
{
  Main &bmain = *CTX_data_main(&C);
  if (bNodeTree *group = reinterpret_cast<bNodeTree *>(
          WM_operator_properties_id_lookup_from_name_or_session_uid(&bmain, &ptr, ID_NT)))
  {
    return group;
  }

  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(C, ptr, reports);
  if (!asset) {
    return nullptr;
  }
  return reinterpret_cast<bNodeTree *>(asset::asset_local_id_ensure_imported(bmain, *asset));
}

static bNodeTree *get_node_group(const bContext &C, PointerRNA &ptr, ReportList *reports)
{
  bNodeTree *node_group = get_asset_or_local_node_group(C, ptr, reports);
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

static wmOperatorStatus modifier_add_asset_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  Vector<PointerRNA> objects = modifier_get_edit_objects(*C, *op);
  if (objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree *node_group = get_node_group(*C, *op->ptr, op->reports);
  if (!node_group) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  for (const PointerRNA &ptr : objects) {
    Object *object = static_cast<Object *>(ptr.data);
    NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(
        modifier_add(op->reports, bmain, scene, object, nullptr, eModifierType_Nodes));
    if (!nmd) {
      continue;
    }
    changed = true;
    nmd->node_group = node_group;
    id_us_plus(&node_group->id);
    MOD_nodes_update_interface(object, nmd);

    /* Don't show the data-block selector since it's not usually necessary for assets. */
    nmd->flag |= NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR;
    SET_FLAG_FROM_TEST(nmd->flag,
                       node_group->geometry_node_asset_traits &&
                           (node_group->geometry_node_asset_traits->flag &
                            GEO_NODE_ASSET_HIDE_MODIFIER_MANAGE_PANEL),
                       NODES_MODIFIER_HIDE_MANAGE_PANEL);

    STRNCPY_UTF8(nmd->modifier.name, DATA_(node_group->id.name + 2));
    BKE_modifier_unique_name(&object->modifiers, &nmd->modifier);

    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_add_asset_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  if (event->modifier & KM_ALT || CTX_wm_view3d(C)) {
    RNA_boolean_set(op->ptr, "use_selected_objects", true);
  }
  return modifier_add_asset_exec(C, op);
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

static void OBJECT_OT_modifier_add_node_group(wmOperatorType *ot)
{
  ot->name = "Add Modifier";
  ot->description = "Add a procedural operation/effect to the active object";
  ot->idname = "OBJECT_OT_modifier_add_node_group";

  ot->invoke = modifier_add_asset_invoke;
  ot->exec = modifier_add_asset_exec;
  ot->poll = ED_operator_object_active_editable;
  ot->get_description = modifier_add_asset_get_description;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  asset::operator_asset_reference_props_register(*ot->srna);
  WM_operator_properties_id_lookup(ot, false);
  modifier_register_use_selected_objects_prop(ot);
}

static MenuType modifier_add_unassigned_assets_menu_type()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "OBJECT_MT_add_modifier_unassigned_assets");
  type.draw = unassigned_assets_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.description = N_(
      "Modifier node group assets not assigned to a catalog.\n"
      "Catalogs can be assigned in the Asset Browser");
  return type;
}

static MenuType modifier_add_catalog_assets_menu_type()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "OBJECT_MT_add_modifier_catalog_assets");
  type.draw = catalog_assets_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

static MenuType modifier_add_root_catalogs_menu_type()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "OBJECT_MT_modifier_add_root_catalogs");
  type.draw = root_catalogs_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

void object_modifier_add_asset_register()
{
  WM_menutype_add(MEM_dupallocN<MenuType>(__func__, modifier_add_catalog_assets_menu_type()));
  WM_menutype_add(MEM_dupallocN<MenuType>(__func__, modifier_add_unassigned_assets_menu_type()));
  WM_menutype_add(MEM_dupallocN<MenuType>(__func__, modifier_add_root_catalogs_menu_type()));
  WM_operatortype_append(OBJECT_OT_modifier_add_node_group);
}

void ui_template_modifier_asset_menu_items(uiLayout &layout,
                                           const StringRef catalog_path,
                                           const bool skip_essentials)
{
  asset::AssetItemTree &tree = *get_static_item_tree();
  const asset_system::AssetCatalogTreeItem *item = tree.catalogs.find_root_item(catalog_path);
  if (!item) {
    return;
  }
  asset_system::AssetLibrary *all_library = asset::list::library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }
  uiLayout *col = &layout.column(false);
  col->context_string_set("asset_catalog_path", item->catalog_path().str());
  col->context_int_set("skip_essentials", skip_essentials);
  col->menu_contents("OBJECT_MT_add_modifier_catalog_assets");
}

}  // namespace blender::ed::object
