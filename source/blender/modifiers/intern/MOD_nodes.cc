/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <fmt/format.h>
#include <sstream>
#include <string>
#include <xxhash.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_array_utils.hh"
#include "DNA_collection_types.h"
#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_bake_data_block_map.hh"
#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_packedFile.hh"
#include "BKE_pointcloud.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "BLO_read_write.hh"

#include "NOD_geometry_nodes_caller_ui.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"
#include "DEG_depsgraph_writeback_sync.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_nodes.hh"
#include "MOD_ui_common.hh"

#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_viewer_path.hh"

#include "NOD_geometry_nodes_dependencies.hh"
#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket_usage_inference.hh"

namespace lf = blender::fn::lazy_function;
namespace geo_log = blender::nodes::geo_eval_log;
namespace bake = blender::bke::bake;

namespace blender {

static void init_data(ModifierData *md)
{
  NodesModifierData *nmd = (NodesModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(nmd, modifier));
  nmd->modifier.layout_panel_open_flag |= 1 << NODES_MODIFIER_PANEL_WARNINGS;

  MEMCPY_STRUCT_AFTER(nmd, DNA_struct_default_get(NodesModifierData), modifier);
  nmd->runtime = MEM_new<NodesModifierRuntime>(__func__);
  nmd->runtime->cache = std::make_shared<bake::ModifierCache>();
}

static void find_dependencies_from_settings(const NodesModifierSettings &settings,
                                            nodes::GeometryNodesEvalDependencies &deps)
{
  IDP_foreach_property(settings.properties, IDP_TYPE_FILTER_ID, [&](IDProperty *property) {
    if (ID *id = IDP_ID_get(property)) {
      deps.add_generic_id_full(id);
    }
  });
}

/* We don't know exactly what attributes from the other object we will need. */
static const CustomData_MeshMasks dependency_data_mask{CD_MASK_PROP_ALL | CD_MASK_MDEFORMVERT,
                                                       CD_MASK_PROP_ALL,
                                                       CD_MASK_PROP_ALL,
                                                       CD_MASK_PROP_ALL,
                                                       CD_MASK_PROP_ALL};

static void add_collection_relation(const ModifierUpdateDepsgraphContext *ctx,
                                    Collection &collection)
{
  DEG_add_collection_geometry_relation(ctx->node, &collection, "Nodes Modifier");
  DEG_add_collection_geometry_customdata_mask(ctx->node, &collection, &dependency_data_mask);
}

static void add_object_relation(
    const ModifierUpdateDepsgraphContext *ctx,
    Object &object,
    const nodes::GeometryNodesEvalDependencies::ObjectDependencyInfo &info)
{
  if (info.transform) {
    DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_TRANSFORM, "Nodes Modifier");
  }
  if (&(ID &)object == &ctx->object->id) {
    return;
  }
  if (info.geometry) {
    if (object.type == OB_EMPTY && object.instance_collection != nullptr) {
      add_collection_relation(ctx, *object.instance_collection);
    }
    else if (DEG_object_has_geometry_component(&object)) {
      DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_GEOMETRY, "Nodes Modifier");
      DEG_add_customdata_mask(ctx->node, &object, &dependency_data_mask);
    }
  }
  if (object.type == OB_CAMERA) {
    if (info.camera_parameters) {
      DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_PARAMETERS, "Nodes Modifier");
    }
  }
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return;
  }
  if (ID_MISSING(nmd->node_group)) {
    return;
  }

  DEG_add_node_tree_output_relation(ctx->node, nmd->node_group, "Nodes Modifier");

  nodes::GeometryNodesEvalDependencies eval_deps =
      nodes::gather_geometry_nodes_eval_dependencies_recursive(*nmd->node_group);

  /* Create dependencies to data-blocks referenced by the settings in the modifier. */
  find_dependencies_from_settings(nmd->settings, eval_deps);

  if (ctx->object->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(ctx->object->data);
    if (curves_id->surface != nullptr) {
      eval_deps.add_object(curves_id->surface);
    }
  }

  for (const NodesModifierBake &bake : Span(nmd->bakes, nmd->bakes_num)) {
    for (const NodesModifierDataBlock &data_block : Span(bake.data_blocks, bake.data_blocks_num)) {
      if (data_block.id) {
        eval_deps.add_generic_id_full(data_block.id);
      }
    }
  }

  for (ID *id : eval_deps.ids.values()) {
    switch ((ID_Type)GS(id->name)) {
      case ID_OB: {
        Object *object = reinterpret_cast<Object *>(id);
        add_object_relation(
            ctx, *object, eval_deps.objects_info.lookup_default(object->id.session_uid, {}));
        break;
      }
      case ID_GR: {
        Collection *collection = reinterpret_cast<Collection *>(id);
        add_collection_relation(ctx, *collection);
        break;
      }
      case ID_IM:
      case ID_TE: {
        DEG_add_generic_id_relation(ctx->node, id, "Nodes Modifier");
        break;
      }
      default: {
        /* Purposefully don't add relations for materials. While there are material sockets,
         * the pointers are only passed around as handles rather than dereferenced. */
        break;
      }
    }
  }

  if (eval_deps.needs_own_transform) {
    DEG_add_depends_on_transform_relation(ctx->node, "Nodes Modifier");
  }
  if (eval_deps.needs_active_camera) {
    DEG_add_scene_camera_relation(ctx->node, ctx->scene, DEG_OB_COMP_TRANSFORM, "Nodes Modifier");
  }
  /* Active camera is a scene parameter that can change, so we need a relation for that, too. */
  if (eval_deps.needs_active_camera || eval_deps.needs_scene_render_params) {
    DEG_add_scene_relation(ctx->node, ctx->scene, DEG_SCENE_COMP_PARAMETERS, "Nodes Modifier");
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  const bNodeTree *tree = nmd->node_group;
  if (tree == nullptr) {
    return false;
  }
  if (ID_MISSING(tree)) {
    return false;
  }
  for (const NodesModifierBake &bake : Span(nmd->bakes, nmd->bakes_num)) {
    if (bake.bake_mode == NODES_MODIFIER_BAKE_MODE_ANIMATION) {
      return true;
    }
  }
  nodes::GeometryNodesEvalDependencies eval_deps =
      nodes::gather_geometry_nodes_eval_dependencies_recursive(*nmd->node_group);
  return eval_deps.time_dependent;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  walk(user_data, ob, (ID **)&nmd->node_group, IDWALK_CB_USER);

  IDP_foreach_property(nmd->settings.properties, IDP_TYPE_FILTER_ID, [&](IDProperty *id_prop) {
    walk(user_data, ob, (ID **)&id_prop->data.pointer, IDWALK_CB_USER);
  });

  for (NodesModifierBake &bake : MutableSpan(nmd->bakes, nmd->bakes_num)) {
    for (NodesModifierDataBlock &data_block : MutableSpan(bake.data_blocks, bake.data_blocks_num))
    {
      walk(user_data, ob, &data_block.id, IDWALK_CB_USER);
    }
  }
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "texture");
  walk(user_data, ob, md, &ptr, prop);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  if (nmd->node_group == nullptr) {
    return true;
  }

  return false;
}

static bool logging_enabled(const ModifierEvalContext *ctx)
{
  if (!DEG_is_active(ctx->depsgraph)) {
    return false;
  }
  if ((ctx->flag & MOD_APPLY_ORCO) != 0) {
    return false;
  }
  return true;
}

static void update_id_properties_from_node_group(NodesModifierData *nmd)
{
  if (nmd->node_group == nullptr) {
    if (nmd->settings.properties) {
      IDP_FreeProperty(nmd->settings.properties);
      nmd->settings.properties = nullptr;
    }
    return;
  }

  IDProperty *old_properties = nmd->settings.properties;
  nmd->settings.properties = bke::idprop::create_group("Nodes Modifier Settings").release();
  IDProperty *new_properties = nmd->settings.properties;

  nodes::update_input_properties_from_node_tree(*nmd->node_group, old_properties, *new_properties);
  nodes::update_output_properties_from_node_tree(
      *nmd->node_group, old_properties, *new_properties);

  if (old_properties != nullptr) {
    IDP_FreeProperty(old_properties);
  }
}

static void remove_outdated_bake_caches(NodesModifierData &nmd)
{
  if (!nmd.runtime->cache) {
    if (nmd.bakes_num == 0) {
      return;
    }
    nmd.runtime->cache = std::make_shared<bake::ModifierCache>();
  }
  bake::ModifierCache &modifier_cache = *nmd.runtime->cache;
  std::lock_guard lock{modifier_cache.mutex};

  Set<int> existing_bake_ids;
  for (const NodesModifierBake &bake : Span{nmd.bakes, nmd.bakes_num}) {
    existing_bake_ids.add(bake.id);
  }

  auto remove_predicate = [&](auto item) { return !existing_bake_ids.contains(item.key); };

  modifier_cache.bake_cache_by_id.remove_if(remove_predicate);
  modifier_cache.simulation_cache_by_id.remove_if(remove_predicate);
}

static void update_bakes_from_node_group(NodesModifierData &nmd)
{
  Map<int, NodesModifierBake *> old_bake_by_id;
  for (NodesModifierBake &bake : MutableSpan(nmd.bakes, nmd.bakes_num)) {
    old_bake_by_id.add(bake.id, &bake);
  }

  Vector<int> new_bake_ids;
  if (nmd.node_group) {
    for (const bNestedNodeRef &ref : nmd.node_group->nested_node_refs_span()) {
      const bNode *node = nmd.node_group->find_nested_node(ref.id);
      if (node) {
        if (ELEM(node->type_legacy, GEO_NODE_SIMULATION_OUTPUT, GEO_NODE_BAKE)) {
          new_bake_ids.append(ref.id);
        }
      }
      else if (old_bake_by_id.contains(ref.id)) {
        /* Keep baked data in case linked data is missing so that it still exists when the linked
         * data has been found. */
        new_bake_ids.append(ref.id);
      }
    }
  }

  NodesModifierBake *new_bake_data = MEM_calloc_arrayN<NodesModifierBake>(new_bake_ids.size(),
                                                                          __func__);
  for (const int i : new_bake_ids.index_range()) {
    const int id = new_bake_ids[i];
    NodesModifierBake *old_bake = old_bake_by_id.lookup_default(id, nullptr);
    NodesModifierBake &new_bake = new_bake_data[i];
    if (old_bake) {
      new_bake = *old_bake;
      /* The ownership of this data was moved to `new_bake`. */
      old_bake->directory = nullptr;
      old_bake->data_blocks = nullptr;
      old_bake->data_blocks_num = 0;
      old_bake->packed = nullptr;
    }
    else {
      new_bake.id = id;
      new_bake.frame_start = 1;
      new_bake.frame_end = 100;
      new_bake.bake_mode = NODES_MODIFIER_BAKE_MODE_STILL;
    }
  }

  for (NodesModifierBake &old_bake : MutableSpan(nmd.bakes, nmd.bakes_num)) {
    nodes_modifier_bake_destruct(&old_bake, true);
  }
  MEM_SAFE_FREE(nmd.bakes);

  nmd.bakes = new_bake_data;
  nmd.bakes_num = new_bake_ids.size();

  remove_outdated_bake_caches(nmd);
}

