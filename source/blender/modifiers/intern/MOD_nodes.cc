/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_path_util.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_search.hh"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_customdata.h"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_global.h"
#include "BKE_idprop.hh"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BLO_read_write.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.h"

#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_nodes.hh"
#include "MOD_ui_common.hh"

#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_spreadsheet.hh"
#include "ED_undo.hh"
#include "ED_viewer_path.hh"

#include "NOD_geometry.hh"
#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"

#include "FN_field.hh"
#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_execute.hh"
#include "FN_lazy_function_graph_executor.hh"
#include "FN_multi_function.hh"

namespace lf = blender::fn::lazy_function;
namespace geo_log = blender::nodes::geo_eval_log;
namespace bake = blender::bke::bake;

namespace blender {

static void init_data(ModifierData *md)
{
  NodesModifierData *nmd = (NodesModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(nmd, modifier));

  MEMCPY_STRUCT_AFTER(nmd, DNA_struct_default_get(NodesModifierData), modifier);
  nmd->runtime = MEM_new<NodesModifierRuntime>(__func__);
  nmd->runtime->cache = std::make_shared<bake::ModifierCache>();
}

static void add_used_ids_from_sockets(const ListBase &sockets, Set<ID *> &ids)
{
  LISTBASE_FOREACH (const bNodeSocket *, socket, &sockets) {
    switch (socket->type) {
      case SOCK_OBJECT: {
        if (Object *object = ((bNodeSocketValueObject *)socket->default_value)->value) {
          ids.add(&object->id);
        }
        break;
      }
      case SOCK_COLLECTION: {
        if (Collection *collection = ((bNodeSocketValueCollection *)socket->default_value)->value)
        {
          ids.add(&collection->id);
        }
        break;
      }
      case SOCK_MATERIAL: {
        if (Material *material = ((bNodeSocketValueMaterial *)socket->default_value)->value) {
          ids.add(&material->id);
        }
        break;
      }
      case SOCK_TEXTURE: {
        if (Tex *texture = ((bNodeSocketValueTexture *)socket->default_value)->value) {
          ids.add(&texture->id);
        }
        break;
      }
      case SOCK_IMAGE: {
        if (Image *image = ((bNodeSocketValueImage *)socket->default_value)->value) {
          ids.add(&image->id);
        }
        break;
      }
    }
  }
}

/**
 * \note We can only check properties here that cause the dependency graph to update relations when
 * they are changed, otherwise there may be a missing relation after editing. So this could check
 * more properties like whether the node is muted, but we would have to accept the cost of updating
 * relations when those properties are changed.
 */
static bool node_needs_own_transform_relation(const bNode &node)
{
  if (node.type == GEO_NODE_COLLECTION_INFO) {
    const NodeGeometryCollectionInfo &storage = *static_cast<const NodeGeometryCollectionInfo *>(
        node.storage);
    return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  if (node.type == GEO_NODE_OBJECT_INFO) {
    const NodeGeometryObjectInfo &storage = *static_cast<const NodeGeometryObjectInfo *>(
        node.storage);
    return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  if (node.type == GEO_NODE_SELF_OBJECT) {
    return true;
  }
  if (node.type == GEO_NODE_DEFORM_CURVES_ON_SURFACE) {
    return true;
  }

  return false;
}

static void process_nodes_for_depsgraph(const bNodeTree &tree,
                                        Set<ID *> &ids,
                                        bool &r_needs_own_transform_relation,
                                        Set<const bNodeTree *> &checked_groups)
{
  if (!checked_groups.add(&tree)) {
    return;
  }

  tree.ensure_topology_cache();
  for (const bNode *node : tree.all_nodes()) {
    add_used_ids_from_sockets(node->inputs, ids);
    add_used_ids_from_sockets(node->outputs, ids);
    r_needs_own_transform_relation |= node_needs_own_transform_relation(*node);
  }

  for (const bNode *node : tree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      process_nodes_for_depsgraph(*sub_tree, ids, r_needs_own_transform_relation, checked_groups);
    }
  }
}

static void find_used_ids_from_settings(const NodesModifierSettings &settings, Set<ID *> &ids)
{
  IDP_foreach_property(
      settings.properties,
      IDP_TYPE_FILTER_ID,
      [](IDProperty *property, void *user_data) {
        Set<ID *> *ids = (Set<ID *> *)user_data;
        ID *id = IDP_Id(property);
        if (id != nullptr) {
          ids->add(id);
        }
      },
      &ids);
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

static void add_object_relation(const ModifierUpdateDepsgraphContext *ctx, Object &object)
{
  DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_TRANSFORM, "Nodes Modifier");
  if (&(ID &)object != &ctx->object->id) {
    if (object.type == OB_EMPTY && object.instance_collection != nullptr) {
      add_collection_relation(ctx, *object.instance_collection);
    }
    else if (DEG_object_has_geometry_component(&object)) {
      DEG_add_object_relation(ctx->node, &object, DEG_OB_COMP_GEOMETRY, "Nodes Modifier");
      DEG_add_customdata_mask(ctx->node, &object, &dependency_data_mask);
    }
  }
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return;
  }

  DEG_add_node_tree_output_relation(ctx->node, nmd->node_group, "Nodes Modifier");

  bool needs_own_transform_relation = false;
  Set<ID *> used_ids;
  find_used_ids_from_settings(nmd->settings, used_ids);
  Set<const bNodeTree *> checked_groups;
  process_nodes_for_depsgraph(
      *nmd->node_group, used_ids, needs_own_transform_relation, checked_groups);

  if (ctx->object->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(ctx->object->data);
    if (curves_id->surface != nullptr) {
      used_ids.add(&curves_id->surface->id);
    }
  }

  for (ID *id : used_ids) {
    switch ((ID_Type)GS(id->name)) {
      case ID_OB: {
        Object *object = reinterpret_cast<Object *>(id);
        add_object_relation(ctx, *object);
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

  if (needs_own_transform_relation) {
    DEG_add_depends_on_transform_relation(ctx->node, "Nodes Modifier");
  }
}

static bool check_tree_for_time_node(const bNodeTree &tree, Set<const bNodeTree *> &checked_groups)
{
  if (!checked_groups.add(&tree)) {
    return false;
  }
  tree.ensure_topology_cache();
  if (!tree.nodes_by_type("GeometryNodeInputSceneTime").is_empty()) {
    return true;
  }
  if (!tree.nodes_by_type("GeometryNodeSimulationInput").is_empty()) {
    return true;
  }
  for (const bNode *node : tree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      if (check_tree_for_time_node(*sub_tree, checked_groups)) {
        return true;
      }
    }
  }
  return false;
}

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  const bNodeTree *tree = nmd->node_group;
  if (tree == nullptr) {
    return false;
  }
  Set<const bNodeTree *> checked_groups;
  return check_tree_for_time_node(*tree, checked_groups);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  walk(user_data, ob, (ID **)&nmd->node_group, IDWALK_CB_USER);

  struct ForeachSettingData {
    IDWalkFunc walk;
    void *user_data;
    Object *ob;
  } settings = {walk, user_data, ob};

  IDP_foreach_property(
      nmd->settings.properties,
      IDP_TYPE_FILTER_ID,
      [](IDProperty *id_prop, void *user_data) {
        ForeachSettingData *settings = (ForeachSettingData *)user_data;
        settings->walk(
            settings->user_data, settings->ob, (ID **)&id_prop->data.pointer, IDWALK_CB_USER);
      },
      &settings);
}

static void foreach_tex_link(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data)
{
  walk(user_data, ob, md, "texture");
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

}  // namespace blender

void MOD_nodes_update_interface(Object *object, NodesModifierData *nmd)
{
  using namespace blender;
  if (nmd->node_group == nullptr) {
    if (nmd->settings.properties) {
      IDP_FreeProperty(nmd->settings.properties);
      nmd->settings.properties = nullptr;
    }
    return;
  }

  IDProperty *old_properties = nmd->settings.properties;
  {
    IDPropertyTemplate idprop = {0};
    nmd->settings.properties = IDP_New(IDP_GROUP, &idprop, "Nodes Modifier Settings");
  }
  IDProperty *new_properties = nmd->settings.properties;

  nodes::update_input_properties_from_node_tree(
      *nmd->node_group, old_properties, false, *new_properties);
  nodes::update_output_properties_from_node_tree(
      *nmd->node_group, old_properties, *new_properties);

  if (old_properties != nullptr) {
    IDP_FreeProperty(old_properties);
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
}

namespace blender {

static void find_side_effect_nodes_for_viewer_path(
    const ViewerPath &viewer_path,
    const NodesModifierData &nmd,
    const ModifierEvalContext &ctx,
    MultiValueMap<ComputeContextHash, const lf::FunctionNode *> &r_side_effect_nodes)
{
  const std::optional<ed::viewer_path::ViewerPathForGeometryNodesViewer> parsed_path =
      ed::viewer_path::parse_geometry_nodes_viewer(viewer_path);
  if (!parsed_path.has_value()) {
    return;
  }
  if (parsed_path->object != DEG_get_original_object(ctx.object)) {
    return;
  }
  if (parsed_path->modifier_name != nmd.modifier.name) {
    return;
  }

  ComputeContextBuilder compute_context_builder;
  compute_context_builder.push<bke::ModifierComputeContext>(parsed_path->modifier_name);

  /* Write side effect nodes to a new map and only if everything succeeds, move the nodes to the
   * caller. This is easier than changing r_side_effect_nodes directly and then undoing changes in
   * case of errors. */
  MultiValueMap<ComputeContextHash, const lf::FunctionNode *> local_side_effect_nodes;

  const bNodeTree *group = nmd.node_group;
  const bke::bNodeTreeZone *zone = nullptr;
  for (const ViewerPathElem *elem : parsed_path->node_path) {
    const bke::bNodeTreeZones *tree_zones = group->zones();
    if (tree_zones == nullptr) {
      return;
    }
    const auto *lf_graph_info = nodes::ensure_geometry_nodes_lazy_function_graph(*group);
    if (lf_graph_info == nullptr) {
      return;
    }
    switch (elem->type) {
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const SimulationZoneViewerPathElem *>(elem);
        const bke::bNodeTreeZone *next_zone = tree_zones->get_zone_by_node(
            typed_elem.sim_output_node_id);
        if (next_zone == nullptr) {
          return;
        }
        if (next_zone->parent_zone != zone) {
          return;
        }
        const lf::FunctionNode *lf_zone_node = lf_graph_info->mapping.zone_node_map.lookup_default(
            next_zone, nullptr);
        if (lf_zone_node == nullptr) {
          return;
        }
        local_side_effect_nodes.add(compute_context_builder.hash(), lf_zone_node);
        compute_context_builder.push<bke::SimulationZoneComputeContext>(*next_zone->output_node);
        zone = next_zone;
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const RepeatZoneViewerPathElem *>(elem);
        const bke::bNodeTreeZone *next_zone = tree_zones->get_zone_by_node(
            typed_elem.repeat_output_node_id);
        if (next_zone == nullptr) {
          return;
        }
        if (next_zone->parent_zone != zone) {
          return;
        }
        const lf::FunctionNode *lf_zone_node = lf_graph_info->mapping.zone_node_map.lookup_default(
            next_zone, nullptr);
        if (lf_zone_node == nullptr) {
          return;
        }
        local_side_effect_nodes.add(compute_context_builder.hash(), lf_zone_node);
        compute_context_builder.push<bke::RepeatZoneComputeContext>(*next_zone->output_node,
                                                                    typed_elem.iteration);
        zone = next_zone;
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
        const auto &typed_elem = *reinterpret_cast<const GroupNodeViewerPathElem *>(elem);
        const bNode *node = group->node_by_id(typed_elem.node_id);
        if (node == nullptr) {
          return;
        }
        if (node->id == nullptr) {
          return;
        }
        if (node->is_muted()) {
          return;
        }
        if (zone != tree_zones->get_zone_by_node(node->identifier)) {
          return;
        }
        const lf::FunctionNode *lf_group_node =
            lf_graph_info->mapping.group_node_map.lookup_default(node, nullptr);
        if (lf_group_node == nullptr) {
          return;
        }
        local_side_effect_nodes.add(compute_context_builder.hash(), lf_group_node);
        compute_context_builder.push<bke::NodeGroupComputeContext>(*node);
        group = reinterpret_cast<const bNodeTree *>(node->id);
        zone = nullptr;
        break;
      }
      default: {
        BLI_assert_unreachable();
        return;
      }
    }
  }

  const bNode *found_viewer_node = group->node_by_id(parsed_path->viewer_node_id);
  if (found_viewer_node == nullptr) {
    return;
  }
  const auto *lf_graph_info = nodes::ensure_geometry_nodes_lazy_function_graph(*group);
  if (lf_graph_info == nullptr) {
    return;
  }
  const bke::bNodeTreeZones *tree_zones = group->zones();
  if (tree_zones == nullptr) {
    return;
  }
  if (tree_zones->get_zone_by_node(found_viewer_node->identifier) != zone) {
    return;
  }
  const lf::FunctionNode *lf_viewer_node = lf_graph_info->mapping.viewer_node_map.lookup_default(
      found_viewer_node, nullptr);
  if (lf_viewer_node == nullptr) {
    return;
  }
  local_side_effect_nodes.add(compute_context_builder.hash(), lf_viewer_node);

  /* Successfully found all compute contexts for the viewer. */
  for (const auto item : local_side_effect_nodes.items()) {
    r_side_effect_nodes.add_multiple(item.key, item.value);
  }
}

static void find_side_effect_nodes(
    const NodesModifierData &nmd,
    const ModifierEvalContext &ctx,
    MultiValueMap<ComputeContextHash, const lf::FunctionNode *> &r_side_effect_nodes)
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
            sspreadsheet.viewer_path, nmd, ctx, r_side_effect_nodes);
      }
      if (sl->spacetype == SPACE_VIEW3D) {
        const View3D &v3d = *reinterpret_cast<const View3D *>(sl);
        find_side_effect_nodes_for_viewer_path(v3d.viewer_path, nmd, ctx, r_side_effect_nodes);
      }
    }
  }
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
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      const SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype == SPACE_NODE) {
        const SpaceNode &snode = *reinterpret_cast<const SpaceNode *>(sl);
        const Map<const bke::bNodeTreeZone *, ComputeContextHash> hash_by_zone =
            geo_log::GeoModifierLog::get_context_hash_by_zone_for_node_editor(snode,
                                                                              nmd.modifier.name);
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
static void check_property_socket_sync(const Object *ob, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);

  int geometry_socket_count = 0;

  nmd->node_group->ensure_interface_cache();
  for (const int i : nmd->node_group->interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket *socket = nmd->node_group->interface_inputs()[i];
    const bNodeSocketType *typeinfo = socket->socket_typeinfo();
    const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
    /* The first socket is the special geometry socket for the modifier object. */
    if (i == 0 && type == SOCK_GEOMETRY) {
      geometry_socket_count++;
      continue;
    }

    IDProperty *property = IDP_GetPropertyFromGroup(nmd->settings.properties, socket->identifier);
    if (property == nullptr) {
      if (type == SOCK_GEOMETRY) {
        geometry_socket_count++;
      }
      else {
        BKE_modifier_set_error(ob, md, "Missing property for input socket \"%s\"", socket->name);
      }
      continue;
    }

    if (!nodes::id_property_type_matches_socket(*socket, *property)) {
      BKE_modifier_set_error(
          ob, md, "Property type does not match input socket \"(%s)\"", socket->name);
      continue;
    }
  }

  if (geometry_socket_count == 1) {
    const bNodeTreeInterfaceSocket *first_socket = nmd->node_group->interface_inputs()[0];
    const bNodeSocketType *typeinfo = first_socket->socket_typeinfo();
    const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
    if (type != SOCK_GEOMETRY) {
      BKE_modifier_set_error(ob, md, "Node group's geometry input must be the first");
    }
  }
}

