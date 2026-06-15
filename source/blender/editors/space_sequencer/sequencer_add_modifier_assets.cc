/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_listbase.hh"
#include "BLI_string_utf8.hh"

#include "BKE_asset.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_object.hh"

#include "NOD_composite.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"

#include "SEQ_modifier.hh"
#include "SEQ_relations.hh"
#include "SEQ_select.hh"

#include "sequencer_intern.hh"

namespace blender::ed::vse {

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
    if (tree_type == nullptr || IDP_int_get(tree_type) != NTREE_COMPOSIT) {
      return false;
    }
    const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
        &meta_data, "compositor_node_asset_traits_flag");
    if (traits_flag == nullptr || !(IDP_int_get(traits_flag) & COMPOSIT_NODE_ASSET_STRIP_MODIFIER))
    {
      return false;
    }
    return true;
  };
  const AssetLibraryReference library = asset_system::all_library_reference();
  asset_system::all_library_reload_catalogs_if_dirty();
  return asset::build_filtered_all_catalog_tree(
      library, C, type_filter, meta_data_filter, ntreeType_Composite->asset_catalog_path_prefix);
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
  const Span<asset_system::AssetRepresentation *> assets = tree.assets_per_path.lookup(
      menu_path->data());
  const asset_system::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(
      menu_path->data());
  BLI_assert(catalog_item != nullptr);

  if (assets.is_empty() && !catalog_item->has_children()) {
    return;
  }

  ui::Layout &layout = *menu->layout;

  bool first = true;
  const auto ensure_separator = [&]() {
    if (first) {
      layout.separator();
      first = false;
    }
  };

  wmOperatorType *ot = WM_operatortype_find("SEQUENCER_OT_strip_modifier_add_node_group", true);
  for (const asset_system::AssetRepresentation *asset : assets) {
    ensure_separator();
    PointerRNA props_ptr = layout.op(
        ot, IFACE_(asset->get_name()), ICON_NONE, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
    asset::operator_asset_reference_props_set(*asset, props_ptr);
  }

  catalog_item->foreach_child([&](const asset_system::AssetCatalogTreeItem &item) {
    ensure_separator();
    asset::draw_menu_for_catalog(item, "SEQUENCER_MT_add_modifier_catalog_assets", layout);
  });
}