static void update_panels_from_node_group(NodesModifierData &nmd)
{
  Map<int, NodesModifierPanel *> old_panel_by_id;
  for (NodesModifierPanel &panel : MutableSpan(nmd.panels, nmd.panels_num)) {
    old_panel_by_id.add(panel.id, &panel);
  }

  Vector<const bNodeTreeInterfacePanel *> interface_panels;
  if (nmd.node_group) {
    nmd.node_group->ensure_interface_cache();
    nmd.node_group->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
      if (item.item_type != NODE_INTERFACE_PANEL) {
        return true;
      }
      interface_panels.append(reinterpret_cast<const bNodeTreeInterfacePanel *>(&item));
      return true;
    });
  }

  NodesModifierPanel *new_panels = MEM_calloc_arrayN<NodesModifierPanel>(interface_panels.size(),
                                                                         __func__);

  for (const int i : interface_panels.index_range()) {
    const bNodeTreeInterfacePanel &interface_panel = *interface_panels[i];
    const int id = interface_panel.identifier;
    NodesModifierPanel *old_panel = old_panel_by_id.lookup_default(id, nullptr);
    NodesModifierPanel &new_panel = new_panels[i];
    if (old_panel) {
      new_panel = *old_panel;
    }
    else {
      new_panel.id = id;
      const bool default_closed = interface_panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED;
      SET_FLAG_FROM_TEST(new_panel.flag, !default_closed, NODES_MODIFIER_PANEL_OPEN);
    }
  }

  MEM_SAFE_FREE(nmd.panels);

  nmd.panels = new_panels;
  nmd.panels_num = interface_panels.size();
}

}  // namespace blender

void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd)
{
  using namespace blender;
  update_id_properties_from_node_group(nmd);
  update_bakes_from_node_group(*nmd);
  update_panels_from_node_group(*nmd);
  nmd->runtime->usage_cache.reset();

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
}

NodesModifierBake *NodesModifierData::find_bake(const int id)
{
  return const_cast<NodesModifierBake *>(std::as_const(*this).find_bake(id));
}

const NodesModifierBake *NodesModifierData::find_bake(const int id) const
{
  for (const NodesModifierBake &bake : blender::Span{this->bakes, this->bakes_num}) {
    if (bake.id == id) {
      return &bake;
    }
  }
  return nullptr;
}

namespace blender {

/**
 * Setup side effects nodes so that the given node in the given compute context will be executed.
 * To make sure that it is executed, all parent group nodes and zones have to be set to have side
 * effects as well.
 */
static void try_add_side_effect_node(const ModifierEvalContext &ctx,
                                     const ComputeContext &final_compute_context,
                                     const int final_node_id,
                                     const NodesModifierData &nmd,
                                     nodes::GeoNodesSideEffectNodes &r_side_effect_nodes)
{
  if (nmd.node_group == nullptr) {
    return;
  }

  Vector<const ComputeContext *> compute_context_vec;
  for (const ComputeContext *c = &final_compute_context; c; c = c->parent()) {
    compute_context_vec.append(c);
  }
  std::reverse(compute_context_vec.begin(), compute_context_vec.end());

  const auto *modifier_compute_context = dynamic_cast<const bke::ModifierComputeContext *>(
      compute_context_vec[0]);
  if (modifier_compute_context == nullptr) {
    return;
  }
  if (modifier_compute_context->modifier_uid() != nmd.modifier.persistent_uid) {
    return;
  }

  const bNodeTree *current_tree = nmd.node_group;
  const bke::bNodeTreeZone *current_zone = nullptr;

  /* Write side effect nodes to a new map and only if everything succeeds, move the nodes to the
   * caller. This is easier than changing r_side_effect_nodes directly and then undoing changes in
   * case of errors. */
  nodes::GeoNodesSideEffectNodes local_side_effect_nodes;
  for (const ComputeContext *compute_context_generic : compute_context_vec.as_span().drop_front(1))
  {
    const bke::bNodeTreeZones *current_zones = current_tree->zones();
    if (current_zones == nullptr) {
      return;
    }
    const auto *lf_graph_info = nodes::ensure_geometry_nodes_lazy_function_graph(*current_tree);
    if (lf_graph_info == nullptr) {
      return;
    }
    const ComputeContextHash &parent_compute_context_hash =
        compute_context_generic->parent()->hash();
    if (const auto *compute_context = dynamic_cast<const bke::SimulationZoneComputeContext *>(
            compute_context_generic))
    {
      const bke::bNodeTreeZone *simulation_zone = current_zones->get_zone_by_node(
          compute_context->output_node_id());
      if (simulation_zone == nullptr) {
        return;
      }
      if (simulation_zone->parent_zone != current_zone) {
        return;
      }
      const lf::FunctionNode *lf_zone_node = lf_graph_info->mapping.zone_node_map.lookup_default(
          simulation_zone, nullptr);
      if (lf_zone_node == nullptr) {
        return;
      }
      const lf::FunctionNode *lf_simulation_output_node =
          lf_graph_info->mapping.possible_side_effect_node_map.lookup_default(
              simulation_zone->output_node(), nullptr);
      if (lf_simulation_output_node == nullptr) {
        return;
      }
      local_side_effect_nodes.nodes_by_context.add(parent_compute_context_hash, lf_zone_node);
      /* By making the simulation output node a side-effect-node, we can ensure that the simulation
       * runs when it contains an active viewer. */
      local_side_effect_nodes.nodes_by_context.add(compute_context_generic->hash(),
                                                   lf_simulation_output_node);

      current_zone = simulation_zone;
    }
    else if (const auto *compute_context = dynamic_cast<const bke::RepeatZoneComputeContext *>(
                 compute_context_generic))
    {
      const bke::bNodeTreeZone *repeat_zone = current_zones->get_zone_by_node(
          compute_context->output_node_id());
      if (repeat_zone == nullptr) {
        return;
      }
      if (repeat_zone->parent_zone != current_zone) {
        return;
      }
      const lf::FunctionNode *lf_zone_node = lf_graph_info->mapping.zone_node_map.lookup_default(
          repeat_zone, nullptr);
      if (lf_zone_node == nullptr) {
        return;
      }
      local_side_effect_nodes.nodes_by_context.add(parent_compute_context_hash, lf_zone_node);
      local_side_effect_nodes.iterations_by_iteration_zone.add(
          {parent_compute_context_hash, compute_context->output_node_id()},
          compute_context->iteration());
      current_zone = repeat_zone;
    }
    else if (const auto *compute_context =
                 dynamic_cast<const bke::ForeachGeometryElementZoneComputeContext *>(
                     compute_context_generic))
    {
      const bke::bNodeTreeZone *foreach_zone = current_zones->get_zone_by_node(
          compute_context->output_node_id());
      if (foreach_zone == nullptr) {
        return;
      }
      if (foreach_zone->parent_zone != current_zone) {
        return;
      }
      const lf::FunctionNode *lf_zone_node = lf_graph_info->mapping.zone_node_map.lookup_default(
          foreach_zone, nullptr);
      if (lf_zone_node == nullptr) {
        return;
      }
      local_side_effect_nodes.nodes_by_context.add(parent_compute_context_hash, lf_zone_node);
      local_side_effect_nodes.iterations_by_iteration_zone.add(
          {parent_compute_context_hash, compute_context->output_node_id()},
          compute_context->index());
      current_zone = foreach_zone;
    }
    else if (const auto *compute_context = dynamic_cast<const bke::GroupNodeComputeContext *>(
                 compute_context_generic))
    {
      const bNode *group_node = current_tree->node_by_id(compute_context->node_id());
      if (group_node == nullptr) {
        return;
      }
      if (group_node->id == nullptr) {
        return;
      }
      if (group_node->is_muted()) {
        return;
      }
      if (current_zone != current_zones->get_zone_by_node(group_node->identifier)) {
        return;
      }
      const lf::FunctionNode *lf_group_node = lf_graph_info->mapping.group_node_map.lookup_default(
          group_node, nullptr);
      if (lf_group_node == nullptr) {
        return;
      }
      local_side_effect_nodes.nodes_by_context.add(parent_compute_context_hash, lf_group_node);
      current_tree = reinterpret_cast<const bNodeTree *>(group_node->id);
      current_zone = nullptr;
    }
    else if (const auto *compute_context =
                 dynamic_cast<const bke::EvaluateClosureComputeContext *>(compute_context_generic))
    {
      const bNode *evaluate_node = current_tree->node_by_id(compute_context->node_id());
      if (!evaluate_node) {
        return;
      }
      if (evaluate_node->is_muted()) {
        return;
      }
      if (current_zone != current_zones->get_zone_by_node(evaluate_node->identifier)) {
        return;
      }
      const std::optional<nodes::ClosureSourceLocation> &source_location =
          compute_context->closure_source_location();
      if (!source_location) {
        return;
      }
      if (!source_location->tree->zones()) {
        return;
      }
      const lf::FunctionNode *lf_evaluate_node =
          lf_graph_info->mapping.possible_side_effect_node_map.lookup_default(evaluate_node,
                                                                              nullptr);
      if (!lf_evaluate_node) {
        return;
      }
      /* The tree may sometimes be original and sometimes evaluated, depending on the source of the
       * compute context. */
      const bNodeTree *eval_closure_tree = DEG_is_evaluated(source_location->tree) ?
                                               source_location->tree :
                                               reinterpret_cast<const bNodeTree *>(
                                                   DEG_get_evaluated_id(
                                                       ctx.depsgraph, &source_location->tree->id));
      const bNode *closure_output_node = eval_closure_tree->node_by_id(
          source_location->closure_output_node_id);
      if (!closure_output_node) {
        return;
      }
      local_side_effect_nodes.nodes_by_context.add(parent_compute_context_hash, lf_evaluate_node);
      current_tree = eval_closure_tree;
      current_zone = eval_closure_tree->zones()->get_zone_by_node(closure_output_node->identifier);
    }
    else {
      return;
    }
  }
  const bNode *final_node = current_tree->node_by_id(final_node_id);
  if (final_node == nullptr) {
    return;
  }
  const auto *lf_graph_info = nodes::ensure_geometry_nodes_lazy_function_graph(*current_tree);
  if (lf_graph_info == nullptr) {
    return;
  }
  const bke::bNodeTreeZones *tree_zones = current_tree->zones();
  if (tree_zones == nullptr) {
    return;
  }
  if (tree_zones->get_zone_by_node(final_node_id) != current_zone) {
    return;
  }
  const lf::FunctionNode *lf_node =
      lf_graph_info->mapping.possible_side_effect_node_map.lookup_default(final_node, nullptr);
  if (lf_node == nullptr) {
    return;
  }
  local_side_effect_nodes.nodes_by_context.add(final_compute_context.hash(), lf_node);

  /* Successfully found all side effect nodes for the viewer path. */
  for (const auto item : local_side_effect_nodes.nodes_by_context.items()) {
    r_side_effect_nodes.nodes_by_context.add_multiple(item.key, item.value);
  }
  for (const auto item : local_side_effect_nodes.iterations_by_iteration_zone.items()) {
    r_side_effect_nodes.iterations_by_iteration_zone.add_multiple(item.key, item.value);
  }
}

static void find_side_effect_nodes_for_viewer_path(
    const ViewerPath &viewer_path,
    const NodesModifierData &nmd,
    const ModifierEvalContext &ctx,
    nodes::GeoNodesSideEffectNodes &r_side_effect_nodes)
{
  const std::optional<ed::viewer_path::ViewerPathForGeometryNodesViewer> parsed_path =
      ed::viewer_path::parse_geometry_nodes_viewer(viewer_path);
  if (!parsed_path.has_value()) {
    return;
  }
  if (parsed_path->object != DEG_get_original(ctx.object)) {
    return;
  }
  if (parsed_path->modifier_uid != nmd.modifier.persistent_uid) {
    return;
  }

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current = &compute_context_cache.for_modifier(nullptr, nmd);
  for (const ViewerPathElem *elem : parsed_path->node_path) {
    current = ed::viewer_path::compute_context_for_viewer_path_elem(
        *elem, compute_context_cache, current);
    if (!current) {
      return;
    }
  }

  try_add_side_effect_node(ctx, *current, parsed_path->viewer_node_id, nmd, r_side_effect_nodes);
}

static void find_side_effect_nodes_for_nested_node(
    const ModifierEvalContext &ctx,
    const NodesModifierData &nmd,
    const int root_nested_node_id,
    nodes::GeoNodesSideEffectNodes &r_side_effect_nodes)
{
  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *compute_context = &compute_context_cache.for_modifier(nullptr, nmd);

  int nested_node_id = root_nested_node_id;
  const bNodeTree *tree = nmd.node_group;
  while (true) {
    const bNestedNodeRef *ref = tree->find_nested_node_ref(nested_node_id);
    if (!ref) {
      return;
    }
    const bNode *node = tree->node_by_id(ref->path.node_id);
    if (!node) {
      return;
    }
    const bke::bNodeTreeZones *zones = tree->zones();
    if (!zones) {
      return;
    }
    if (zones->get_zone_by_node(node->identifier) != nullptr) {
      /* Only top level nodes are allowed here. */
      return;
    }
    if (node->is_group()) {
      if (!node->id) {
        return;
      }
      compute_context = &compute_context_cache.for_group_node(
          compute_context, node->identifier, tree);
      tree = reinterpret_cast<const bNodeTree *>(node->id);
      nested_node_id = ref->path.id_in_node;
    }
    else {
      try_add_side_effect_node(ctx, *compute_context, ref->path.node_id, nmd, r_side_effect_nodes);
      return;
    }
  }
}

/**
 * This ensures that nodes that the user wants to bake are actually evaluated. Otherwise they might
 * not be if they are not connected to the output.
 */
static void find_side_effect_nodes_for_baking(const NodesModifierData &nmd,
                                              const ModifierEvalContext &ctx,
                                              nodes::GeoNodesSideEffectNodes &r_side_effect_nodes)
{
  if (!nmd.runtime->cache) {
    return;
  }
  if (!DEG_is_active(ctx.depsgraph)) {
    /* Only the active depsgraph can bake. */
    return;
  }
  bake::ModifierCache &modifier_cache = *nmd.runtime->cache;
  for (const bNestedNodeRef &ref : nmd.node_group->nested_node_refs_span()) {
    if (!modifier_cache.requested_bakes.contains(ref.id)) {
      continue;
    }
    find_side_effect_nodes_for_nested_node(ctx, nmd, ref.id, r_side_effect_nodes);
  }
}

static void find_side_effect_nodes_for_active_gizmos(
    const NodesModifierData &nmd,
    const ModifierEvalContext &ctx,
    const wmWindowManager &wm,
    nodes::GeoNodesSideEffectNodes &r_side_effect_nodes,
    Set<ComputeContextHash> &r_socket_log_contexts)
{
  Object *object_orig = DEG_get_original(ctx.object);
  const NodesModifierData &nmd_orig = *reinterpret_cast<const NodesModifierData *>(
      BKE_modifier_get_original(ctx.object, const_cast<ModifierData *>(&nmd.modifier)));
  bke::ComputeContextCache compute_context_cache;
  nodes::gizmos::foreach_active_gizmo_in_modifier(
      *object_orig,
      nmd_orig,
      wm,
      compute_context_cache,
      [&](const ComputeContext &compute_context,
          const bNode &gizmo_node,
          const bNodeSocket &gizmo_socket) {
        try_add_side_effect_node(
            ctx, compute_context, gizmo_node.identifier, nmd, r_side_effect_nodes);
        r_socket_log_contexts.add(compute_context.hash());

        nodes::gizmos::foreach_compute_context_on_gizmo_path(
            compute_context, gizmo_node, gizmo_socket, [&](const ComputeContext &node_context) {
              /* Make sure that all intermediate sockets are logged. This is necessary to be able
               * to evaluate the nodes in reverse for the gizmo. */
              r_socket_log_contexts.add(node_context.hash());
            });
      });
}

static void find_side_effect_nodes(const NodesModifierData &nmd,
                                   const ModifierEvalContext &ctx,
                                   nodes::GeoNodesSideEffectNodes &r_side_effect_nodes,
                                   Set<ComputeContextHash> &r_socket_log_contexts)
{
  Main *bmain = DEG_get_bmain(ctx.depsgraph);
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return;
  }
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    const WorkSpace *workspace = BKE_workspace_active_get(window->workspace_hook);
    find_side_effect_nodes_for_viewer_path(workspace->viewer_path, nmd, ctx, r_side_effect_nodes);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      const SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype == SPACE_SPREADSHEET) {
        const SpaceSpreadsheet &sspreadsheet = *reinterpret_cast<const SpaceSpreadsheet *>(sl);
        find_side_effect_nodes_for_viewer_path(
            sspreadsheet.geometry_id.viewer_path, nmd, ctx, r_side_effect_nodes);
      }
      if (sl->spacetype == SPACE_VIEW3D) {
        const View3D &v3d = *reinterpret_cast<const View3D *>(sl);
        find_side_effect_nodes_for_viewer_path(v3d.viewer_path, nmd, ctx, r_side_effect_nodes);
      }
    }
  }

  find_side_effect_nodes_for_baking(nmd, ctx, r_side_effect_nodes);
  find_side_effect_nodes_for_active_gizmos(
      nmd, ctx, *wm, r_side_effect_nodes, r_socket_log_contexts);
}

