/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_dependencies.hh"

#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

namespace blender::nodes {

void GeometryNodesEvalDependencies::add_generic_id(ID *id)
{
  if (!id) {
    return;
  }
  this->ids.add(id->session_uid, id);
}

void GeometryNodesEvalDependencies::add_generic_id_full(ID *id)
{
  if (!id) {
    return;
  }
  if (GS(id->name) == ID_OB) {
    this->add_object(reinterpret_cast<Object *>(id));
  }
  else {
    this->add_generic_id(id);
  }
}

void GeometryNodesEvalDependencies::add_object(Object *object,
                                               const ObjectDependencyInfo &object_deps)
{
  if (!object) {
    return;
  }
  this->add_generic_id(&object->id);
  ObjectDependencyInfo &deps = this->objects_info.lookup_or_add(object->id.session_uid,
                                                                object_deps);
  deps.geometry |= object_deps.geometry;
  deps.transform |= object_deps.transform;
  deps.camera_parameters |= object_deps.camera_parameters;
}

void GeometryNodesEvalDependencies::merge(const GeometryNodesEvalDependencies &other)
{
  for (ID *id : other.ids.values()) {
    this->add_generic_id(id);
  }
  for (const auto &&item : other.objects_info.items()) {
    ID *id = other.ids.lookup(item.key);
    BLI_assert(GS(id->name) == ID_OB);
    this->add_object(reinterpret_cast<Object *>(id), item.value);
  }
  this->needs_own_transform |= other.needs_own_transform;
  this->needs_active_camera |= other.needs_active_camera;
  this->needs_scene_render_params |= other.needs_scene_render_params;
  this->time_dependent |= other.time_dependent;
}

static void add_eval_dependencies_from_socket(const bNodeSocket &socket,
                                              GeometryNodesEvalDependencies &deps)
{
  if (socket.is_input()) {
    if (socket.is_logically_linked()) {
      /* The input value is unused. */
      return;
    }
  }
  switch (socket.type) {
    case SOCK_OBJECT: {
      if (Object *object = static_cast<bNodeSocketValueObject *>(socket.default_value)->value) {
        deps.add_object(object);
      }
      break;
    }
    case SOCK_COLLECTION: {
      if (Collection *collection =
              static_cast<bNodeSocketValueCollection *>(socket.default_value)->value)
      {
        deps.add_generic_id(reinterpret_cast<ID *>(collection));
      }
      break;
    }
    case SOCK_MATERIAL: {
      if (Material *material =
              static_cast<bNodeSocketValueMaterial *>(socket.default_value)->value)
      {
        deps.add_generic_id(reinterpret_cast<ID *>(material));
      }
      break;
    }
    case SOCK_TEXTURE: {
      if (Tex *texture = static_cast<bNodeSocketValueTexture *>(socket.default_value)->value) {
        deps.add_generic_id(reinterpret_cast<ID *>(texture));
      }
      break;
    }
    case SOCK_IMAGE: {
      if (Image *image = static_cast<bNodeSocketValueImage *>(socket.default_value)->value) {
        deps.add_generic_id(reinterpret_cast<ID *>(image));
      }
      break;
    }
  }
}

static void add_eval_dependencies_from_node_data(const bNodeTree &tree,
                                                 GeometryNodesEvalDependencies &deps)
{
  for (const bNode *node : tree.nodes_by_type("GeometryNodeInputObject")) {
    if (node->is_muted()) {
      continue;
    }
    deps.add_object(reinterpret_cast<Object *>(node->id));
  }
  for (const bNode *node : tree.nodes_by_type("GeometryNodeInputCollection")) {
    if (node->is_muted()) {
      continue;
    }
    deps.add_generic_id(node->id);
  }
}

static bool has_enabled_nodes_of_type(const bNodeTree &tree,
                                      const blender::StringRefNull type_idname)
{
  for (const bNode *node : tree.nodes_by_type(type_idname)) {
    if (!node->is_muted()) {
      return true;
    }
  }
  return false;
}

static void add_own_transform_dependencies(const bNodeTree &tree,
                                           GeometryNodesEvalDependencies &deps)
{
  bool needs_own_transform = false;

  needs_own_transform |= has_enabled_nodes_of_type(tree, "GeometryNodeSelfObject");
  needs_own_transform |= has_enabled_nodes_of_type(tree, "GeometryNodeDeformCurvesOnSurface");

  for (const bNode *node : tree.nodes_by_type("GeometryNodeCollectionInfo")) {
    if (node->is_muted()) {
      continue;
    }
    const NodeGeometryCollectionInfo &storage = *static_cast<const NodeGeometryCollectionInfo *>(
        node->storage);
    needs_own_transform |= storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  for (const bNode *node : tree.nodes_by_type("GeometryNodeObjectInfo")) {
    if (node->is_muted()) {
      continue;
    }
    const NodeGeometryObjectInfo &storage = *static_cast<const NodeGeometryObjectInfo *>(
        node->storage);
    needs_own_transform |= storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
  }

  deps.needs_own_transform |= needs_own_transform;
}

static bool needs_scene_render_params(const bNodeTree &ntree)
{
  for (const bNode *node : ntree.nodes_by_type("GeometryNodeCameraInfo")) {
    if (node->is_muted()) {
      continue;
    }
    const bNodeSocket &projection_matrix_socket = node->output_by_identifier("Projection Matrix");
    if (projection_matrix_socket.is_logically_linked()) {
      return true;
    }
  }
  return false;
}

static void gather_geometry_nodes_eval_dependencies(
    const bNodeTree &ntree,
    GeometryNodesEvalDependencies &deps,
    FunctionRef<const GeometryNodesEvalDependencies *(const bNodeTree &group)> get_group_deps)
{
  ntree.ensure_topology_cache();
  for (const bNodeSocket *socket : ntree.all_sockets()) {
    add_eval_dependencies_from_socket(*socket, deps);
  }
  deps.needs_active_camera |= has_enabled_nodes_of_type(ntree, "GeometryNodeInputActiveCamera");
  deps.needs_scene_render_params |= needs_scene_render_params(ntree);
  deps.time_dependent |= has_enabled_nodes_of_type(ntree, "GeometryNodeSimulationInput") ||
                         has_enabled_nodes_of_type(ntree, "GeometryNodeInputSceneTime");

  add_eval_dependencies_from_node_data(ntree, deps);
  add_own_transform_dependencies(ntree, deps);

  for (const bNode *node : ntree.group_nodes()) {
    if (!node->id) {
      continue;
    }
    const bNodeTree &group = *reinterpret_cast<const bNodeTree *>(node->id);
    if (const GeometryNodesEvalDependencies *group_deps = get_group_deps(group)) {
      deps.merge(*group_deps);
    }
  }
}

GeometryNodesEvalDependencies gather_geometry_nodes_eval_dependencies_with_cache(
    const bNodeTree &ntree)
{
  GeometryNodesEvalDependencies deps;
  gather_geometry_nodes_eval_dependencies(ntree, deps, [](const bNodeTree &group) {
    return group.runtime->geometry_nodes_eval_dependencies.get();
  });
  return deps;
}

static void gather_geometry_nodes_eval_dependencies_recursive_impl(
    const bNodeTree &ntree, Map<const bNodeTree *, GeometryNodesEvalDependencies> &deps_by_tree)
{
  if (deps_by_tree.contains(&ntree)) {
    return;
  }
  GeometryNodesEvalDependencies new_deps;
  gather_geometry_nodes_eval_dependencies(ntree, new_deps, [&](const bNodeTree &group) {
    gather_geometry_nodes_eval_dependencies_recursive_impl(group, deps_by_tree);
    return &deps_by_tree.lookup(&group);
  });
  deps_by_tree.add(&ntree, std::move(new_deps));
}

GeometryNodesEvalDependencies gather_geometry_nodes_eval_dependencies_recursive(
    const bNodeTree &ntree)
{
  Map<const bNodeTree *, GeometryNodesEvalDependencies> deps_by_tree;
  gather_geometry_nodes_eval_dependencies_recursive_impl(ntree, deps_by_tree);
  return deps_by_tree.lookup(&ntree);
}

}  // namespace blender::nodes
