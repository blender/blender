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
}

void GeometryNodesEvalDependencies::merge(const GeometryNodesEvalDependencies &other)
{
  for (ID *id : other.ids.values()) {
    this->add_generic_id(id);
  }
  for (const auto &&item : other.objects_info.items()) {
    ID *id = this->ids.lookup(item.key);
    BLI_assert(GS(id->name) == ID_OB);
    this->add_object(reinterpret_cast<Object *>(id), item.value);
  }
  this->needs_own_transform |= other.needs_own_transform;
  this->needs_active_camera |= other.needs_active_camera;
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

static bool node_needs_own_transform(const bNode &node)
{
  if (node.is_muted()) {
    return false;
  }
  switch (node.type_legacy) {
    case GEO_NODE_COLLECTION_INFO: {
      const NodeGeometryCollectionInfo &storage = *static_cast<const NodeGeometryCollectionInfo *>(
          node.storage);
      return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
    }
    case GEO_NODE_OBJECT_INFO: {
      const NodeGeometryObjectInfo &storage = *static_cast<const NodeGeometryObjectInfo *>(
          node.storage);
      return storage.transform_space == GEO_NODE_TRANSFORM_SPACE_RELATIVE;
    }
    case GEO_NODE_DEFORM_CURVES_ON_SURFACE:
    case GEO_NODE_SELF_OBJECT: {
      return true;
    }
    default: {
      return false;
    }
  }
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
  deps.needs_active_camera |= !ntree.nodes_by_type("GeometryNodeInputActiveCamera").is_empty();
  deps.time_dependent |= !ntree.nodes_by_type("GeometryNodeSimulationInput").is_empty() ||
                         !ntree.nodes_by_type("GeometryNodeInputSceneTime").is_empty();
  for (const bNode *node : ntree.group_nodes()) {
    if (!node->id) {
      continue;
    }
    const bNodeTree &group = *reinterpret_cast<const bNodeTree *>(node->id);
    if (const GeometryNodesEvalDependencies *group_deps = get_group_deps(group)) {
      deps.merge(*group_deps);
    }
  }
  for (const bNode *node : ntree.all_nodes()) {
    deps.needs_own_transform |= node_needs_own_transform(*node);
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
