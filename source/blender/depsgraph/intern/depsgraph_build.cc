/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"
#include "PIL_time_utildefines.h"

#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_debug.h"

#include "builder/deg_builder.h"
#include "builder/deg_builder_cache.h"
#include "builder/deg_builder_cycle.h"
#include "builder/deg_builder_nodes.h"
#include "builder/deg_builder_relations.h"
#include "builder/deg_builder_transitive.h"

#include "intern/debug/deg_debug.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

#include "intern/depsgraph_registry.h"
#include "intern/depsgraph_relation.h"
#include "intern/depsgraph_type.h"

/* ****************** */
/* External Build API */

static DEG::NodeType deg_build_scene_component_type(eDepsSceneComponentType component)
{
  switch (component) {
    case DEG_SCENE_COMP_PARAMETERS:
      return DEG::NodeType::PARAMETERS;
    case DEG_SCENE_COMP_ANIMATION:
      return DEG::NodeType::ANIMATION;
    case DEG_SCENE_COMP_SEQUENCER:
      return DEG::NodeType::SEQUENCER;
  }
  return DEG::NodeType::UNDEFINED;
}

static DEG::DepsNodeHandle *get_node_handle(DepsNodeHandle *node_handle)
{
  return reinterpret_cast<DEG::DepsNodeHandle *>(node_handle);
}

void DEG_add_scene_relation(DepsNodeHandle *node_handle,
                            Scene *scene,
                            eDepsSceneComponentType component,
                            const char *description)
{
  DEG::NodeType type = deg_build_scene_component_type(component);
  DEG::ComponentKey comp_key(&scene->id, type);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_node_handle_relation(comp_key, deg_node_handle, description);
}

void DEG_add_object_relation(DepsNodeHandle *node_handle,
                             Object *object,
                             eDepsObjectComponentType component,
                             const char *description)
{
  DEG::NodeType type = DEG::nodeTypeFromObjectComponent(component);
  DEG::ComponentKey comp_key(&object->id, type);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_node_handle_relation(comp_key, deg_node_handle, description);
}

void DEG_add_simulation_relation(DepsNodeHandle *node_handle,
                                 Simulation *simulation,
                                 const char *description)
{
  DEG::OperationKey operation_key(
      &simulation->id, DEG::NodeType::SIMULATION, DEG::OperationCode::SIMULATION_EVAL);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_node_handle_relation(operation_key, deg_node_handle, description);
}

void DEG_add_object_cache_relation(DepsNodeHandle *node_handle,
                                   CacheFile *cache_file,
                                   eDepsObjectComponentType component,
                                   const char *description)
{
  DEG::NodeType type = DEG::nodeTypeFromObjectComponent(component);
  DEG::ComponentKey comp_key(&cache_file->id, type);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_node_handle_relation(comp_key, deg_node_handle, description);
}

void DEG_add_bone_relation(DepsNodeHandle *node_handle,
                           Object *object,
                           const char *bone_name,
                           eDepsObjectComponentType component,
                           const char *description)
{
  DEG::NodeType type = DEG::nodeTypeFromObjectComponent(component);
  DEG::ComponentKey comp_key(&object->id, type, bone_name);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_node_handle_relation(comp_key, deg_node_handle, description);
}

void DEG_add_object_pointcache_relation(struct DepsNodeHandle *node_handle,
                                        struct Object *object,
                                        eDepsObjectComponentType component,
                                        const char *description)
{
  DEG::NodeType type = DEG::nodeTypeFromObjectComponent(component);
  DEG::ComponentKey comp_key(&object->id, type);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  DEG::DepsgraphRelationBuilder *relation_builder = deg_node_handle->builder;
  /* Add relation from source to the node handle. */
  relation_builder->add_node_handle_relation(comp_key, deg_node_handle, description);
  /* Node deduct point cache component and connect source to it. */
  ID *id = DEG_get_id_from_handle(node_handle);
  DEG::ComponentKey point_cache_key(id, DEG::NodeType::POINT_CACHE);
  DEG::Relation *rel = relation_builder->add_relation(comp_key, point_cache_key, "Point Cache");
  if (rel != nullptr) {
    rel->flag |= DEG::RELATION_FLAG_FLUSH_USER_EDIT_ONLY;
  }
  else {
    fprintf(stderr, "Error in point cache relation from %s to ^%s.\n", object->id.name, id->name);
  }
}