namespace sim_input = nodes::sim_input;
namespace sim_output = nodes::sim_output;

class NodesModifierSimulationParams : public nodes::GeoNodesSimulationParams {
 private:
  static constexpr float max_delta_frames = 1.0f;

  mutable Map<int, std::unique_ptr<nodes::SimulationZoneBehavior>> behavior_by_zone_id_;
  const NodesModifierData &nmd_;
  const ModifierEvalContext &ctx_;
  const Main *bmain_;
  SubFrame current_frame_;
  SubFrame start_frame_;
  bool is_start_frame_;
  bool use_frame_cache_;
  bool depsgraph_is_active_;
  bake::ModifierCache *modifier_cache_;
  float fps_;

 public:
  NodesModifierSimulationParams(NodesModifierData &nmd, const ModifierEvalContext &ctx)
      : nmd_(nmd), ctx_(ctx)
  {
    const Depsgraph *depsgraph = ctx_.depsgraph;
    bmain_ = DEG_get_bmain(depsgraph);
    current_frame_ = DEG_get_ctime(depsgraph);
    const Scene *scene = DEG_get_input_scene(depsgraph);
    start_frame_ = scene->r.sfra;
    is_start_frame_ = current_frame_ == start_frame_;
    use_frame_cache_ = ctx_.object->flag & OB_FLAG_USE_SIMULATION_CACHE;
    depsgraph_is_active_ = DEG_is_active(depsgraph);
    modifier_cache_ = nmd.runtime->cache.get();
    fps_ = FPS;

    if (!modifier_cache_) {
      return;
    }
    std::lock_guard lock{modifier_cache_->mutex};
    if (depsgraph_is_active_) {
      /* Invalidate data on user edits. */
      if (nmd.modifier.flag & eModifierFlag_UserModified) {
        for (std::unique_ptr<bake::NodeCache> &node_cache : modifier_cache_->cache_by_id.values())
        {
          if (node_cache->cache_status != bake::CacheStatus::Baked) {
            node_cache->cache_status = bake::CacheStatus::Invalid;
          }
        }
      }
      /* Reset cached data if necessary. */
      if (is_start_frame_) {
        for (std::unique_ptr<bake::NodeCache> &node_cache : modifier_cache_->cache_by_id.values())
        {
          if (node_cache->cache_status == bake::CacheStatus::Invalid) {
            node_cache->reset();
          }
        }
      }
    }
  }