static void find_socket_log_contexts(const NodesModifierData &nmd,
                                     const ModifierEvalContext &ctx,
                                     Set<ComputeContextHash> &r_socket_log_contexts)
{
  Main *bmain = DEG_get_bmain(ctx.depsgraph);
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return;
  }
  bke::ComputeContextCache compute_context_cache;
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      const SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype == SPACE_NODE) {
        const SpaceNode &snode = *reinterpret_cast<const SpaceNode *>(sl);
        if (snode.edittree == nullptr || snode.edittree->type != NTREE_GEOMETRY) {
          continue;
        }
        if (!ed::space_node::node_editor_is_for_geometry_nodes_modifier(snode, *ctx.object, nmd)) {
          continue;
        }
        const Map<const bke::bNodeTreeZone *, ComputeContextHash> hash_by_zone =
            geo_log::GeoNodesLog::get_context_hash_by_zone_for_node_editor(snode,
                                                                           compute_context_cache);
        for (const ComputeContextHash &hash : hash_by_zone.values()) {
          r_socket_log_contexts.add(hash);
        }
      }
    }
  }
}

/**
 * \note This could be done in #initialize_group_input, though that would require adding the
 * the object as a parameter, so it's likely better to this check as a separate step.
 */
static void check_property_socket_sync(const Object *ob,
                                       const IDProperty *properties,
                                       ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  int geometry_socket_count = 0;

  nmd->node_group->ensure_interface_cache();
  const Span<nodes::StructureType> input_structure_types =
      nmd->node_group->runtime->structure_type_interface->inputs;
  for (const int i : nmd->node_group->interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket *socket = nmd->node_group->interface_inputs()[i];
    const bke::bNodeSocketType *typeinfo = socket->socket_typeinfo();
    const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (type == SOCK_GEOMETRY) {
      geometry_socket_count++;
    }
    /* The first socket is the special geometry socket for the modifier object. */
    if (i == 0 && type == SOCK_GEOMETRY) {
      continue;
    }
    if (ELEM(input_structure_types[i], nodes::StructureType::Grid, nodes::StructureType::List)) {
      continue;
    }

    IDProperty *property = IDP_GetPropertyFromGroup_null(properties, socket->identifier);
    if (property == nullptr) {
      if (!ELEM(type, SOCK_GEOMETRY, SOCK_MATRIX, SOCK_BUNDLE, SOCK_CLOSURE)) {
        BKE_modifier_set_error(
            ob, md, "Missing property for input socket \"%s\"", socket->name ? socket->name : "");
      }
      continue;
    }

    if (!nodes::id_property_type_matches_socket(*socket, *property)) {
      BKE_modifier_set_error(ob,
                             md,
                             "Property type does not match input socket \"(%s)\"",
                             socket->name ? socket->name : "");
      continue;
    }
  }

  if (geometry_socket_count == 1) {
    const bNodeTreeInterfaceSocket *first_socket = nmd->node_group->interface_inputs()[0];
    const bke::bNodeSocketType *typeinfo = first_socket->socket_typeinfo();
    const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (type != SOCK_GEOMETRY) {
      BKE_modifier_set_error(ob, md, "Node group's geometry input must be the first");
    }
  }
}