static bool unassigned_local_poll(const Main &bmain)
{
  for (const bNodeTree &group : bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (ID_IS_ASSET(&group.id)) {
      continue;
    }
    if (!group.compositor_node_asset_traits ||
        !(group.compositor_node_asset_traits->flag & COMPOSIT_NODE_ASSET_STRIP_MODIFIER))
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
  ui::Layout &layout = *menu->layout;
  wmOperatorType *ot = WM_operatortype_find("SEQUENCER_OT_strip_modifier_add_node_group", true);
  for (const asset_system::AssetRepresentation *asset : tree.unassigned_assets) {
    PointerRNA props_ptr = layout.op(
        ot, IFACE_(asset->get_name()), ICON_NONE, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
    asset::operator_asset_reference_props_set(*asset, props_ptr);
  }

  bool first = true;
  bool add_separator = !tree.unassigned_assets.is_empty();
  for (const bNodeTree &group : bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (ID_IS_ASSET(&group.id)) {
      continue;
    }
    if (!group.compositor_node_asset_traits ||
        !(group.compositor_node_asset_traits->flag & COMPOSIT_NODE_ASSET_STRIP_MODIFIER))
    {
      continue;
    }

    if (add_separator) {
      layout.separator();
      add_separator = false;
    }
    if (first) {
      layout.label(IFACE_("Non-Assets"), ICON_NONE);
      first = false;
    }

    PointerRNA props_ptr = layout.op(
        ot, group.id.name + 2, ICON_NONE, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
    WM_operator_properties_id_lookup_set_from_id(&props_ptr, &group.id);
  }
}

static void root_catalogs_draw(const bContext *C, Menu *menu)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  if (!strip) {
    return;
  }
  ui::Layout &layout = *menu->layout;

  const bool loading_finished = all_loading_finished();

  asset::AssetItemTree &tree = *get_static_item_tree();
  tree = build_catalog_tree(*C);
  if (!tree.catalogs.is_empty() || loading_finished) {
    layout.separator();

    if (!loading_finished) {
      layout.label(IFACE_("Loading Asset Libraries"), ICON_INFO);
    }

    tree.catalogs.foreach_root_item([&](const asset_system::AssetCatalogTreeItem &item) {
      asset::draw_menu_for_catalog(item, "SEQUENCER_MT_add_modifier_catalog_assets", layout);
    });
  }

  if (!tree.unassigned_assets.is_empty() || unassigned_local_poll(*CTX_data_main(C))) {
    layout.separator();
    layout.menu(
        "SEQUENCER_MT_add_modifier_unassigned_assets", IFACE_("Unassigned"), ICON_FILE_HIDDEN);
  }
}

/* -------------------------------------------------------------------- */
/** \name Add compositor modifier node group Operator
 * \{ */

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
  if (!node_group || ID_MISSING(node_group)) {
    return nullptr;
  }
  if (node_group->type != NTREE_COMPOSIT) {
    if (reports) {
      BKE_report(reports, RPT_ERROR, "Asset is not a compositor node group");
    }
    return nullptr;
  }
  return node_group;
}

static wmOperatorStatus strip_modifier_add_asset_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);

  bNodeTree *node_group = get_node_group(*C, *op->ptr, op->reports);
  if (!node_group) {
    return OPERATOR_CANCELLED;
  }

  auto *cmd = reinterpret_cast<SequencerCompositorModifierData *>(
      seq::modifier_new(strip, nullptr, eSeqModifierType_Compositor));
  if (!cmd) {
    return OPERATOR_CANCELLED;
  }
  /* Assign the node group. */
  cmd->node_group = node_group;
  id_us_plus(&node_group->id);
  /* Set the modifier name. */
  STRNCPY_UTF8(cmd->modifier.name, DATA_(node_group->id.name + 2));
  seq::modifier_unique_name(strip, &cmd->modifier);
  /* Hide the datablock selector by default for assets. */
  cmd->flag |= SEQ_COMP_MOD_HIDE_DATABLOCK_SELECTOR;

  seq::modifier_persistent_uid_init(*strip, cmd->modifier);

  seq::compositor_nodes_update_interface(*scene, *cmd);

  seq::relations_invalidate_cache(scene, strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static std::string strip_modifier_add_asset_get_description(bContext *C,
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

static void SEQUENCER_OT_strip_modifier_add_node_group(wmOperatorType *ot)
{
  ot->name = "Add Strip Modifier";
  ot->description = "Add a modifier to the strip";
  ot->idname = "SEQUENCER_OT_strip_modifier_add_node_group";

  ot->exec = strip_modifier_add_asset_exec;
  ot->poll = sequencer_strip_editable_poll;
  ot->get_description = strip_modifier_add_asset_get_description;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  asset::operator_asset_reference_props_register(*ot->srna);
  WM_operator_properties_id_lookup(ot, false);
}

/** \} */

static MenuType modifier_add_unassigned_assets_menu_type()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "SEQUENCER_MT_add_modifier_unassigned_assets");
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
  STRNCPY_UTF8(type.idname, "SEQUENCER_MT_add_modifier_catalog_assets");
  type.draw = catalog_assets_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

static MenuType modifier_add_root_catalogs_menu_type()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "SEQUENCER_MT_modifier_add_root_catalogs");
  type.draw = root_catalogs_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

void sequencer_strip_modifier_add_asset_register()
{
  WM_menutype_add(MEM_new<MenuType>(__func__, modifier_add_catalog_assets_menu_type()));
  WM_menutype_add(MEM_new<MenuType>(__func__, modifier_add_unassigned_assets_menu_type()));
  WM_menutype_add(MEM_new<MenuType>(__func__, modifier_add_root_catalogs_menu_type()));
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_add_node_group);
}

}  // namespace blender::ed::vse