  nodes::SimulationZoneBehavior *get(const int zone_id) const override
  {
    if (!modifier_cache_) {
      return nullptr;
    }
    std::lock_guard lock{modifier_cache_->mutex};
    return behavior_by_zone_id_
        .lookup_or_add_cb(zone_id,
                          [&]() {
                            auto info = std::make_unique<nodes::SimulationZoneBehavior>();
                            this->init_simulation_info(zone_id, *info);
                            return info;
                          })
        .get();
  }

  struct FrameIndices {
    std::optional<int> prev;
    std::optional<int> current;
    std::optional<int> next;
  };

  void init_simulation_info(const int zone_id, nodes::SimulationZoneBehavior &zone_behavior) const
  {
    bake::NodeCache &node_cache = *modifier_cache_->cache_by_id.lookup_or_add_cb(
        zone_id, []() { return std::make_unique<bake::NodeCache>(); });

    /* Try load baked data. */
    if (!node_cache.failed_finding_bake) {
      if (node_cache.cache_status != bake::CacheStatus::Baked) {
        if (std::optional<bake::BakePath> zone_bake_path = bake::get_node_bake_path(
                *bmain_, *ctx_.object, nmd_, zone_id))
        {

          Vector<bake::MetaFile> meta_files = bake::find_sorted_meta_files(
              zone_bake_path->meta_dir);
          if (!meta_files.is_empty()) {
            node_cache.reset();

            for (const bake::MetaFile &meta_file : meta_files) {
              auto frame_cache = std::make_unique<bake::FrameCache>();
              frame_cache->frame = meta_file.frame;
              frame_cache->meta_path = meta_file.path;
              node_cache.frame_caches.append(std::move(frame_cache));
            }
            node_cache.blobs_dir = zone_bake_path->blobs_dir;
            node_cache.blob_sharing = std::make_unique<bke::bake::BlobSharing>();
            node_cache.cache_status = bake::CacheStatus::Baked;
          }
        }
      }
      if (node_cache.cache_status != bake::CacheStatus::Baked) {
        node_cache.failed_finding_bake = true;
      }
    }

    const FrameIndices frame_indices = this->get_frame_indices(node_cache);
    if (node_cache.cache_status == bake::CacheStatus::Baked) {
      this->read_from_cache(frame_indices, node_cache, zone_behavior);
      return;
    }
    if (use_frame_cache_) {
      /* If the depsgraph is active, we allow creating new simulation states. Otherwise, the access
       * is read-only. */
      if (depsgraph_is_active_) {
        if (node_cache.frame_caches.is_empty()) {
          /* Initialize the simulation. */
          this->input_pass_through(zone_behavior);
          this->output_store_frame_cache(node_cache, zone_behavior);
          if (!is_start_frame_) {
            /* If we initialize at a frame that is not the start frame, the simulation is not
             * valid. */
            node_cache.cache_status = bake::CacheStatus::Invalid;
          }
          return;
        }
        if (frame_indices.prev && !frame_indices.current && !frame_indices.next) {
          /* Read the previous frame's data and store the newly computed simulation state. */
          auto &output_copy_info = zone_behavior.input.emplace<sim_input::OutputCopy>();
          const bake::FrameCache &prev_frame_cache = *node_cache.frame_caches[*frame_indices.prev];
          const float delta_frames = std::min(
              max_delta_frames, float(current_frame_) - float(prev_frame_cache.frame));
          if (delta_frames != 1) {
            node_cache.cache_status = bake::CacheStatus::Invalid;
          }
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

  FrameIndices get_frame_indices(const bake::NodeCache &node_cache) const
  {
    FrameIndices frame_indices;
    if (!node_cache.frame_caches.is_empty()) {
      const int first_future_frame_index = binary_search::find_predicate_begin(
          node_cache.frame_caches, [&](const std::unique_ptr<bake::FrameCache> &value) {
            return value->frame > current_frame_;
          });
      frame_indices.next = (first_future_frame_index == node_cache.frame_caches.size()) ?
                               std::nullopt :
                               std::optional<int>(first_future_frame_index);
      if (first_future_frame_index > 0) {
        const int index = first_future_frame_index - 1;
        if (node_cache.frame_caches[index]->frame < current_frame_) {
          frame_indices.prev = index;
        }
        else {
          BLI_assert(node_cache.frame_caches[index]->frame == current_frame_);
          frame_indices.current = index;
          if (index > 0) {
            frame_indices.prev = index - 1;
          }
        }
      }
    }
    return frame_indices;
  }

  void input_pass_through(nodes::SimulationZoneBehavior &zone_behavior) const
  {
    zone_behavior.input.emplace<sim_input::PassThrough>();
  }

  void output_pass_through(nodes::SimulationZoneBehavior &zone_behavior) const
  {
    zone_behavior.output.emplace<sim_output::PassThrough>();
  }

  void output_store_frame_cache(bake::NodeCache &node_cache,
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
      node_cache->frame_caches.append(std::move(frame_cache));
    };
  }

  void store_as_prev_items(bake::NodeCache &node_cache,
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

  void read_from_cache(const FrameIndices &frame_indices,
                       bake::NodeCache &node_cache,
                       nodes::SimulationZoneBehavior &zone_behavior) const
  {
    if (frame_indices.prev) {
      auto &output_copy_info = zone_behavior.input.emplace<sim_input::OutputCopy>();
      bake::FrameCache &frame_cache = *node_cache.frame_caches[*frame_indices.prev];
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
                   bake::NodeCache &node_cache,
                   nodes::SimulationZoneBehavior &zone_behavior) const
  {
    bake::FrameCache &frame_cache = *node_cache.frame_caches[frame_index];
    this->ensure_bake_loaded(node_cache, frame_cache);
    auto &read_single_info = zone_behavior.output.emplace<sim_output::ReadSingle>();
    read_single_info.state = frame_cache.state;
  }

  void read_interpolated(const int prev_frame_index,
                         const int next_frame_index,
                         bake::NodeCache &node_cache,
                         nodes::SimulationZoneBehavior &zone_behavior) const
  {
    bake::FrameCache &prev_frame_cache = *node_cache.frame_caches[prev_frame_index];
    bake::FrameCache &next_frame_cache = *node_cache.frame_caches[next_frame_index];
    this->ensure_bake_loaded(node_cache, prev_frame_cache);
    this->ensure_bake_loaded(node_cache, next_frame_cache);
    auto &read_interpolated_info = zone_behavior.output.emplace<sim_output::ReadInterpolated>();
    read_interpolated_info.mix_factor = (float(current_frame_) - float(prev_frame_cache.frame)) /
                                        (float(next_frame_cache.frame) -
                                         float(prev_frame_cache.frame));
    read_interpolated_info.prev_state = prev_frame_cache.state;
    read_interpolated_info.next_state = next_frame_cache.state;
  }

  void ensure_bake_loaded(bake::NodeCache &node_cache, bake::FrameCache &frame_cache) const
  {
    if (!frame_cache.state.items_by_id.is_empty()) {
      return;
    }
    if (!node_cache.blobs_dir) {
      return;
    }
    if (!frame_cache.meta_path) {
      return;
    }
    bke::bake::DiskBlobReader blob_reader{*node_cache.blobs_dir};
    fstream meta_file{*frame_cache.meta_path};
    std::optional<bke::bake::BakeState> bake_state = bke::bake::deserialize_bake(
        meta_file, blob_reader, *node_cache.blob_sharing);
    if (!bake_state.has_value()) {
      return;
    }
    frame_cache.state = std::move(*bake_state);
  }
};

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

  const bNodeTree &tree = *nmd->node_group;
  check_property_socket_sync(ctx->object, md);

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

  nodes::GeoNodesModifierData modifier_eval_data{};
  modifier_eval_data.depsgraph = ctx->depsgraph;
  modifier_eval_data.self_object = ctx->object;
  auto eval_log = std::make_unique<geo_log::GeoModifierLog>();

  NodesModifierSimulationParams simulation_params(*nmd, *ctx);
  modifier_eval_data.simulation_params = &simulation_params;

  Set<ComputeContextHash> socket_log_contexts;
  if (logging_enabled(ctx)) {
    modifier_eval_data.eval_log = eval_log.get();

    find_socket_log_contexts(*nmd, *ctx, socket_log_contexts);
    modifier_eval_data.socket_log_contexts = &socket_log_contexts;
  }
  MultiValueMap<ComputeContextHash, const lf::FunctionNode *> side_effect_nodes;
  find_side_effect_nodes(*nmd, *ctx, side_effect_nodes);
  modifier_eval_data.side_effect_nodes = &side_effect_nodes;

  bke::ModifierComputeContext modifier_compute_context{nullptr, nmd->modifier.name};

  geometry_set = nodes::execute_geometry_nodes_on_geometry(
      tree,
      nmd->settings.properties,
      modifier_compute_context,
      std::move(geometry_set),
      [&](nodes::GeoNodesLFUserData &user_data) {
        user_data.modifier_data = &modifier_eval_data;
      });

  if (logging_enabled(ctx)) {
    nmd_orig->runtime->eval_log = std::move(eval_log);
  }

  if (use_orig_index_verts || use_orig_index_edges || use_orig_index_faces) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      /* Add #CD_ORIGINDEX layers if they don't exist already. This is required because the
       * #eModifierTypeFlag_SupportsMapping flag is set. If the layers did not exist before, it is
       * assumed that the output mesh does not have a mapping to the original mesh. */
      if (use_orig_index_verts) {
        CustomData_add_layer(&mesh->vert_data, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->totvert);
      }
      if (use_orig_index_edges) {
        CustomData_add_layer(&mesh->edge_data, CD_ORIGINDEX, CD_SET_DEFAULT, mesh->totedge);
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

  Mesh *new_mesh = geometry_set.get_component_for_write<bke::MeshComponent>().release();
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

struct AttributeSearchData {
  uint32_t object_session_uid;
  char modifier_name[MAX_NAME];
  char socket_identifier[MAX_NAME];
  bool is_output;
};
/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<AttributeSearchData>, "");

static NodesModifierData *get_modifier_data(Main &bmain,
                                            const wmWindowManager &wm,
                                            const AttributeSearchData &data)
{
  if (ED_screen_animation_playing(&wm)) {
    /* Work around an issue where the attribute search exec function has stale pointers when data
     * is reallocated when evaluating the node tree, causing a crash. This would be solved by
     * allowing the UI search data to own arbitrary memory rather than just referencing it. */
    return nullptr;
  }

  const Object *object = (Object *)BKE_libblock_find_session_uuid(
      &bmain, ID_OB, data.object_session_uid);
  if (object == nullptr) {
    return nullptr;
  }
  ModifierData *md = BKE_modifiers_findby_name(object, data.modifier_name);
  if (md == nullptr) {
    return nullptr;
  }
  BLI_assert(md->type == eModifierType_Nodes);
  return reinterpret_cast<NodesModifierData *>(md);
}

static geo_log::GeoTreeLog *get_root_tree_log(const NodesModifierData &nmd)
{
  if (!nmd.runtime->eval_log) {
    return nullptr;
  }
  bke::ModifierComputeContext compute_context{nullptr, nmd.modifier.name};
  return &nmd.runtime->eval_log->get_tree_log(compute_context.hash());
}

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AttributeSearchData &data = *static_cast<AttributeSearchData *>(arg);
  const NodesModifierData *nmd = get_modifier_data(*CTX_data_main(C), *CTX_wm_manager(C), data);
  if (nmd == nullptr) {
    return;
  }
  if (nmd->node_group == nullptr) {
    return;
  }
  geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
  if (tree_log == nullptr) {
    return;
  }
  tree_log->ensure_existing_attributes();
  nmd->node_group->ensure_topology_cache();

  Vector<const bNodeSocket *> sockets_to_check;
  if (data.is_output) {
    for (const bNode *node : nmd->node_group->nodes_by_type("NodeGroupOutput")) {
      for (const bNodeSocket *socket : node->input_sockets()) {
        if (socket->type == SOCK_GEOMETRY) {
          sockets_to_check.append(socket);
        }
      }
    }
  }
  else {
    for (const bNode *node : nmd->node_group->group_input_nodes()) {
      for (const bNodeSocket *socket : node->output_sockets()) {
        if (socket->type == SOCK_GEOMETRY) {
          sockets_to_check.append(socket);
        }
      }
    }
  }
  Set<StringRef> names;
  Vector<const geo_log::GeometryAttributeInfo *> attributes;
  for (const bNodeSocket *socket : sockets_to_check) {
    const geo_log::ValueLog *value_log = tree_log->find_socket_value_log(*socket);
    if (value_log == nullptr) {
      continue;
    }
    if (const auto *geo_log = dynamic_cast<const geo_log::GeometryInfoLog *>(value_log)) {
      for (const geo_log::GeometryAttributeInfo &attribute : geo_log->attributes) {
        if (names.add(attribute.name)) {
          attributes.append(&attribute);
        }
      }
    }
  }
  ui::attribute_search_add_items(str, data.is_output, attributes.as_span(), items, is_first);
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (item_v == nullptr) {
    return;
  }
  AttributeSearchData &data = *static_cast<AttributeSearchData *>(data_v);
  const auto &item = *static_cast<const geo_log::GeometryAttributeInfo *>(item_v);
  const NodesModifierData *nmd = get_modifier_data(*CTX_data_main(C), *CTX_wm_manager(C), data);
  if (nmd == nullptr) {
    return;
  }

  const std::string attribute_prop_name = data.socket_identifier +
                                          nodes::input_attribute_name_suffix();
  IDProperty &name_property = *IDP_GetPropertyFromGroup(nmd->settings.properties,
                                                        attribute_prop_name.c_str());
  IDP_AssignString(&name_property, item.name.c_str());

  ED_undo_push(C, "Assign Attribute Name");
}

static void add_attribute_search_button(const bContext &C,
                                        uiLayout *layout,
                                        const NodesModifierData &nmd,
                                        PointerRNA *md_ptr,
                                        const StringRefNull rna_path_attribute_name,
                                        const bNodeTreeInterfaceSocket &socket,
                                        const bool is_output)
{
  if (!nmd.runtime->eval_log) {
    uiItemR(layout, md_ptr, rna_path_attribute_name.c_str(), UI_ITEM_NONE, "", ICON_NONE);
    return;
  }

  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but = uiDefIconTextButR(block,
                                 UI_BTYPE_SEARCH_MENU,
                                 0,
                                 ICON_NONE,
                                 "",
                                 0,
                                 0,
                                 10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                 UI_UNIT_Y,
                                 md_ptr,
                                 rna_path_attribute_name.c_str(),
                                 0,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 socket.description);

  const Object *object = ED_object_context(&C);
  BLI_assert(object != nullptr);
  if (object == nullptr) {
    return;
  }

  AttributeSearchData *data = MEM_new<AttributeSearchData>(__func__);
  data->object_session_uid = object->id.session_uuid;
  STRNCPY(data->modifier_name, nmd.modifier.name);
  STRNCPY(data->socket_identifier, socket.identifier);
  data->is_output = is_output;

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         attribute_search_exec_fn,
                         nullptr);

  char *attribute_name = RNA_string_get_alloc(
      md_ptr, rna_path_attribute_name.c_str(), nullptr, 0, nullptr);
  const bool access_allowed = bke::allow_procedural_attribute_access(attribute_name);
  MEM_freeN(attribute_name);
  if (!access_allowed) {
    UI_but_flag_enable(but, UI_BUT_REDALERT);
  }
}

static void add_attribute_search_or_value_buttons(const bContext &C,
                                                  uiLayout *layout,
                                                  const NodesModifierData &nmd,
                                                  PointerRNA *md_ptr,
                                                  const bNodeTreeInterfaceSocket &socket)
{
  const StringRefNull identifier = socket.identifier;
  const bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
  char socket_id_esc[MAX_NAME * 2];
  BLI_str_escape(socket_id_esc, identifier.c_str(), sizeof(socket_id_esc));
  const std::string rna_path = "[\"" + std::string(socket_id_esc) + "\"]";
  const std::string rna_path_attribute_name = "[\"" + std::string(socket_id_esc) +
                                              nodes::input_attribute_name_suffix() + "\"]";

  /* We're handling this manually in this case. */
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *split = uiLayoutSplit(layout, 0.4f, false);
  uiLayout *name_row = uiLayoutRow(split, false);
  uiLayoutSetAlignment(name_row, UI_LAYOUT_ALIGN_RIGHT);

  const std::optional<StringRef> attribute_name = nodes::input_attribute_name_get(
      *nmd.settings.properties, socket);
  if (type == SOCK_BOOLEAN && !attribute_name) {
    uiItemL(name_row, "", ICON_NONE);
  }
  else {
    uiItemL(name_row, socket.name, ICON_NONE);
  }

  uiLayout *prop_row = uiLayoutRow(split, true);
  if (type == SOCK_BOOLEAN) {
    uiLayoutSetPropSep(prop_row, false);
    uiLayoutSetAlignment(prop_row, UI_LAYOUT_ALIGN_EXPAND);
  }

  if (attribute_name) {
    add_attribute_search_button(C, prop_row, nmd, md_ptr, rna_path_attribute_name, socket, false);
    uiItemL(layout, "", ICON_BLANK1);
  }
  else {
    const char *name = type == SOCK_BOOLEAN ? socket.name : "";
    uiItemR(prop_row, md_ptr, rna_path.c_str(), UI_ITEM_NONE, name, ICON_NONE);
    uiItemDecoratorR(layout, md_ptr, rna_path.c_str(), -1);
  }

  PointerRNA props;
  uiItemFullO(prop_row,
              "object.geometry_nodes_input_attribute_toggle",
              "",
              ICON_SPREADSHEET,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &props);
  RNA_string_set(&props, "modifier_name", nmd.modifier.name);
  RNA_string_set(&props, "input_name", socket.identifier);
}

/* Drawing the properties manually with #uiItemR instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI. */
static void draw_property_for_socket(const bContext &C,
                                     uiLayout *layout,
                                     NodesModifierData *nmd,
                                     PointerRNA *bmain_ptr,
                                     PointerRNA *md_ptr,
                                     const bNodeTreeInterfaceSocket &socket,
                                     const int socket_index)
{
  const StringRefNull identifier = socket.identifier;
  /* The property should be created in #MOD_nodes_update_interface with the correct type. */
  IDProperty *property = IDP_GetPropertyFromGroup(nmd->settings.properties, identifier.c_str());

  /* IDProperties can be removed with python, so there could be a situation where
   * there isn't a property for a socket or it doesn't have the correct type. */
  if (property == nullptr || !nodes::id_property_type_matches_socket(socket, *property)) {
    return;
  }

  char socket_id_esc[MAX_NAME * 2];
  BLI_str_escape(socket_id_esc, identifier.c_str(), sizeof(socket_id_esc));

  char rna_path[sizeof(socket_id_esc) + 4];
  SNPRINTF(rna_path, "[\"%s\"]", socket_id_esc);

  uiLayout *row = uiLayoutRow(layout, true);
  uiLayoutSetPropDecorate(row, true);

  /* Use #uiItemPointerR to draw pointer properties because #uiItemR would not have enough
   * information about what type of ID to select for editing the values. This is because
   * pointer IDProperties contain no information about their type. */
  const bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
  switch (type) {
    case SOCK_OBJECT: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "objects", socket.name, ICON_OBJECT_DATA);
      break;
    }
    case SOCK_COLLECTION: {
      uiItemPointerR(
          row, md_ptr, rna_path, bmain_ptr, "collections", socket.name, ICON_OUTLINER_COLLECTION);
      break;
    }
    case SOCK_MATERIAL: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "materials", socket.name, ICON_MATERIAL);
      break;
    }
    case SOCK_TEXTURE: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "textures", socket.name, ICON_TEXTURE);
      break;
    }
    case SOCK_IMAGE: {
      uiItemPointerR(row, md_ptr, rna_path, bmain_ptr, "images", socket.name, ICON_IMAGE);
      break;
    }
    default: {
      if (nodes::input_has_attribute_toggle(*nmd->node_group, socket_index)) {
        add_attribute_search_or_value_buttons(C, row, *nmd, md_ptr, socket);
      }
      else {
        uiItemR(row, md_ptr, rna_path, UI_ITEM_NONE, socket.name, ICON_NONE);
      }
    }
  }
  if (!nodes::input_has_attribute_toggle(*nmd->node_group, socket_index)) {
    uiItemL(row, "", ICON_BLANK1);
  }
}