class NodesModifierBakeDataBlockMap : public bake::BakeDataBlockMap {
  /** Protects access to `new_mappings` which may be added to from multiple threads. */
  Mutex mutex_;

 public:
  Map<bake::BakeDataBlockID, ID *> old_mappings;
  Map<bake::BakeDataBlockID, ID *> new_mappings;

  ID *lookup_or_remember_missing(const bake::BakeDataBlockID &key) override
  {
    if (ID *id = this->old_mappings.lookup_default(key, nullptr)) {
      return id;
    }
    if (this->old_mappings.contains(key)) {
      /* Don't allow overwriting old mappings. */
      return nullptr;
    }
    std::lock_guard lock{mutex_};
    return this->new_mappings.lookup_or_add(key, nullptr);
  }

  void try_add(ID &id) override
  {
    bake::BakeDataBlockID key{id};
    if (this->old_mappings.contains(key)) {
      return;
    }
    std::lock_guard lock{mutex_};
    this->new_mappings.add_overwrite(std::move(key), &id);
  }

 private:
  ID *lookup_in_map(Map<bake::BakeDataBlockID, ID *> &map,
                    const bake::BakeDataBlockID &key,
                    const std::optional<ID_Type> &type)
  {
    ID *id = map.lookup_default(key, nullptr);
    if (!id) {
      return nullptr;
    }
    if (type && GS(id->name) != *type) {
      return nullptr;
    }
    return id;
  }
};

namespace sim_input = nodes::sim_input;
namespace sim_output = nodes::sim_output;

struct BakeFrameIndices {
  std::optional<int> prev;
  std::optional<int> current;
  std::optional<int> next;
};

static BakeFrameIndices get_bake_frame_indices(
    const Span<std::unique_ptr<bake::FrameCache>> frame_caches, const SubFrame frame)
{
  BakeFrameIndices frame_indices;
  if (!frame_caches.is_empty()) {
    const int first_future_frame_index = binary_search::first_if(
        frame_caches,
        [&](const std::unique_ptr<bake::FrameCache> &value) { return value->frame > frame; });
    frame_indices.next = (first_future_frame_index == frame_caches.size()) ?
                             std::nullopt :
                             std::optional<int>(first_future_frame_index);
    if (first_future_frame_index > 0) {
      const int index = first_future_frame_index - 1;
      if (frame_caches[index]->frame < frame) {
        frame_indices.prev = index;
      }
      else {
        BLI_assert(frame_caches[index]->frame == frame);
        frame_indices.current = index;
        if (index > 0) {
          frame_indices.prev = index - 1;
        }
      }
    }
  }
  return frame_indices;
}

static void ensure_bake_loaded(bake::NodeBakeCache &bake_cache, bake::FrameCache &frame_cache)
{
  if (!frame_cache.state.items_by_id.is_empty()) {
    return;
  }
  if (!frame_cache.meta_data_source.has_value()) {
    return;
  }
  if (bake_cache.memory_blob_reader) {
    if (const auto *meta_buffer = std::get_if<Span<std::byte>>(&*frame_cache.meta_data_source)) {
      const std::string meta_str{reinterpret_cast<const char *>(meta_buffer->data()),
                                 size_t(meta_buffer->size())};
      std::istringstream meta_stream{meta_str};
      std::optional<bake::BakeState> bake_state = bake::deserialize_bake(
          meta_stream, *bake_cache.memory_blob_reader, *bake_cache.blob_sharing);
      if (!bake_state.has_value()) {
        return;
      }
      frame_cache.state = std::move(*bake_state);
      return;
    }
  }
  if (!bake_cache.blobs_dir) {
    return;
  }
  const auto *meta_path = std::get_if<std::string>(&*frame_cache.meta_data_source);
  if (!meta_path) {
    return;
  }
  bake::DiskBlobReader blob_reader{*bake_cache.blobs_dir};
  fstream meta_file{*meta_path};
  std::optional<bake::BakeState> bake_state = bake::deserialize_bake(
      meta_file, blob_reader, *bake_cache.blob_sharing);
  if (!bake_state.has_value()) {
    return;
  }
  frame_cache.state = std::move(*bake_state);
}

static bool try_find_baked_data(const NodesModifierBake &bake,
                                bake::NodeBakeCache &bake_cache,
                                const Main &bmain,
                                const Object &object,
                                const NodesModifierData &nmd,
                                const int id)
{
  if (bake.packed) {
    if (bake.packed->meta_files_num == 0) {
      return false;
    }
    bake_cache.reset();
    Map<SubFrame, const NodesModifierBakeFile *> file_by_frame;
    for (const NodesModifierBakeFile &meta_file :
         Span{bake.packed->meta_files, bake.packed->meta_files_num})
    {
      const std::optional<SubFrame> frame = bake::file_name_to_frame(meta_file.name);
      if (!frame) {
        return false;
      }
      if (!file_by_frame.add(*frame, &meta_file)) {
        /* Can only have on file per (sub)frame. */
        return false;
      }
    }
    /* Make sure frames processed in the right order. */
    Vector<SubFrame> frames;
    frames.extend(file_by_frame.keys().begin(), file_by_frame.keys().end());

    for (const SubFrame &frame : frames) {
      const NodesModifierBakeFile &meta_file = *file_by_frame.lookup(frame);
      auto frame_cache = std::make_unique<bake::FrameCache>();
      frame_cache->frame = frame;
      frame_cache->meta_data_source = meta_file.data();
      bake_cache.frames.append(std::move(frame_cache));
    }

    bake_cache.memory_blob_reader = std::make_unique<bake::MemoryBlobReader>();
    for (const NodesModifierBakeFile &blob_file :
         Span{bake.packed->blob_files, bake.packed->blob_files_num})
    {
      bake_cache.memory_blob_reader->add(blob_file.name, blob_file.data());
    }
    bake_cache.blob_sharing = std::make_unique<bake::BlobReadSharing>();
    return true;
  }

  std::optional<bake::BakePath> bake_path = bake::get_node_bake_path(bmain, object, nmd, id);
  if (!bake_path) {
    return false;
  }
  Vector<bake::MetaFile> meta_files = bake::find_sorted_meta_files(bake_path->meta_dir);
  if (meta_files.is_empty()) {
    return false;
  }
  bake_cache.reset();
  for (const bake::MetaFile &meta_file : meta_files) {
    auto frame_cache = std::make_unique<bake::FrameCache>();
    frame_cache->frame = meta_file.frame;
    frame_cache->meta_data_source = meta_file.path;
    bake_cache.frames.append(std::move(frame_cache));
  }
  bake_cache.blobs_dir = bake_path->blobs_dir;
  bake_cache.blob_sharing = std::make_unique<bake::BlobReadSharing>();
  return true;
}

class NodesModifierSimulationParams : public nodes::GeoNodesSimulationParams {
 private:
  static constexpr float max_delta_frames = 1.0f;

  const NodesModifierData &nmd_;
  const ModifierEvalContext &ctx_;
  const Main *bmain_;
  const Scene *scene_;
  SubFrame current_frame_;
  bool use_frame_cache_;
  bool depsgraph_is_active_;
  bake::ModifierCache *modifier_cache_;
  float fps_;
  bool has_invalid_simulation_ = false;

 public:
  struct DataPerZone {
    nodes::SimulationZoneBehavior behavior;
    NodesModifierBakeDataBlockMap data_block_map;
  };

  mutable Map<int, std::unique_ptr<DataPerZone>> data_by_zone_id;

  NodesModifierSimulationParams(NodesModifierData &nmd, const ModifierEvalContext &ctx)
      : nmd_(nmd), ctx_(ctx)
  {
    const Depsgraph *depsgraph = ctx_.depsgraph;
    bmain_ = DEG_get_bmain(depsgraph);
    current_frame_ = DEG_get_ctime(depsgraph);
    const Scene *scene = DEG_get_input_scene(depsgraph);
    scene_ = scene;
    use_frame_cache_ = ctx_.object->flag & OB_FLAG_USE_SIMULATION_CACHE;
    depsgraph_is_active_ = DEG_is_active(depsgraph);
    modifier_cache_ = nmd.runtime->cache.get();
    fps_ = scene->frames_per_second();

    if (!modifier_cache_) {
      return;
    }
    std::lock_guard lock{modifier_cache_->mutex};
    if (depsgraph_is_active_) {
      /* Invalidate data on user edits. */
      if (nmd.modifier.flag & eModifierFlag_UserModified) {
        for (std::unique_ptr<bake::SimulationNodeCache> &node_cache :
             modifier_cache_->simulation_cache_by_id.values())
        {
          if (node_cache->cache_status != bake::CacheStatus::Baked) {
            node_cache->cache_status = bake::CacheStatus::Invalid;
            if (!node_cache->bake.frames.is_empty()) {
              if (node_cache->bake.frames.last()->frame == current_frame_) {
                /* Remove the last (which is the current) cached frame so that it is simulated
                 * again. */
                node_cache->bake.frames.pop_last();
              }
            }
          }
        }
      }
      this->reset_invalid_node_bakes();
    }
    for (const std::unique_ptr<bake::SimulationNodeCache> &node_cache_ptr :
         modifier_cache_->simulation_cache_by_id.values())
    {
      const bake::SimulationNodeCache &node_cache = *node_cache_ptr;
      if (node_cache.cache_status == bake::CacheStatus::Invalid) {
        has_invalid_simulation_ = true;
        break;
      }
    }
  }

  void reset_invalid_node_bakes()
  {
    for (auto item : modifier_cache_->simulation_cache_by_id.items()) {
      const int id = item.key;
      bake::SimulationNodeCache &node_cache = *item.value;
      if (node_cache.cache_status != bake::CacheStatus::Invalid) {
        continue;
      }
      const std::optional<IndexRange> sim_frame_range = bake::get_node_bake_frame_range(
          *scene_, *ctx_.object, nmd_, id);
      if (!sim_frame_range.has_value()) {
        continue;
      }
      const SubFrame start_frame{int(sim_frame_range->start())};
      if (current_frame_ <= start_frame) {
        node_cache.reset();
      }
      if (!node_cache.bake.frames.is_empty() &&
          current_frame_ < node_cache.bake.frames.first()->frame)
      {
        node_cache.reset();
      }
    }
  }

  nodes::SimulationZoneBehavior *get(const int zone_id) const override
  {
    if (!modifier_cache_) {
      return nullptr;
    }
    std::lock_guard lock{modifier_cache_->mutex};
    return &this->data_by_zone_id
                .lookup_or_add_cb(zone_id,
                                  [&]() {
                                    auto data = std::make_unique<DataPerZone>();
                                    data->behavior.data_block_map = &data->data_block_map;
                                    this->init_simulation_info(
                                        zone_id, data->behavior, data->data_block_map);
                                    return data;
                                  })
                ->behavior;
  }

