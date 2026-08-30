/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscene
 */

#include "BLI_listbase.hh"
#include "BLI_string_utf8.hh"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_node_types.h"
#include "DNA_windowmanager_enums.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BKE_asset.hh"
#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "NOD_composite.hh"
#include "NOD_defaults.hh"

#include "ED_asset.hh"
#include "ED_asset_import.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_object.hh"
#include "ED_scene.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Operator Utilities
 * \{ */

/* If the "name" property is not set, fill the name property with the name of the effect with a UI
 * panel below the mouse cursor, unless a specific effect is set with a context pointer. Used in
 * order to apply effect operators on hover over their panels. */
static wmOperatorStatus compositor_effect_invoke_properties_with_hover(bContext *C,
                                                                       wmOperator *op,
                                                                       const wmEvent *event)
{
  if (RNA_struct_property_is_set(op->ptr, "name")) {
    return OPERATOR_FINISHED;
  }

  /* Note that the context pointer is *not* the active effect, it is set in UI layouts, see the
   * panel_context_pointer_set calls in effect panel draw functions. */
  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "effect", RNA_SceneCompositorEffect);
  if (ctx_ptr) {
    SceneCompositorEffect *effect = static_cast<SceneCompositorEffect *>(ctx_ptr.data);
    RNA_string_set(op->ptr, "name", effect->name);
    return OPERATOR_FINISHED;
  }

  PointerRNA *panel_ptr = ui::region_panel_custom_data_under_cursor(C, event);
  if (panel_ptr == nullptr || !*panel_ptr) {
    /* The operators using this function can typically be called from UIs that aren't related to
     * the effects UI at all. So include #OPERATOR_PASS_THROUGH to not block events from reaching
     * other operators/handlers. */
    return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
  }

  if (!RNA_struct_is_a(panel_ptr->type, RNA_SceneCompositorEffect)) {
    /* Work around multiple operators using the same shortcut. The operators for the other
     * stacks in the property editor use the same key, and will not run after these return
     * OPERATOR_CANCELLED. */
    return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
  }

  const SceneCompositorEffect *effect = static_cast<const SceneCompositorEffect *>(
      panel_ptr->data);
  RNA_string_set(op->ptr, "name", effect->name);
  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Effect Operator
 * \{ */

static wmOperatorStatus add_compositor_effect_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  bke::compositor::new_effect(*scene, "Effect");

  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static void SCENE_OT_add_compositor_effect(wmOperatorType *ot)
{
  ot->name = "Add Scene Compositor Effect";
  ot->idname = "SCENE_OT_add_compositor_effect";
  ot->description = "Add a scene compositor effect to the scene";

  ot->exec = add_compositor_effect_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Effect Operator
 * \{ */

static wmOperatorStatus remove_compositor_effect_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  const std::string name = RNA_string_get(op->ptr, "name");
  SceneCompositorEffect *effect = bke::compositor::get_effect(*scene, name);
  if (!effect) {
    BKE_reportf(op->reports, RPT_ERROR, "No effect '%s' in scene", name.c_str());
    return OPERATOR_CANCELLED;
  }

  bke::compositor::remove_effect(*scene, *effect);

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus remove_compositor_effect_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  wmOperatorStatus status = compositor_effect_invoke_properties_with_hover(C, op, event);
  if (!(status & OPERATOR_CANCELLED)) {
    return remove_compositor_effect_exec(C, op);
  }
  return status;
}

static void SCENE_OT_remove_compositor_effect(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Remove Scene Compositor Effect";
  ot->idname = "SCENE_OT_remove_compositor_effect";
  ot->description = "Remove a scene compositor effect from the scene";

  ot->invoke = remove_compositor_effect_invoke;
  ot->exec = remove_compositor_effect_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_string(
      ot->srna, "name", "Name", MAX_NAME, "Name", "Name of the effect to remove");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Effect Operator
 * \{ */

static wmOperatorStatus duplicate_compositor_effect_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  if (scene->compositor_effects.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "No effect to duplicate");
    return OPERATOR_CANCELLED;
  }

  std::string name = RNA_string_get(op->ptr, "name");
  SceneCompositorEffect *effect = name.empty() ? bke::compositor::get_active_effect(*scene) :
                                                 bke::compositor::get_effect(*scene, name);
  if (!effect) {
    if (name.empty()) {
      BKE_reportf(op->reports, RPT_ERROR, "No active effect in scene");
    }
    else {
      BKE_reportf(op->reports, RPT_ERROR, "No effect '%s' in scene", name.c_str());
    }
    return OPERATOR_CANCELLED;
  }

  bke::compositor::duplicate_effect(*scene, *effect);

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus duplicate_compositor_effect_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  wmOperatorStatus status = compositor_effect_invoke_properties_with_hover(C, op, event);
  if (!(status & OPERATOR_CANCELLED)) {
    return duplicate_compositor_effect_exec(C, op);
  }
  return status;
}

static void SCENE_OT_duplicate_compositor_effect(wmOperatorType *ot)
{
  ot->name = "Duplicate Scene Compositor Effect";
  ot->idname = "SCENE_OT_duplicate_compositor_effect";
  ot->description = "Duplicate the active or the given scene compositor effect";

  ot->invoke = duplicate_compositor_effect_invoke;
  ot->exec = duplicate_compositor_effect_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_string(
      ot->srna,
      "name",
      nullptr,
      MAX_NAME,
      "Name",
      "Name of the effect to duplicate. If empty duplicate the active effect");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Effect to Index Operator
 * \{ */

static wmOperatorStatus move_compositor_effect_to_index_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const std::string name = RNA_string_get(op->ptr, "name");
  SceneCompositorEffect *effect = bke::compositor::get_effect(*scene, name);
  if (!effect) {
    BKE_reportf(op->reports, RPT_ERROR, "No effect '%s' in scene", name.c_str());
    return OPERATOR_CANCELLED;
  }

  const int current_index = BLI_findindex(&scene->compositor_effects, effect);
  const int new_index = RNA_int_get(op->ptr, "index");
  const bool successful = BLI_listbase_move_index(
      &scene->compositor_effects, current_index, new_index);
  if (!successful) {
    BKE_report(op->reports, RPT_ERROR, "Index is out of range");
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus move_compositor_effect_to_index_invoke(bContext *C,
                                                               wmOperator *op,
                                                               const wmEvent * /*event*/)
{
  return move_compositor_effect_to_index_exec(C, op);
}

static void SCENE_OT_move_compositor_effect_to_index(wmOperatorType *ot)
{
  ot->name = "Move Active Scene Compositor Effect to Index";
  ot->description =
      "Change the scene compositor effect's index in the stack so it evaluates after the set "
      "number of others";
  ot->idname = "SCENE_OT_move_compositor_effect_to_index";

  ot->invoke = move_compositor_effect_to_index_invoke;
  ot->exec = move_compositor_effect_to_index_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  PropertyRNA *prop;
  prop = RNA_def_string(ot->srna, "name", nullptr, MAX_NAME, "Name", "Name of the effect to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "The index to move the effect to", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Active Effect Operator
 * \{ */

static wmOperatorStatus set_active_compositor_effect_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const std::string name = RNA_string_get(op->ptr, "name");
  SceneCompositorEffect *effect = bke::compositor::get_effect(*scene, name);
  if (!effect) {
    BKE_reportf(op->reports, RPT_ERROR, "No effect '%s' in scene", name.c_str());
    return OPERATOR_CANCELLED;
  }
  bke::compositor::set_active_effect(*scene, *effect);

  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus set_active_compositor_effect_invoke(bContext *C,
                                                            wmOperator *op,
                                                            const wmEvent *event)
{
  wmOperatorStatus status = compositor_effect_invoke_properties_with_hover(C, op, event);
  if (!(status & OPERATOR_CANCELLED)) {
    return set_active_compositor_effect_exec(C, op);
  }
  return status;
}

static void SCENE_OT_set_active_compositor_effect(wmOperatorType *ot)
{
  ot->name = "Set Active Scene Compositor Effect";
  ot->description = "Set the given scene compositor effect as the active one";
  ot->idname = "SCENE_OT_set_active_compositor_effect";

  ot->invoke = set_active_compositor_effect_invoke;
  ot->exec = set_active_compositor_effect_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  ot->prop = RNA_def_string(
      ot->srna, "name", nullptr, MAX_NAME, "Name", "Name of the strip effect to edit");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Compositor Effect Node Group Operator
 * \{ */

static wmOperatorStatus new_compositor_effect_node_group_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  bNodeTree *node_group = bke::node_tree_add_tree(bmain, "Scene Compositor", "CompositorNodeTree");
  nodes::node_tree_composit_default_init(C, node_group);

  if (!node_group->compositor_node_asset_traits) {
    node_group->compositor_node_asset_traits = MEM_new<CompositorNodeAssetTraits>(__func__);
  }
  node_group->compositor_node_asset_traits->flag |= COMPOSIT_NODE_ASSET_SCENE_EFFECT;
  bke::node_update_asset_metadata(*node_group);

  Scene *scene = CTX_data_scene(C);
  SceneCompositorEffect *active_effect = bke::compositor::get_active_effect(*scene);
  if (!active_effect) {
    SceneCompositorEffect &effect = bke::compositor::new_effect(*scene, "Effect");
    active_effect = &effect;
  }

  if (active_effect->node_group) {
    id_us_min(&active_effect->node_group->id);
  }
  active_effect->node_group = node_group;

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static void SCENE_OT_new_compositor_effect_node_group(wmOperatorType *ot)
{
  ot->name = "New Scene Compositor Effect Node Group";
  ot->idname = "SCENE_OT_new_compositor_effect_node_group";
  ot->description =
      "Create a new compositor node group and assign it to the active scene compositor effect";

  ot->exec = new_compositor_effect_node_group_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Compositor Effect Node Group Operator
 * \{ */

static wmOperatorStatus duplicate_compositor_effect_node_group_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SceneCompositorEffect *effect = bke::compositor::get_active_effect(*scene);
  if (!effect) {
    BKE_report(op->reports, RPT_ERROR, "No active effect to duplicate");
    return OPERATOR_CANCELLED;
  }

  bNodeTree *original_node_group = effect->node_group;
  if (!original_node_group || ID_MISSING(original_node_group)) {
    BKE_report(op->reports, RPT_ERROR, "No node group to duplicate");
    return OPERATOR_CANCELLED;
  }

  Main *main = CTX_data_main(C);
  bNodeTree *node_tree = id_cast<bNodeTree *>(
      BKE_id_copy_ex(main, &original_node_group->id, nullptr, LIB_ID_COPY_ACTIONS));

  effect->flags |= SceneCompositorEffectFlags::ShowNodeGroupSelector;

  effect->node_group = node_tree;
  id_us_min(&original_node_group->id);

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static void SCENE_OT_duplicate_compositor_effect_node_group(wmOperatorType *ot)
{
  ot->name = "Duplicate Compositor Effect Node Group";
  ot->idname = "SCENE_OT_duplicate_compositor_effect_node_group";
  ot->description =
      "Duplicate the active scene compositor effect node group and assign the new node group to "
      "the effect";

  ot->exec = duplicate_compositor_effect_node_group_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Compositor Effect Node Group Asset Operator
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
      ed::asset::operator_asset_reference_props_get_asset_from_all_library(C, ptr, reports);
  if (!asset) {
    return nullptr;
  }
  return reinterpret_cast<bNodeTree *>(ed::asset::asset_local_id_ensure_imported(bmain, *asset));
}

static bNodeTree *get_node_group(const bContext &C,
                                 PointerRNA &properties_ptr,
                                 ReportList *reports)
{
  bNodeTree *node_group = get_asset_or_local_node_group(C, properties_ptr, reports);
  if (!node_group || ID_MISSING(node_group)) {
    BKE_report(reports, RPT_ERROR, "Missing node group for asset");
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

static wmOperatorStatus add_compositor_effect_node_group_asset_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  bNodeTree *node_group = get_node_group(*C, *op->ptr, op->reports);
  if (!node_group) {
    return OPERATOR_CANCELLED;
  }

  SceneCompositorEffect &effect = bke::compositor::new_effect(*scene,
                                                              DATA_(node_group->id.name + 2));
  effect.node_group = node_group;
  id_us_plus(&node_group->id);
  effect.flags &= ~SceneCompositorEffectFlags::ShowNodeGroupSelector;

  Main &main = *CTX_data_main(C);
  bke::compositor::update_effect_node_group_interface(main, *scene, effect);

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COMPOSITOR);
  WM_event_add_notifier(C, NC_SCENE | ND_COMPO_RESULT, scene);

  return OPERATOR_FINISHED;
}

static std::string add_compositor_effect_node_group_asset_get_description(
    bContext *C, wmOperatorType * /*ot*/, PointerRNA *properties_ptr)
{
  const asset_system::AssetRepresentation *asset =
      ed::asset::operator_asset_reference_props_get_asset_from_all_library(
          *C, *properties_ptr, nullptr);
  if (!asset) {
    return "";
  }
  if (!asset->get_metadata().description) {
    return "";
  }
  return TIP_(asset->get_metadata().description);
}

static void SCENE_OT_add_compositor_effect_node_group_asset(wmOperatorType *ot)
{
  ot->name = "Add Scene Compositor Effect Node Group Asset";
  ot->description = "Add a scene compositor effect to the scene with a node group asset";
  ot->idname = "SCENE_OT_add_compositor_effect_node_group_asset";

  ot->exec = add_compositor_effect_node_group_asset_exec;
  ot->get_description = add_compositor_effect_node_group_asset_get_description;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ed::asset::operator_asset_reference_props_register(*ot->srna);
  WM_operator_properties_id_lookup(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Root Asset Catalogs Menu
 * \{ */

static ed::asset::AssetItemTree &get_static_item_tree()
{
  static ed::asset::AssetItemTree tree;
  return tree;
}

static ed::asset::AssetItemTree build_catalog_tree(const bContext &C)
{
  ed::asset::AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_NT;
  auto meta_data_filter = [&](const AssetMetaData &meta_data) {
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
    if (tree_type == nullptr || IDP_int_get(tree_type) != NTREE_COMPOSIT) {
      return false;
    }
    const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
        &meta_data, "compositor_node_asset_traits_flag");
    if (traits_flag == nullptr || !(IDP_int_get(traits_flag) & COMPOSIT_NODE_ASSET_SCENE_EFFECT)) {
      return false;
    }
    return true;
  };
  const AssetLibraryReference library = asset_system::all_library_reference();
  asset_system::all_library_reload_catalogs_if_dirty();
  return ed::asset::build_filtered_all_catalog_tree(
      library, C, type_filter, meta_data_filter, ntreeType_Composite->asset_catalog_path_prefix);
}

static bool unassigned_local_poll(const Main &bmain)
{
  for (const bNodeTree &group : bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (group.id.library_weak_reference || ID_IS_ASSET(&group.id)) {
      continue;
    }
    if (!group.compositor_node_asset_traits ||
        !(group.compositor_node_asset_traits->flag & COMPOSIT_NODE_ASSET_SCENE_EFFECT))
    {
      continue;
    }
    return true;
  }
  return false;
}

static void root_catalogs_draw(const bContext *C, Menu *menu)
{
  ui::Layout &layout = *menu->layout;

  AssetLibraryReference all_library_ref = asset_system::all_library_reference();
  const bool loading_finished = ed::asset::list::is_loaded(&all_library_ref);

  ed::asset::AssetItemTree &tree = get_static_item_tree();
  tree = build_catalog_tree(*C);
  if (tree.catalogs.is_empty() && loading_finished) {
    return;
  }

  layout.separator();

  if (!loading_finished) {
    layout.label(IFACE_("Loading Asset Libraries"), ICON_INFO);
  }

  tree.catalogs.foreach_root_item([&](const asset_system::AssetCatalogTreeItem &item) {
    ed::asset::draw_menu_for_catalog(
        item, "SCENE_MT_add_compositor_effect_catalog_assets", layout);
  });

  if (!tree.unassigned_assets.is_empty() || unassigned_local_poll(*CTX_data_main(C))) {
    layout.separator();
    layout.menu("SCENE_MT_add_compositor_effect_unassigned_assets",
                IFACE_("Unassigned"),
                ICON_FILE_HIDDEN);
  }
}

static MenuType SCENE_MT_add_compositor_effect_root_catalogs()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "SCENE_MT_add_compositor_effect_root_catalogs");
  type.draw = root_catalogs_draw;
  type.listener = ed::asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Catalog Assets Menu
 * \{ */

static void catalog_assets_draw(const bContext *C, Menu *menu)
{
  ed::asset::AssetItemTree &tree = get_static_item_tree();

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

  wmOperatorType *ot = WM_operatortype_find("SCENE_OT_add_compositor_effect_node_group_asset",
                                            true);
  for (const asset_system::AssetRepresentation *asset : assets) {
    ensure_separator();
    ed::asset::draw_asset_menu_item(asset, ot->idname, wm::OpCallContext::InvokeDefault, layout);
  }

  catalog_item->foreach_child([&](const asset_system::AssetCatalogTreeItem &item) {
    ensure_separator();
    ed::asset::draw_menu_for_catalog(
        item, "SCENE_MT_add_compositor_effect_catalog_assets", layout);
  });
}

static MenuType SCENE_MT_add_compositor_effect_catalog_assets()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "SCENE_MT_add_compositor_effect_catalog_assets");
  type.draw = catalog_assets_draw;
  type.listener = ed::asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unassigned Assets Menu
 * \{ */

static void unassigned_assets_draw(const bContext *C, Menu *menu)
{
  Main &bmain = *CTX_data_main(C);
  ed::asset::AssetItemTree &tree = get_static_item_tree();
  ui::Layout &layout = *menu->layout;
  wmOperatorType *ot = WM_operatortype_find("SCENE_OT_add_compositor_effect_node_group_asset",
                                            true);
  for (const asset_system::AssetRepresentation *asset : tree.unassigned_assets) {
    ed::asset::draw_asset_menu_item(asset, ot->idname, wm::OpCallContext::InvokeDefault, layout);
  }

  bool first = true;
  bool add_separator = !tree.unassigned_assets.is_empty();
  for (const bNodeTree &group : bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (group.id.library_weak_reference || ID_IS_ASSET(&group.id)) {
      continue;
    }
    if (!group.compositor_node_asset_traits ||
        !(group.compositor_node_asset_traits->flag & COMPOSIT_NODE_ASSET_SCENE_EFFECT))
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

static MenuType SCENE_MT_add_compositor_effect_unassigned_assets()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "SCENE_MT_add_compositor_effect_unassigned_assets");
  type.draw = unassigned_assets_draw;
  type.listener = ed::asset::list::asset_reading_region_listen_fn;
  type.description = N_(
      "Effect node group assets not assigned to a catalog.\n"
      "Catalogs can be assigned in the Asset Browser");
  return type;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Registration
 * \{ */

void ED_operatortypes_scene_compositor()
{
  WM_operatortype_append(SCENE_OT_add_compositor_effect);
  WM_operatortype_append(SCENE_OT_remove_compositor_effect);
  WM_operatortype_append(SCENE_OT_duplicate_compositor_effect);
  WM_operatortype_append(SCENE_OT_move_compositor_effect_to_index);
  WM_operatortype_append(SCENE_OT_set_active_compositor_effect);
  WM_operatortype_append(SCENE_OT_new_compositor_effect_node_group);
  WM_operatortype_append(SCENE_OT_duplicate_compositor_effect_node_group);
  WM_operatortype_append(SCENE_OT_add_compositor_effect_node_group_asset);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Registration
 * \{ */

void ED_menutypes_scene_compositor()
{
  WM_menutype_add(MEM_new<MenuType>(__func__, SCENE_MT_add_compositor_effect_root_catalogs()));
  WM_menutype_add(MEM_new<MenuType>(__func__, SCENE_MT_add_compositor_effect_catalog_assets()));
  WM_menutype_add(MEM_new<MenuType>(__func__, SCENE_MT_add_compositor_effect_unassigned_assets()));
}

/** \} */

}  // namespace blender