static void draw_property_for_output_socket(const bContext &C,
                                            uiLayout *layout,
                                            const NodesModifierData &nmd,
                                            PointerRNA *md_ptr,
                                            const bNodeTreeInterfaceSocket &socket)
{
  const StringRefNull identifier = socket.identifier;
  char socket_id_esc[MAX_NAME * 2];
  BLI_str_escape(socket_id_esc, identifier.c_str(), sizeof(socket_id_esc));
  const std::string rna_path_attribute_name = "[\"" + StringRef(socket_id_esc) +
                                              nodes::input_attribute_name_suffix() + "\"]";

  uiLayout *split = uiLayoutSplit(layout, 0.4f, false);
  uiLayout *name_row = uiLayoutRow(split, false);
  uiLayoutSetAlignment(name_row, UI_LAYOUT_ALIGN_RIGHT);
  uiItemL(name_row, socket.name, ICON_NONE);

  uiLayout *row = uiLayoutRow(split, true);
  add_attribute_search_button(C, row, nmd, md_ptr, rna_path_attribute_name, socket, true);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Main *bmain = CTX_data_main(C);

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);
  /* Decorators are added manually for supported properties because the
   * attribute/value toggle requires a manually built layout anyway. */
  uiLayoutSetPropDecorate(layout, false);

  if (!(nmd->flag & NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR)) {
    uiTemplateID(layout,
                 C,
                 ptr,
                 "node_group",
                 "node.new_geometry_node_group_assign",
                 nullptr,
                 nullptr,
                 0,
                 false,
                 nullptr);
  }

  if (nmd->node_group != nullptr && nmd->settings.properties != nullptr) {
    PointerRNA bmain_ptr = RNA_main_pointer_create(bmain);

    nmd->node_group->ensure_interface_cache();

    for (const int socket_index : nmd->node_group->interface_inputs().index_range()) {
      const bNodeTreeInterfaceSocket *socket = nmd->node_group->interface_inputs()[socket_index];
      if (!(socket->flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
        draw_property_for_socket(*C, layout, nmd, &bmain_ptr, ptr, *socket, socket_index);
      }
    }
  }

  /* Draw node warnings. */
  geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
  if (tree_log != nullptr) {
    tree_log->ensure_node_warnings();
    for (const geo_log::NodeWarning &warning : tree_log->all_warnings) {
      if (warning.type != geo_log::NodeWarningType::Info) {
        uiItemL(layout, warning.message.c_str(), ICON_ERROR);
      }
    }
  }

  modifier_panel_end(layout, ptr);
}