  void init_simulation_info(const int zone_id,
                            nodes::SimulationZoneBehavior &zone_behavior,
                            NodesModifierBakeDataBlockMap &data_block_map) const
  {
    bake::SimulationNodeCache &node_cache =
        *modifier_cache_->simulation_cache_by_id.lookup_or_add_cb(
            zone_id, []() { return std::make_unique<bake::SimulationNodeCache>(); });
    const NodesModifierBake &bake = *nmd_.find_bake(zone_id);
    const IndexRange sim_frame_range = *bake::get_node_bake_frame_range(
        *scene_, *ctx_.object, nmd_, zone_id);
    const SubFrame sim_start_frame{int(sim_frame_range.first())};
    const SubFrame sim_end_frame{int(sim_frame_range.last())};

    if (!modifier_cache_->requested_bakes.contains(zone_id)) {
      /* Try load baked data. */
      if (!node_cache.bake.failed_finding_bake) {
        if (node_cache.cache_status != bake::CacheStatus::Baked) {
          if (try_find_baked_data(bake, node_cache.bake, *bmain_, *ctx_.object, nmd_, zone_id)) {
            node_cache.cache_status = bake::CacheStatus::Baked;
          }
          else {
            node_cache.bake.failed_finding_bake = true;
          }
        }
      }
    }

    /* If there are no baked frames, we don't need keep track of the data-blocks. */
    if (!node_cache.bake.frames.is_empty() || node_cache.prev_cache.has_value()) {
      for (const NodesModifierDataBlock &data_block : Span{bake.data_blocks, bake.data_blocks_num})
      {
        data_block_map.old_mappings.add(data_block, data_block.id);
      }
    }

    const BakeFrameIndices frame_indices = get_bake_frame_indices(node_cache.bake.frames,
                                                                  current_frame_);
    if (node_cache.cache_status == bake::CacheStatus::Baked) {
      this->read_from_cache(frame_indices, node_cache, zone_behavior);
      return;
    }
    if (use_frame_cache_) {
      /* If the depsgraph is active, we allow creating new simulation states. Otherwise, the access
       * is read-only. */
      if (depsgraph_is_active_) {
        if (node_cache.bake.frames.is_empty()) {
          if (current_frame_ < sim_start_frame || current_frame_ > sim_end_frame) {
            /* Outside of simulation frame range, so ignore the simulation if there is no cache. */
            this->input_pass_through(zone_behavior);
            this->output_pass_through(zone_behavior);
            return;
          }
          /* Initialize the simulation. */
          if (current_frame_ > sim_start_frame || has_invalid_simulation_) {
            node_cache.cache_status = bake::CacheStatus::Invalid;
          }
          this->input_pass_through(zone_behavior);
          this->output_store_frame_cache(node_cache, zone_behavior);
          return;
        }
        if (frame_indices.prev && !frame_indices.current && !frame_indices.next &&
            current_frame_ <= sim_end_frame)
        {
          /* Read the previous frame's data and store the newly computed simulation state. */
          auto &output_copy_info = zone_behavior.input.emplace<sim_input::OutputCopy>();
          const bake::FrameCache &prev_frame_cache = *node_cache.bake.frames[*frame_indices.prev];
          const float real_delta_frames = float(current_frame_) - float(prev_frame_cache.frame);
          if (real_delta_frames != 1) {
            node_cache.cache_status = bake::CacheStatus::Invalid;
          }
          const float delta_frames = std::min(max_delta_frames, real_delta_frames);
          output_copy_info.delta_time = delta_frames / fps_;
          output_copy_info.state = prev_frame_cache.state;
          this->output_store_frame_cache(node_cache, zone_behavior);
          return;
        }
      }
      this->read_from_cache(frame_indices, node_cache, zone_behavior);
      return;
    }

    /* When there is no per-frame cache, check if there is a previous state. */
    if (node_cache.prev_cache) {
      if (node_cache.prev_cache->frame < current_frame_) {
        /* Do a simulation step. */
        const float delta_frames = std::min(
            max_delta_frames, float(current_frame_) - float(node_cache.prev_cache->frame));
        auto &output_move_info = zone_behavior.input.emplace<sim_input::OutputMove>();
        output_move_info.delta_time = delta_frames / fps_;
        output_move_info.state = std::move(node_cache.prev_cache->state);
        this->store_as_prev_items(node_cache, zone_behavior);
        return;
      }
      if (node_cache.prev_cache->frame == current_frame_) {
        /* Just read from the previous state if the frame has not changed. */
        auto &output_copy_info = zone_behavior.input.emplace<sim_input::OutputCopy>();
        output_copy_info.delta_time = 0.0f;
        output_copy_info.state = node_cache.prev_cache->state;
        auto &read_single_info = zone_behavior.output.emplace<sim_output::ReadSingle>();
        read_single_info.state = node_cache.prev_cache->state;
        return;
      }
      if (!depsgraph_is_active_) {
        /* There is no previous state, and it's not possible to initialize the simulation because
         * the depsgraph is not active. */
        zone_behavior.input.emplace<sim_input::PassThrough>();
        zone_behavior.output.emplace<sim_output::PassThrough>();
        return;
      }
      /* Reset the simulation when the scene time moved backwards. */
      node_cache.prev_cache.reset();
    }
    zone_behavior.input.emplace<sim_input::PassThrough>();
    if (depsgraph_is_active_) {
      /* Initialize the simulation. */
      this->store_as_prev_items(node_cache, zone_behavior);
    }
    else {
      zone_behavior.output.emplace<sim_output::PassThrough>();
    }
  }

  void input_pass_through(nodes::SimulationZoneBehavior &zone_behavior) const
  {
    zone_behavior.input.emplace<sim_input::PassThrough>();
  }

  void output_pass_through(nodes::SimulationZoneBehavior &zone_behavior) const
  {
    zone_behavior.output.emplace<sim_output::PassThrough>();
  }

  void output_store_frame_cache(bake::SimulationNodeCache &node_cache,
                                nodes::SimulationZoneBehavior &zone_behavior) const
  {
    auto &store_new_state_info = zone_behavior.output.emplace<sim_output::StoreNewState>();
    store_new_state_info.store_fn = [simulation_cache = modifier_cache_,
                                     node_cache = &node_cache,
                                     current_frame = current_frame_](bke::bake::BakeState state) {
      std::lock_guard lock{simulation_cache->mutex};
      auto frame_cache = std::make_unique<bake::FrameCache>();
      frame_cache->frame = current_frame;
      frame_cache->state = std::move(state);
      node_cache->bake.frames.append(std::move(frame_cache));
    };
  }

  void store_as_prev_items(bake::SimulationNodeCache &node_cache,
                           nodes::SimulationZoneBehavior &zone_behavior) const
  {
    auto &store_new_state_info = zone_behavior.output.emplace<sim_output::StoreNewState>();
    store_new_state_info.store_fn = [simulation_cache = modifier_cache_,
                                     node_cache = &node_cache,
                                     current_frame = current_frame_](bke::bake::BakeState state) {
      std::lock_guard lock{simulation_cache->mutex};
      if (!node_cache->prev_cache) {
        node_cache->prev_cache.emplace();
      }
      node_cache->prev_cache->state = std::move(state);
      node_cache->prev_cache->frame = current_frame;
    };
  }

  void read_from_cache(const BakeFrameIndices &frame_indices,
                       bake::SimulationNodeCache &node_cache,
                       nodes::SimulationZoneBehavior &zone_behavior) const
  {
    if (frame_indices.prev) {
      auto &output_copy_info = zone_behavior.input.emplace<sim_input::OutputCopy>();
      bake::FrameCache &frame_cache = *node_cache.bake.frames[*frame_indices.prev];
      const float delta_frames = std::min(max_delta_frames,
                                          float(current_frame_) - float(frame_cache.frame));
      output_copy_info.delta_time = delta_frames / fps_;
      output_copy_info.state = frame_cache.state;
    }
    else {
      zone_behavior.input.emplace<sim_input::PassThrough>();
    }
    if (frame_indices.current) {
      this->read_single(*frame_indices.current, node_cache, zone_behavior);
    }
    else if (frame_indices.next) {
      if (frame_indices.prev) {
        this->read_interpolated(
            *frame_indices.prev, *frame_indices.next, node_cache, zone_behavior);
      }
      else {
        this->output_pass_through(zone_behavior);
      }
    }
    else if (frame_indices.prev) {
      this->read_single(*frame_indices.prev, node_cache, zone_behavior);
    }
    else {
      this->output_pass_through(zone_behavior);
    }
  }

  void read_single(const int frame_index,
                   bake::SimulationNodeCache &node_cache,
                   nodes::SimulationZoneBehavior &zone_behavior) const
  {
    bake::FrameCache &frame_cache = *node_cache.bake.frames[frame_index];
    ensure_bake_loaded(node_cache.bake, frame_cache);
    auto &read_single_info = zone_behavior.output.emplace<sim_output::ReadSingle>();
    read_single_info.state = frame_cache.state;
  }

  void read_interpolated(const int prev_frame_index,
                         const int next_frame_index,
                         bake::SimulationNodeCache &node_cache,
                         nodes::SimulationZoneBehavior &zone_behavior) const
  {
    bake::FrameCache &prev_frame_cache = *node_cache.bake.frames[prev_frame_index];
    bake::FrameCache &next_frame_cache = *node_cache.bake.frames[next_frame_index];
    ensure_bake_loaded(node_cache.bake, prev_frame_cache);
    ensure_bake_loaded(node_cache.bake, next_frame_cache);
    auto &read_interpolated_info = zone_behavior.output.emplace<sim_output::ReadInterpolated>();
    read_interpolated_info.mix_factor = (float(current_frame_) - float(prev_frame_cache.frame)) /
                                        (float(next_frame_cache.frame) -
                                         float(prev_frame_cache.frame));
    read_interpolated_info.prev_state = prev_frame_cache.state;
    read_interpolated_info.next_state = next_frame_cache.state;
  }
};

class NodesModifierBakeParams : public nodes::GeoNodesBakeParams {
 private:
  const NodesModifierData &nmd_;
  const ModifierEvalContext &ctx_;
  Main *bmain_;
  SubFrame current_frame_;
  bake::ModifierCache *modifier_cache_;
  bool depsgraph_is_active_;

 public:
  struct DataPerNode {
    nodes::BakeNodeBehavior behavior;
    NodesModifierBakeDataBlockMap data_block_map;
  };

  mutable Map<int, std::unique_ptr<DataPerNode>> data_by_node_id;