void DEG_add_generic_id_relation(struct DepsNodeHandle *node_handle,
                                 struct ID *id,
                                 const char *description)
{
  DEG::OperationKey operation_key(
      id, DEG::NodeType::GENERIC_DATABLOCK, DEG::OperationCode::GENERIC_DATABLOCK_UPDATE);
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_node_handle_relation(operation_key, deg_node_handle, description);
}

void DEG_add_modifier_to_transform_relation(struct DepsNodeHandle *node_handle,
                                            const char *description)
{
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_modifier_to_transform_relation(deg_node_handle, description);
}

void DEG_add_special_eval_flag(struct DepsNodeHandle *node_handle, ID *id, uint32_t flag)
{
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_special_eval_flag(id, flag);
}

void DEG_add_customdata_mask(struct DepsNodeHandle *node_handle,
                             struct Object *object,
                             const CustomData_MeshMasks *masks)
{
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  deg_node_handle->builder->add_customdata_mask(object, DEG::DEGCustomDataMeshMasks(masks));
}

struct ID *DEG_get_id_from_handle(struct DepsNodeHandle *node_handle)
{
  DEG::DepsNodeHandle *deg_handle = get_node_handle(node_handle);
  return deg_handle->node->owner->owner->id_orig;
}

struct Depsgraph *DEG_get_graph_from_handle(struct DepsNodeHandle *node_handle)
{
  DEG::DepsNodeHandle *deg_node_handle = get_node_handle(node_handle);
  DEG::DepsgraphRelationBuilder *relation_builder = deg_node_handle->builder;
  return reinterpret_cast<Depsgraph *>(relation_builder->getGraph());
}

/* ******************** */
/* Graph Building API's */

static void graph_build_finalize_common(DEG::Depsgraph *deg_graph, Main *bmain)
{
  /* Detect and solve cycles. */
  DEG::deg_graph_detect_cycles(deg_graph);
  /* Simplify the graph by removing redundant relations (to optimize
   * traversal later). */
  /* TODO: it would be useful to have an option to disable this in cases where
   *       it is causing trouble. */
  if (G.debug_value == 799) {
    DEG::deg_graph_transitive_reduction(deg_graph);
  }
  /* Store pointers to commonly used valuated datablocks. */
  deg_graph->scene_cow = (Scene *)deg_graph->get_cow_id(&deg_graph->scene->id);
  /* Flush visibility layer and re-schedule nodes for update. */
  DEG::deg_graph_build_finalize(bmain, deg_graph);
  DEG_graph_on_visible_update(bmain, reinterpret_cast<::Depsgraph *>(deg_graph), false);
#if 0
  if (!DEG_debug_consistency_check(deg_graph)) {
    printf("Consistency validation failed, ABORTING!\n");
    abort();
  }
#endif
  /* Relations are up to date. */
  deg_graph->need_update = false;
}

/* Build depsgraph for the given scene layer, and dump results in given graph container. */
void DEG_graph_build_from_view_layer(Depsgraph *graph,
                                     Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer)
{
  double start_time = 0.0;
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    start_time = PIL_check_seconds_timer();
  }
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  /* Perform sanity checks. */
  BLI_assert(BLI_findindex(&scene->view_layers, view_layer) != -1);
  BLI_assert(deg_graph->scene == scene);
  BLI_assert(deg_graph->view_layer == view_layer);
  DEG::DepsgraphBuilderCache builder_cache;
  /* Generate all the nodes in the graph first */
  DEG::DepsgraphNodeBuilder node_builder(bmain, deg_graph, &builder_cache);
  node_builder.begin_build();
  node_builder.build_view_layer(scene, view_layer, DEG::DEG_ID_LINKED_DIRECTLY);
  node_builder.end_build();
  /* Hook up relationships between operations - to determine evaluation order. */
  DEG::DepsgraphRelationBuilder relation_builder(bmain, deg_graph, &builder_cache);
  relation_builder.begin_build();
  relation_builder.build_view_layer(scene, view_layer, DEG::DEG_ID_LINKED_DIRECTLY);
  relation_builder.build_copy_on_write_relations();
  relation_builder.build_driver_relations();
  /* Finalize building. */
  graph_build_finalize_common(deg_graph, bmain);
  /* Finish statistics. */
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    printf("Depsgraph built in %f seconds.\n", PIL_check_seconds_timer() - start_time);
  }
}