static void output_attribute_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, true);

  bool has_output_attribute = false;
  if (nmd->node_group != nullptr && nmd->settings.properties != nullptr) {
    for (const bNodeTreeInterfaceSocket *socket : nmd->node_group->interface_outputs()) {
      const bNodeSocketType *typeinfo = socket->socket_typeinfo();
      const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) :
                                                  SOCK_CUSTOM;
      if (nodes::socket_type_has_attribute_toggle(type)) {
        has_output_attribute = true;
        draw_property_for_output_socket(*C, layout, *nmd, ptr, *socket);
      }
    }
  }
  if (!has_output_attribute) {
    uiItemL(layout, TIP_("No group output attributes connected"), ICON_INFO);
  }
}

static void internal_dependencies_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  NodesModifierData *nmd = static_cast<NodesModifierData *>(ptr->data);

  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayoutSetPropSep(col, true);
  uiLayoutSetPropDecorate(col, false);
  uiItemR(col, ptr, "simulation_bake_directory", UI_ITEM_NONE, "Bake", ICON_NONE);

  geo_log::GeoTreeLog *tree_log = get_root_tree_log(*nmd);
  if (tree_log == nullptr) {
    return;
  }

  tree_log->ensure_used_named_attributes();
  const Map<StringRefNull, geo_log::NamedAttributeUsage> &usage_by_attribute =
      tree_log->used_named_attributes;

  if (usage_by_attribute.is_empty()) {
    uiItemL(layout, IFACE_("No named attributes used"), ICON_INFO);
    return;
  }

  struct NameWithUsage {
    StringRefNull name;
    geo_log::NamedAttributeUsage usage;
  };

  Vector<NameWithUsage> sorted_used_attribute;
  for (auto &&item : usage_by_attribute.items()) {
    sorted_used_attribute.append({item.key, item.value});
  }
  std::sort(sorted_used_attribute.begin(),
            sorted_used_attribute.end(),
            [](const NameWithUsage &a, const NameWithUsage &b) {
              return BLI_strcasecmp_natural(a.name.c_str(), b.name.c_str()) <= 0;
            });

  for (const NameWithUsage &attribute : sorted_used_attribute) {
    const StringRefNull attribute_name = attribute.name;
    const geo_log::NamedAttributeUsage usage = attribute.usage;

    /* #uiLayoutRowWithHeading doesn't seem to work in this case. */
    uiLayout *split = uiLayoutSplit(layout, 0.4f, false);

    std::stringstream ss;
    Vector<std::string> usages;
    if ((usage & geo_log::NamedAttributeUsage::Read) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("Read"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Write) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("Write"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Remove) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("Remove"));
    }
    for (const int i : usages.index_range()) {
      ss << usages[i];
      if (i < usages.size() - 1) {
        ss << ", ";
      }
    }

    uiLayout *row = uiLayoutRow(split, false);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
    uiLayoutSetActive(row, false);
    uiItemL(row, ss.str().c_str(), ICON_NONE);

    row = uiLayoutRow(split, false);
    uiItemL(row, attribute_name.c_str(), ICON_NONE);
  }
}