  NodesModifierBakeParams(NodesModifierData &nmd, const ModifierEvalContext &ctx)
      : nmd_(nmd), ctx_(ctx)
  {
    const Depsgraph *depsgraph = ctx_.depsgraph;
    current_frame_ = DEG_get_ctime(depsgraph);
    modifier_cache_ = nmd.runtime->cache.get();
    depsgraph_is_active_ = DEG_is_active(depsgraph);
    bmain_ = DEG_get_bmain(depsgraph);
  }

  nodes::BakeNodeBehavior *get(const int id) const override
  {
    if (!modifier_cache_) {
      return nullptr;
    }
    std::lock_guard lock{modifier_cache_->mutex};
    return &this->data_by_node_id
                .lookup_or_add_cb(id,
                                  [&]() {
                                    auto data = std::make_unique<DataPerNode>();
                                    data->behavior.data_block_map = &data->data_block_map;
                                    this->init_bake_behavior(
                                        id, data->behavior, data->data_block_map);
                                    return data;
                                  })
                ->behavior;
    return nullptr;
  }

 private:
  void init_bake_behavior(const int id,
                          nodes::BakeNodeBehavior &behavior,
                          NodesModifierBakeDataBlockMap &data_block_map) const
  {
    bake::BakeNodeCache &node_cache = *modifier_cache_->bake_cache_by_id.lookup_or_add_cb(
        id, []() { return std::make_unique<bake::BakeNodeCache>(); });
    const NodesModifierBake &bake = *nmd_.find_bake(id);

    for (const NodesModifierDataBlock &data_block : Span{bake.data_blocks, bake.data_blocks_num}) {
      data_block_map.old_mappings.add(data_block, data_block.id);
    }

    if (depsgraph_is_active_) {
      if (modifier_cache_->requested_bakes.contains(id)) {
        /* This node is baked during the current evaluation. */
        auto &store_info = behavior.behavior.emplace<sim_output::StoreNewState>();
        store_info.store_fn = [modifier_cache = modifier_cache_,
                               node_cache = &node_cache,
                               current_frame = current_frame_](bake::BakeState state) {
          std::lock_guard lock{modifier_cache->mutex};
          auto frame_cache = std::make_unique<bake::FrameCache>();
          frame_cache->frame = current_frame;
          frame_cache->state = std::move(state);
          auto &frames = node_cache->bake.frames;
          const int insert_index = binary_search::first_if(
              frames, [&](const std::unique_ptr<bake::FrameCache> &frame_cache) {
                return frame_cache->frame > current_frame;
              });
          frames.insert(insert_index, std::move(frame_cache));
        };
        return;
      }
    }

    /* Try load baked data. */
    if (node_cache.bake.frames.is_empty()) {
      if (!node_cache.bake.failed_finding_bake) {
        if (!try_find_baked_data(bake, node_cache.bake, *bmain_, *ctx_.object, nmd_, id)) {
          node_cache.bake.failed_finding_bake = true;
        }
      }
    }

    if (node_cache.bake.frames.is_empty()) {
      behavior.behavior.emplace<sim_output::PassThrough>();
      return;
    }
    const BakeFrameIndices frame_indices = get_bake_frame_indices(node_cache.bake.frames,
                                                                  current_frame_);
    if (frame_indices.current) {
      this->read_single(*frame_indices.current, node_cache, behavior);
      return;
    }
    if (frame_indices.prev && frame_indices.next) {
      this->read_interpolated(*frame_indices.prev, *frame_indices.next, node_cache, behavior);
      return;
    }
    if (frame_indices.prev) {
      this->read_single(*frame_indices.prev, node_cache, behavior);
      return;
    }
    if (frame_indices.next) {
      this->read_single(*frame_indices.next, node_cache, behavior);
      return;
    }
    BLI_assert_unreachable();
  }

  void read_single(const int frame_index,
                   bake::BakeNodeCache &node_cache,
                   nodes::BakeNodeBehavior &behavior) const
  {
    bake::FrameCache &frame_cache = *node_cache.bake.frames[frame_index];
    ensure_bake_loaded(node_cache.bake, frame_cache);
    if (this->check_read_error(frame_cache, behavior)) {
      return;
    }
    auto &read_single_info = behavior.behavior.emplace<sim_output::ReadSingle>();
    read_single_info.state = frame_cache.state;
  }

  void read_interpolated(const int prev_frame_index,
                         const int next_frame_index,
                         bake::BakeNodeCache &node_cache,
                         nodes::BakeNodeBehavior &behavior) const
  {
    bake::FrameCache &prev_frame_cache = *node_cache.bake.frames[prev_frame_index];
    bake::FrameCache &next_frame_cache = *node_cache.bake.frames[next_frame_index];
    ensure_bake_loaded(node_cache.bake, prev_frame_cache);
    ensure_bake_loaded(node_cache.bake, next_frame_cache);
    if (this->check_read_error(prev_frame_cache, behavior) ||
        this->check_read_error(next_frame_cache, behavior))
    {
      return;
    }
    auto &read_interpolated_info = behavior.behavior.emplace<sim_output::ReadInterpolated>();
    read_interpolated_info.mix_factor = (float(current_frame_) - float(prev_frame_cache.frame)) /
                                        (float(next_frame_cache.frame) -
                                         float(prev_frame_cache.frame));
    read_interpolated_info.prev_state = prev_frame_cache.state;
    read_interpolated_info.next_state = next_frame_cache.state;
  }

  [[nodiscard]] bool check_read_error(const bake::FrameCache &frame_cache,
                                      nodes::BakeNodeBehavior &behavior) const
  {
    if (frame_cache.meta_data_source && frame_cache.state.items_by_id.is_empty()) {
      auto &read_error_info = behavior.behavior.emplace<sim_output::ReadError>();
      read_error_info.message = RPT_("Cannot load the baked data");
      return true;
    }
    return false;
  }
};

static void add_missing_data_block_mappings(
    NodesModifierBake &bake,
    const Span<bake::BakeDataBlockID> missing,
    FunctionRef<ID *(const bake::BakeDataBlockID &)> get_data_block)
{
  const int old_num = bake.data_blocks_num;
  const int new_num = old_num + missing.size();
  bake.data_blocks = reinterpret_cast<NodesModifierDataBlock *>(
      MEM_recallocN(bake.data_blocks, sizeof(NodesModifierDataBlock) * new_num));
  for (const int i : missing.index_range()) {
    NodesModifierDataBlock &data_block = bake.data_blocks[old_num + i];
    const blender::bke::bake::BakeDataBlockID &key = missing[i];

    data_block.id_name = BLI_strdup(key.id_name.c_str());
    if (!key.lib_name.empty()) {
      data_block.lib_name = BLI_strdup(key.lib_name.c_str());
    }
    data_block.id_type = int(key.type);
    ID *id = get_data_block(key);
    if (id) {
      data_block.id = id;
    }
  }
  bake.data_blocks_num = new_num;
}

void nodes_modifier_data_block_destruct(NodesModifierDataBlock *data_block, const bool do_id_user)
{
  MEM_SAFE_FREE(data_block->id_name);
  MEM_SAFE_FREE(data_block->lib_name);
  if (do_id_user) {
    id_us_min(data_block->id);
  }
}

/**
 * During evaluation we might have baked geometry that contains references to other data-blocks
 * (such as materials). We need to make sure that those data-blocks stay dependencies of the
 * modifier. Otherwise, the data-block references might not work when the baked data is loaded
 * again. Therefor, the dependencies are written back to the original modifier.
 */
static void add_data_block_items_writeback(const ModifierEvalContext &ctx,
                                           NodesModifierData &nmd_eval,
                                           NodesModifierData &nmd_orig,
                                           NodesModifierSimulationParams &simulation_params,
                                           NodesModifierBakeParams &bake_params)
{
  Depsgraph *depsgraph = ctx.depsgraph;
  Main *bmain = DEG_get_bmain(depsgraph);

  struct DataPerBake {
    bool reset_first = false;
    Map<bake::BakeDataBlockID, ID *> new_mappings;
  };
  Map<int, DataPerBake> writeback_data;
  for (auto item : simulation_params.data_by_zone_id.items()) {
    DataPerBake data;
    NodesModifierBake &bake = *nmd_eval.find_bake(item.key);
    if (item.value->data_block_map.old_mappings.size() < bake.data_blocks_num) {
      data.reset_first = true;
    }
    if (bake::SimulationNodeCache *node_cache = nmd_eval.runtime->cache->get_simulation_node_cache(
            item.key))
    {
      /* Only writeback if the bake node has actually baked anything. */
      if (!node_cache->bake.frames.is_empty() || node_cache->prev_cache.has_value()) {
        data.new_mappings = std::move(item.value->data_block_map.new_mappings);
      }
    }
    if (data.reset_first || !data.new_mappings.is_empty()) {
      writeback_data.add(item.key, std::move(data));
    }
  }
  for (auto item : bake_params.data_by_node_id.items()) {
    if (bake::BakeNodeCache *node_cache = nmd_eval.runtime->cache->get_bake_node_cache(item.key)) {
      /* Only writeback if the bake node has actually baked anything. */
      if (!node_cache->bake.frames.is_empty()) {
        DataPerBake data;
        data.new_mappings = std::move(item.value->data_block_map.new_mappings);
        writeback_data.add(item.key, std::move(data));
      }
    }
  }

  if (writeback_data.is_empty()) {
    /* Nothing to do. */
    return;
  }

  deg::sync_writeback::add(
      *depsgraph,
      [object_eval = ctx.object,
       bmain,
       &nmd_orig,
       &nmd_eval,
       writeback_data = std::move(writeback_data)]() {
        for (auto item : writeback_data.items()) {
          const int bake_id = item.key;
          DataPerBake data = item.value;

          NodesModifierBake &bake_orig = *nmd_orig.find_bake(bake_id);
          NodesModifierBake &bake_eval = *nmd_eval.find_bake(bake_id);

          if (data.reset_first) {
            /* Reset data-block list on original data. */
            dna::array::clear<NodesModifierDataBlock>(&bake_orig.data_blocks,
                                                      &bake_orig.data_blocks_num,
                                                      &bake_orig.active_data_block,
                                                      [](NodesModifierDataBlock *data_block) {
                                                        nodes_modifier_data_block_destruct(
                                                            data_block, true);
                                                      });
            /* Reset data-block list on evaluated data. */
            dna::array::clear<NodesModifierDataBlock>(&bake_eval.data_blocks,
                                                      &bake_eval.data_blocks_num,
                                                      &bake_eval.active_data_block,
                                                      [](NodesModifierDataBlock *data_block) {
                                                        nodes_modifier_data_block_destruct(
                                                            data_block, false);
                                                      });
          }

          Vector<bake::BakeDataBlockID> sorted_new_mappings;
          sorted_new_mappings.extend(data.new_mappings.keys().begin(),
                                     data.new_mappings.keys().end());
          bool needs_reevaluation = false;
          /* Add new data block mappings to the original modifier. This may do a name lookup in
           * bmain to find the data block if there is not faster way to get it. */
          add_missing_data_block_mappings(
              bake_orig, sorted_new_mappings, [&](const bake::BakeDataBlockID &key) -> ID * {
                ID *id_orig = nullptr;
                if (ID *id_eval = data.new_mappings.lookup_default(key, nullptr)) {
                  id_orig = DEG_get_original(id_eval);
                }
                else {
                  needs_reevaluation = true;
                  id_orig = BKE_libblock_find_name_and_library(
                      bmain, short(key.type), key.id_name.c_str(), key.lib_name.c_str());
                }
                if (id_orig) {
                  id_us_plus(id_orig);
                }
                return id_orig;
              });
          /* Add new data block mappings to the evaluated modifier. In most cases this makes it so
           * the evaluated modifier is in the same state as if it were copied from the updated
           * original again. The exception is when a missing data block was found that is not in
           * the depsgraph currently. */
          add_missing_data_block_mappings(
              bake_eval, sorted_new_mappings, [&](const bake::BakeDataBlockID &key) -> ID * {
                return data.new_mappings.lookup_default(key, nullptr);
              });

          if (needs_reevaluation) {
            Object *object_orig = DEG_get_original(object_eval);
            DEG_id_tag_update(&object_orig->id, ID_RECALC_GEOMETRY);
            DEG_relations_tag_update(bmain);
          }
        }
      });
}