void DEG_graph_build_for_render_pipeline(Depsgraph *graph,
                                         Main *bmain,
                                         Scene *scene,
                                         ViewLayer *view_layer)
{
  double start_time = 0.0;
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    start_time = PIL_check_seconds_timer();
  }
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  /* Perform sanity checks. */
  BLI_assert(deg_graph->scene == scene);
  deg_graph->is_render_pipeline_depsgraph = true;
  DEG::DepsgraphBuilderCache builder_cache;
  /* Generate all the nodes in the graph first */
  DEG::DepsgraphNodeBuilder node_builder(bmain, deg_graph, &builder_cache);
  node_builder.begin_build();
  node_builder.build_scene_render(scene, view_layer);
  node_builder.end_build();
  /* Hook up relationships between operations - to determine evaluation
   * order. */
  DEG::DepsgraphRelationBuilder relation_builder(bmain, deg_graph, &builder_cache);
  relation_builder.begin_build();
  relation_builder.build_scene_render(scene, view_layer);
  relation_builder.build_copy_on_write_relations();
  relation_builder.build_driver_relations();
  /* Finalize building. */
  graph_build_finalize_common(deg_graph, bmain);
  /* Finish statistics. */
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    printf("Depsgraph built in %f seconds.\n", PIL_check_seconds_timer() - start_time);
  }
}

void DEG_graph_build_for_compositor_preview(
    Depsgraph *graph, Main *bmain, Scene *scene, struct ViewLayer *view_layer, bNodeTree *nodetree)
{
  double start_time = 0.0;
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    start_time = PIL_check_seconds_timer();
  }
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  /* Perform sanity checks. */
  BLI_assert(deg_graph->scene == scene);
  deg_graph->is_render_pipeline_depsgraph = true;
  DEG::DepsgraphBuilderCache builder_cache;
  /* Generate all the nodes in the graph first */
  DEG::DepsgraphNodeBuilder node_builder(bmain, deg_graph, &builder_cache);
  node_builder.begin_build();
  node_builder.build_scene_render(scene, view_layer);
  node_builder.build_nodetree(nodetree);
  node_builder.end_build();
  /* Hook up relationships between operations - to determine evaluation
   * order. */
  DEG::DepsgraphRelationBuilder relation_builder(bmain, deg_graph, &builder_cache);
  relation_builder.begin_build();
  relation_builder.build_scene_render(scene, view_layer);
  relation_builder.build_nodetree(nodetree);
  relation_builder.build_copy_on_write_relations();
  relation_builder.build_driver_relations();
  /* Finalize building. */
  graph_build_finalize_common(deg_graph, bmain);
  /* Finish statistics. */
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    printf("Depsgraph built in %f seconds.\n", PIL_check_seconds_timer() - start_time);
  }
}

/* Optimized builders for dependency graph built from a given set of IDs.
 *
 * General notes:
 *
 * - We pull in all bases if their objects are in the set of IDs. This allows to have proper
 *   visibility and other flags assigned to the objects.
 *   All other bases (the ones which points to object which is outside of the set of IDs) are
 *   completely ignored.
 *
 * - Proxy groups pointing to objects which are outside of the IDs set are also ignored.
 *   This way we avoid high-poly character body pulled into the dependency graph when it's coming
 *   from a library into an animation file and the dependency graph constructed for a proxy rig. */

namespace DEG {
namespace {

class DepsgraphFromIDsFilter {
 public:
  DepsgraphFromIDsFilter(ID **ids, const int num_ids)
  {
    for (int i = 0; i < num_ids; ++i) {
      ids_.add(ids[i]);
    }
  }

  bool contains(ID *id)
  {
    return ids_.contains(id);
  }

 protected:
  Set<ID *> ids_;
};

class DepsgraphFromIDsNodeBuilder : public DepsgraphNodeBuilder {
 public:
  DepsgraphFromIDsNodeBuilder(
      Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache, ID **ids, const int num_ids)
      : DepsgraphNodeBuilder(bmain, graph, cache), filter_(ids, num_ids)
  {
  }

  virtual bool need_pull_base_into_graph(Base *base) override
  {
    if (!filter_.contains(&base->object->id)) {
      return false;
    }
    return DepsgraphNodeBuilder::need_pull_base_into_graph(base);
  }

  virtual void build_object_proxy_group(Object *object, bool is_visible) override
  {
    if (object->proxy_group == nullptr) {
      return;
    }
    if (!filter_.contains(&object->proxy_group->id)) {
      return;
    }
    DepsgraphNodeBuilder::build_object_proxy_group(object, is_visible);
  }