static void panel_register(ARegionType *region_type)
{
  using namespace blender;
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Nodes, panel_draw);
  modifier_subpanel_register(region_type,
                             "output_attributes",
                             N_("Output Attributes"),
                             nullptr,
                             output_attribute_panel_draw,
                             panel_type);
  modifier_subpanel_register(region_type,
                             "internal_dependencies",
                             N_("Internal Dependencies"),
                             nullptr,
                             internal_dependencies_panel_draw,
                             panel_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);

  BLO_write_struct(writer, NodesModifierData, nmd);

  BLO_write_string(writer, nmd->simulation_bake_directory);

  if (nmd->settings.properties != nullptr) {
    Map<IDProperty *, IDPropertyUIDataBool *> boolean_props;
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
  BLO_read_data_address(reader, &nmd->simulation_bake_directory);
  if (nmd->node_group == nullptr) {
    nmd->settings.properties = nullptr;
  }
  else {
    BLO_read_data_address(reader, &nmd->settings.properties);
    IDP_BlendDataRead(reader, &nmd->settings.properties);
  }
  nmd->runtime = MEM_new<NodesModifierRuntime>(__func__);
  nmd->runtime->cache = std::make_shared<bake::ModifierCache>();
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  NodesModifierData *tnmd = reinterpret_cast<NodesModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);

  tnmd->runtime = MEM_new<NodesModifierRuntime>(__func__);

  if (flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) {
    /* Share the simulation cache between the original and evaluated modifier. */
    tnmd->runtime->cache = nmd->runtime->cache;
    /* Keep bake path in the evaluated modifier. */
    tnmd->simulation_bake_directory = nmd->simulation_bake_directory ?
                                          BLI_strdup(nmd->simulation_bake_directory) :
                                          nullptr;
  }
  else {
    tnmd->runtime->cache = std::make_shared<bake::ModifierCache>();
    /* Clear the bake path when duplicating. */
    tnmd->simulation_bake_directory = nullptr;
  }

  if (nmd->settings.properties != nullptr) {
    tnmd->settings.properties = IDP_CopyProperty_ex(nmd->settings.properties, flag);
  }
}

static void free_data(ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->settings.properties != nullptr) {
    IDP_FreeProperty_ex(nmd->settings.properties, false);
    nmd->settings.properties = nullptr;
  }

  MEM_SAFE_FREE(nmd->simulation_bake_directory);
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
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/
    static_cast<ModifierTypeFlag>(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
                                  eModifierTypeFlag_SupportsEditmode |
                                  eModifierTypeFlag_EnableInEditmode |
                                  eModifierTypeFlag_SupportsMapping),
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
};