static void modifyGeometry(ModifierData *md,
                           const ModifierEvalContext *ctx,
                           bke::GeometrySet &geometry_set)
{
  using namespace blender;
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return;
  }
  NodesModifierData *nmd_orig = reinterpret_cast<NodesModifierData *>(
      BKE_modifier_get_original(ctx->object, &nmd->modifier));
  if (ID_MISSING(nmd_orig->node_group)) {
    return;
  }

  const bNodeTree &tree = *nmd->node_group;
  check_property_socket_sync(ctx->object, nmd->settings.properties, md);

  tree.ensure_topology_cache();
  const bNode *output_node = tree.group_output_node();
  if (output_node == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Node group must have a group output node");
    geometry_set.clear();
    return;
  }

  Span<const bNodeSocket *> group_outputs = output_node->input_sockets().drop_back(1);
  if (group_outputs.is_empty()) {
    BKE_modifier_set_error(ctx->object, md, "Node group must have an output socket");
    geometry_set.clear();
    return;
  }

  const bNodeSocket *first_output_socket = group_outputs[0];
  if (!STREQ(first_output_socket->idname, "NodeSocketGeometry")) {
    BKE_modifier_set_error(ctx->object, md, "Node group's first output must be a geometry");
    geometry_set.clear();
    return;
  }

  const nodes::GeometryNodesLazyFunctionGraphInfo *lf_graph_info =
      nodes::ensure_geometry_nodes_lazy_function_graph(tree);
  if (lf_graph_info == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Cannot evaluate node group");
    geometry_set.clear();
    return;
  }

  bool use_orig_index_verts = false;
  bool use_orig_index_edges = false;
  bool use_orig_index_faces = false;
  if (const Mesh *mesh = geometry_set.get_mesh()) {
    use_orig_index_verts = CustomData_has_layer(&mesh->vert_data, CD_ORIGINDEX);
    use_orig_index_edges = CustomData_has_layer(&mesh->edge_data, CD_ORIGINDEX);
    use_orig_index_faces = CustomData_has_layer(&mesh->face_data, CD_ORIGINDEX);
  }

  nodes::GeoNodesCallData call_data;

  nodes::GeoNodesModifierData modifier_eval_data{};
  modifier_eval_data.depsgraph = ctx->depsgraph;
  modifier_eval_data.self_object = ctx->object;
  auto eval_log = std::make_unique<geo_log::GeoNodesLog>();
  call_data.modifier_data = &modifier_eval_data;

  NodesModifierSimulationParams simulation_params(*nmd, *ctx);
  call_data.simulation_params = &simulation_params;
  NodesModifierBakeParams bake_params{*nmd, *ctx};
  call_data.bake_params = &bake_params;

  Set<ComputeContextHash> socket_log_contexts;
  if (logging_enabled(ctx)) {
    call_data.eval_log = eval_log.get();

    find_socket_log_contexts(*nmd, *ctx, socket_log_contexts);
    call_data.socket_log_contexts = &socket_log_contexts;
  }

  nodes::GeoNodesSideEffectNodes side_effect_nodes;
  find_side_effect_nodes(*nmd, *ctx, side_effect_nodes, socket_log_contexts);
  call_data.side_effect_nodes = &side_effect_nodes;

  bke::ModifierComputeContext modifier_compute_context{nullptr, *nmd};

  geometry_set = nodes::execute_geometry_nodes_on_geometry(tree,
                                                           nmd->settings.properties,
                                                           modifier_compute_context,
                                                           call_data,
                                                           std::move(geometry_set));

  if (logging_enabled(ctx)) {
    nmd_orig->runtime->eval_log = std::move(eval_log);
  }

  if (DEG_is_active(ctx->depsgraph) && !(ctx->flag & MOD_APPLY_TO_ORIGINAL)) {
    add_data_block_items_writeback(*ctx, *nmd, *nmd_orig, simulation_params, bake_params);
  }

  if (use_orig_index_verts || use_orig_index_edges || use_orig_index_faces) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      /* Add #CD_ORIGINDEX layers if they don't exist already. This is required because the
       * #eModifierTypeFlag_SupportsMapping flag is set. If the layers did not exist before, it is
       * assumed that the output mesh does not have a mapping to the original mesh. */
      if (use_orig_index_verts) {
        CustomData_add_layer(&mesh->vert_data, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->verts_num);
      }
      if (use_orig_index_edges) {
        CustomData_add_layer(&mesh->edge_data, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->edges_num);
      }
      if (use_orig_index_faces) {
        CustomData_add_layer(&mesh->face_data, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->faces_num);
      }
    }
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  bke::GeometrySet geometry_set = bke::GeometrySet::from_mesh(
      mesh, bke::GeometryOwnershipType::Editable);

  modifyGeometry(md, ctx, geometry_set);

  bke::MeshComponent &mesh_component = geometry_set.get_component_for_write<bke::MeshComponent>();
  if (mesh_component.get() != mesh) {
    /* If this is the same as the input mesh, it's not necessary to make a copy of it even if it's
     * not owned by the geometry set. That's because we know that the caller manages the ownership
     * of the mesh. */
    mesh_component.ensure_owns_direct_data();
  }
  Mesh *new_mesh = mesh_component.release();
  if (new_mesh == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0);
  }
  return new_mesh;
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  modifyGeometry(md, ctx, *geometry_set);
}

void NodesModifierUsageInferenceCache::ensure(const NodesModifierData &nmd)
{
  if (!nmd.node_group) {
    this->reset();
    return;
  }
  if (ID_MISSING(&nmd.node_group->id)) {
    this->reset();
    return;
  }
  const bNodeTree &tree = *nmd.node_group;
  tree.ensure_interface_cache();
  tree.ensure_topology_cache();
  ResourceScope scope;
  const Vector<nodes::InferenceValue> group_input_values =
      nodes::get_geometry_nodes_input_inference_values(tree, nmd.settings.properties, scope);

  /* Compute the hash of the input values. This has to be done everytime currently, because there
   * is no reliable callback yet that is called any of the modifier properties changes. */
  XXH3_state_t *state = XXH3_createState();
  XXH3_64bits_reset(state);
  BLI_SCOPED_DEFER([&]() { XXH3_freeState(state); });
  for (const int input_i : IndexRange(nmd.node_group->interface_inputs().size())) {
    const nodes::InferenceValue &value = group_input_values[input_i];
    XXH3_64bits_update(state, &input_i, sizeof(input_i));
    if (value.is_primitive_value()) {
      const void *value_ptr = value.get_primitive_ptr();
      const bNodeTreeInterfaceSocket &io_socket = *nmd.node_group->interface_inputs()[input_i];
      const CPPType &base_type = *io_socket.socket_typeinfo()->base_cpp_type;
      uint64_t value_hash = base_type.hash_or_fallback(value_ptr, 0);
      XXH3_64bits_update(state, &value_hash, sizeof(value_hash));
    }
  }
  const uint64_t new_input_values_hash = XXH3_64bits_digest(state);
  if (new_input_values_hash == input_values_hash_) {
    if (this->inputs.size() == tree.interface_inputs().size() &&
        this->outputs.size() == tree.interface_outputs().size())
    {
      /* The cache is up to date, so return early. */
      return;
    }
  }
  /* Compute the new usage inference result. */
  this->inputs.reinitialize(tree.interface_inputs().size());
  this->outputs.reinitialize(tree.interface_outputs().size());
  nodes::socket_usage_inference::infer_group_interface_usage(
      tree, group_input_values, inputs, outputs);
  input_values_hash_ = new_input_values_hash;
}

void NodesModifierUsageInferenceCache::reset()
{
  input_values_hash_ = 0;
  this->inputs = {};
  this->outputs = {};
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *modifier_ptr = modifier_panel_get_property_pointers(panel, nullptr);
  nodes::draw_geometry_nodes_modifier_ui(*C, modifier_ptr, *layout);
}