 protected:
  DepsgraphFromIDsFilter filter_;
};

class DepsgraphFromIDsRelationBuilder : public DepsgraphRelationBuilder {
 public:
  DepsgraphFromIDsRelationBuilder(
      Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache, ID **ids, const int num_ids)
      : DepsgraphRelationBuilder(bmain, graph, cache), filter_(ids, num_ids)
  {
  }

  virtual bool need_pull_base_into_graph(Base *base) override
  {
    if (!filter_.contains(&base->object->id)) {
      return false;
    }
    return DepsgraphRelationBuilder::need_pull_base_into_graph(base);
  }

  virtual void build_object_proxy_group(Object *object) override
  {
    if (object->proxy_group == nullptr) {
      return;
    }
    if (!filter_.contains(&object->proxy_group->id)) {
      return;
    }
    DepsgraphRelationBuilder::build_object_proxy_group(object);
  }

 protected:
  DepsgraphFromIDsFilter filter_;
};

}  // namespace
}  // namespace DEG

void DEG_graph_build_from_ids(Depsgraph *graph,
                              Main *bmain,
                              Scene *scene,
                              ViewLayer *view_layer,
                              ID **ids,
                              const int num_ids)
{
  double start_time = 0.0;
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    start_time = PIL_check_seconds_timer();
  }
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  /* Perform sanity checks. */
  BLI_assert(BLI_findindex(&scene->view_layers, view_layer) != -1);
  BLI_assert(deg_graph->scene == scene);
  BLI_assert(deg_graph->view_layer == view_layer);
  DEG::DepsgraphBuilderCache builder_cache;
  /* Generate all the nodes in the graph first */
  DEG::DepsgraphFromIDsNodeBuilder node_builder(bmain, deg_graph, &builder_cache, ids, num_ids);
  node_builder.begin_build();
  node_builder.build_view_layer(scene, view_layer, DEG::DEG_ID_LINKED_DIRECTLY);
  for (int i = 0; i < num_ids; ++i) {
    node_builder.build_id(ids[i]);
  }
  node_builder.end_build();
  /* Hook up relationships between operations - to determine evaluation order. */
  DEG::DepsgraphFromIDsRelationBuilder relation_builder(
      bmain, deg_graph, &builder_cache, ids, num_ids);
  relation_builder.begin_build();
  relation_builder.build_view_layer(scene, view_layer, DEG::DEG_ID_LINKED_DIRECTLY);
  for (int i = 0; i < num_ids; ++i) {
    relation_builder.build_id(ids[i]);
  }
  relation_builder.build_copy_on_write_relations();
  relation_builder.build_driver_relations();
  /* Finalize building. */
  graph_build_finalize_common(deg_graph, bmain);
  /* Finish statistics. */
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    printf("Depsgraph built in %f seconds.\n", PIL_check_seconds_timer() - start_time);
  }
}

/* Tag graph relations for update. */
void DEG_graph_tag_relations_update(Depsgraph *graph)
{
  DEG_DEBUG_PRINTF(graph, TAG, "%s: Tagging relations for update.\n", __func__);
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  deg_graph->need_update = true;
  /* NOTE: When relations are updated, it's quite possible that
   * we've got new bases in the scene. This means, we need to
   * re-create flat array of bases in view layer.
   *
   * TODO(sergey): Try to make it so we don't flush updates
   * to the whole depsgraph. */
  DEG::IDNode *id_node = deg_graph->find_id_node(&deg_graph->scene->id);
  if (id_node != nullptr) {
    id_node->tag_update(deg_graph, DEG::DEG_UPDATE_SOURCE_RELATIONS);
  }
}

/* Create or update relations in the specified graph. */
void DEG_graph_relations_update(Depsgraph *graph, Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  DEG::Depsgraph *deg_graph = (DEG::Depsgraph *)graph;
  if (!deg_graph->need_update) {
    /* Graph is up to date, nothing to do. */
    return;
  }
  DEG_graph_build_from_view_layer(graph, bmain, scene, view_layer);
}

/* Tag all relations for update. */
void DEG_relations_tag_update(Main *bmain)
{
  DEG_GLOBAL_DEBUG_PRINTF(TAG, "%s: Tagging relations for update.\n", __func__);
  for (DEG::Depsgraph *depsgraph : DEG::get_all_registered_graphs(bmain)) {
    DEG_graph_tag_relations_update(reinterpret_cast<Depsgraph *>(depsgraph));
  }
}