static void panel_register(ARegionType *region_type)
{
  using namespace blender;
  modifier_panel_register(region_type, eModifierType_Nodes, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);

  BLO_write_struct(writer, NodesModifierData, nmd);

  BLO_write_string(writer, nmd->bake_directory);

  Map<IDProperty *, IDPropertyUIDataBool *> boolean_props;
  if (nmd->settings.properties != nullptr) {
    if (!BLO_write_is_undo(writer)) {
      /* Boolean properties are added automatically for boolean node group inputs. Integer
       * properties are automatically converted to boolean sockets where applicable as well.
       * However, boolean properties will crash old versions of Blender, so convert them to integer
       * properties for writing. The actual value is stored in the same variable for both types */
      LISTBASE_FOREACH (IDProperty *, prop, &nmd->settings.properties->data.group) {
        if (prop->type == IDP_BOOLEAN) {
          boolean_props.add_new(prop, reinterpret_cast<IDPropertyUIDataBool *>(prop->ui_data));
          prop->type = IDP_INT;
          prop->ui_data = nullptr;
        }
      }
    }

    /* Note that the property settings are based on the socket type info
     * and don't necessarily need to be written, but we can't just free them. */
    IDP_BlendWrite(writer, nmd->settings.properties);
  }

  BLO_write_struct_array(writer, NodesModifierBake, nmd->bakes_num, nmd->bakes);
  for (const NodesModifierBake &bake : Span(nmd->bakes, nmd->bakes_num)) {
    BLO_write_string(writer, bake.directory);

    BLO_write_struct_array(writer, NodesModifierDataBlock, bake.data_blocks_num, bake.data_blocks);
    for (const NodesModifierDataBlock &item : Span(bake.data_blocks, bake.data_blocks_num)) {
      BLO_write_string(writer, item.id_name);
      BLO_write_string(writer, item.lib_name);
    }
    if (bake.packed) {
      BLO_write_struct(writer, NodesModifierPackedBake, bake.packed);
      BLO_write_struct_array(
          writer, NodesModifierBakeFile, bake.packed->meta_files_num, bake.packed->meta_files);
      BLO_write_struct_array(
          writer, NodesModifierBakeFile, bake.packed->blob_files_num, bake.packed->blob_files);
      const auto write_bake_file = [&](const NodesModifierBakeFile &bake_file) {
        BLO_write_string(writer, bake_file.name);
        if (bake_file.packed_file) {
          BKE_packedfile_blend_write(writer, bake_file.packed_file);
        }
      };
      for (const NodesModifierBakeFile &meta_file :
           Span{bake.packed->meta_files, bake.packed->meta_files_num})
      {
        write_bake_file(meta_file);
      }
      for (const NodesModifierBakeFile &blob_file :
           Span{bake.packed->blob_files, bake.packed->blob_files_num})
      {
        write_bake_file(blob_file);
      }
    }
  }
  BLO_write_struct_array(writer, NodesModifierPanel, nmd->panels_num, nmd->panels);

  if (nmd->settings.properties) {
    if (!BLO_write_is_undo(writer)) {
      LISTBASE_FOREACH (IDProperty *, prop, &nmd->settings.properties->data.group) {
        if (prop->type == IDP_INT) {
          if (IDPropertyUIDataBool **ui_data = boolean_props.lookup_ptr(prop)) {
            prop->type = IDP_BOOLEAN;
            if (ui_data) {
              prop->ui_data = reinterpret_cast<IDPropertyUIData *>(*ui_data);
            }
          }
        }
      }
    }
  }
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  BLO_read_string(reader, &nmd->bake_directory);
  if (nmd->node_group == nullptr) {
    nmd->settings.properties = nullptr;
  }
  else {
    BLO_read_struct(reader, IDProperty, &nmd->settings.properties);
    IDP_BlendDataRead(reader, &nmd->settings.properties);
  }

  BLO_read_struct_array(reader, NodesModifierBake, nmd->bakes_num, &nmd->bakes);

  if (nmd->bakes_num > 0 && nmd->bakes == nullptr) {
    /* This case generally shouldn't be allowed to happen. However, there is a bug report with a
     * corrupted .blend file (#123974) that triggers this case. Unfortunately, it's not clear how
     * that could have happened. For now, handle this case more gracefully in release builds, while
     * still crashing in debug builds. */
    nmd->bakes_num = 0;
    BLI_assert_unreachable();
  }

  for (NodesModifierBake &bake : MutableSpan(nmd->bakes, nmd->bakes_num)) {
    BLO_read_string(reader, &bake.directory);

    BLO_read_struct_array(reader, NodesModifierDataBlock, bake.data_blocks_num, &bake.data_blocks);
    for (NodesModifierDataBlock &data_block : MutableSpan(bake.data_blocks, bake.data_blocks_num))
    {
      BLO_read_string(reader, &data_block.id_name);
      BLO_read_string(reader, &data_block.lib_name);
    }

    BLO_read_struct(reader, NodesModifierPackedBake, &bake.packed);
    if (bake.packed) {
      BLO_read_struct_array(
          reader, NodesModifierBakeFile, bake.packed->meta_files_num, &bake.packed->meta_files);
      BLO_read_struct_array(
          reader, NodesModifierBakeFile, bake.packed->blob_files_num, &bake.packed->blob_files);
      const auto read_bake_file = [&](NodesModifierBakeFile &bake_file) {
        BLO_read_string(reader, &bake_file.name);
        if (bake_file.packed_file) {
          BKE_packedfile_blend_read(reader, &bake_file.packed_file, "");
        }
      };
      for (NodesModifierBakeFile &meta_file :
           MutableSpan{bake.packed->meta_files, bake.packed->meta_files_num})
      {
        read_bake_file(meta_file);
      }
      for (NodesModifierBakeFile &blob_file :
           MutableSpan{bake.packed->blob_files, bake.packed->blob_files_num})
      {
        read_bake_file(blob_file);
      }
    }
  }
  BLO_read_struct_array(reader, NodesModifierPanel, nmd->panels_num, &nmd->panels);

  nmd->runtime = MEM_new<NodesModifierRuntime>(__func__);
  nmd->runtime->cache = std::make_shared<bake::ModifierCache>();
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  NodesModifierData *tnmd = reinterpret_cast<NodesModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);

  if (nmd->bakes) {
    tnmd->bakes = static_cast<NodesModifierBake *>(MEM_dupallocN(nmd->bakes));
    for (const int i : IndexRange(nmd->bakes_num)) {
      NodesModifierBake &bake = tnmd->bakes[i];
      if (bake.directory) {
        bake.directory = BLI_strdup(bake.directory);
      }
      if (bake.data_blocks) {
        bake.data_blocks = static_cast<NodesModifierDataBlock *>(MEM_dupallocN(bake.data_blocks));
        for (const int i : IndexRange(bake.data_blocks_num)) {
          NodesModifierDataBlock &data_block = bake.data_blocks[i];
          if (data_block.id_name) {
            data_block.id_name = BLI_strdup(data_block.id_name);
          }
          if (data_block.lib_name) {
            data_block.lib_name = BLI_strdup(data_block.lib_name);
          }
        }
      }
      if (bake.packed) {
        bake.packed = static_cast<NodesModifierPackedBake *>(MEM_dupallocN(bake.packed));
        const auto copy_bake_files_inplace = [](NodesModifierBakeFile **bake_files,
                                                const int bake_files_num) {
          if (!*bake_files) {
            return;
          }
          *bake_files = static_cast<NodesModifierBakeFile *>(MEM_dupallocN(*bake_files));
          for (NodesModifierBakeFile &bake_file : MutableSpan{*bake_files, bake_files_num}) {
            bake_file.name = BLI_strdup_null(bake_file.name);
            if (bake_file.packed_file) {
              bake_file.packed_file = BKE_packedfile_duplicate(bake_file.packed_file);
            }
          }
        };
        copy_bake_files_inplace(&bake.packed->meta_files, bake.packed->meta_files_num);
        copy_bake_files_inplace(&bake.packed->blob_files, bake.packed->blob_files_num);
      }
    }
  }

  if (nmd->panels) {
    tnmd->panels = static_cast<NodesModifierPanel *>(MEM_dupallocN(nmd->panels));
  }

  tnmd->runtime = MEM_new<NodesModifierRuntime>(__func__);

  if (flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) {
    /* Share the simulation cache between the original and evaluated modifier. */
    tnmd->runtime->cache = nmd->runtime->cache;
    /* Keep bake path in the evaluated modifier. */
    tnmd->bake_directory = nmd->bake_directory ? BLI_strdup(nmd->bake_directory) : nullptr;
  }
  else {
    tnmd->runtime->cache = std::make_shared<bake::ModifierCache>();
    /* Clear the bake path when duplicating. */
    tnmd->bake_directory = nullptr;
  }

  if (nmd->settings.properties != nullptr) {
    tnmd->settings.properties = IDP_CopyProperty_ex(nmd->settings.properties, flag);
  }
}

void nodes_modifier_packed_bake_free(NodesModifierPackedBake *packed_bake)
{
  const auto free_packed_files = [](NodesModifierBakeFile *files, const int files_num) {
    for (NodesModifierBakeFile &file : MutableSpan{files, files_num}) {
      MEM_SAFE_FREE(file.name);
      if (file.packed_file) {
        BKE_packedfile_free(file.packed_file);
      }
    }
    MEM_SAFE_FREE(files);
  };
  free_packed_files(packed_bake->meta_files, packed_bake->meta_files_num);
  free_packed_files(packed_bake->blob_files, packed_bake->blob_files_num);
  MEM_SAFE_FREE(packed_bake);
}

void nodes_modifier_bake_destruct(NodesModifierBake *bake, const bool do_id_user)
{
  MEM_SAFE_FREE(bake->directory);

  for (NodesModifierDataBlock &data_block : MutableSpan(bake->data_blocks, bake->data_blocks_num))
  {
    nodes_modifier_data_block_destruct(&data_block, do_id_user);
  }
  MEM_SAFE_FREE(bake->data_blocks);

  if (bake->packed) {
    nodes_modifier_packed_bake_free(bake->packed);
  }
}

static void free_data(ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->settings.properties != nullptr) {
    IDP_FreeProperty_ex(nmd->settings.properties, false);
    nmd->settings.properties = nullptr;
  }

  for (NodesModifierBake &bake : MutableSpan(nmd->bakes, nmd->bakes_num)) {
    nodes_modifier_bake_destruct(&bake, false);
  }
  MEM_SAFE_FREE(nmd->bakes);

  MEM_SAFE_FREE(nmd->panels);

  MEM_SAFE_FREE(nmd->bake_directory);
  MEM_delete(nmd->runtime);
}

static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  /* We don't know what the node tree will need. If there are vertex groups, it is likely that the
   * node tree wants to access them. */
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  r_cddata_masks->vmask |= CD_MASK_PROP_ALL;
}

}  // namespace blender

ModifierTypeInfo modifierType_Nodes = {
    /*idname*/ "GeometryNodes",
    /*name*/ N_("GeometryNodes"),
    /*struct_name*/ "NodesModifierData",
    /*struct_size*/ sizeof(NodesModifierData),
    /*srna*/ &RNA_NodesModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/
    (eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
     eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode |
     eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_AcceptsGreasePencil),
    /*icon*/ ICON_GEOMETRY_NODES,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ blender::modify_mesh,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ blender::required_data_mask,
    /*free_data*/ blender::free_data,
    /*is_disabled*/ blender::is_disabled,
    /*update_depsgraph*/ blender::update_depsgraph,
    /*depends_on_time*/ blender::depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ blender::foreach_tex_link,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
