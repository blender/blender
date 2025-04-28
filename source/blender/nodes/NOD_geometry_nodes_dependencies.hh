/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_struct_equality_utils.hh"

struct ID;
struct Object;
struct bNodeTree;

namespace blender::nodes {

/**
 * Gathers dependencies that the node tree requires before it can be evaluated.
 */
struct GeometryNodesEvalDependencies {
  /**
   * Stores additional dependency information for objects. It can be more efficient to only depend
   * on an object partially.
   */
  struct ObjectDependencyInfo {
    bool transform = false;
    bool geometry = false;
    bool camera_parameters = false;

    BLI_STRUCT_EQUALITY_OPERATORS_3(ObjectDependencyInfo, transform, geometry, camera_parameters);
  };
  static constexpr ObjectDependencyInfo all_object_deps{true, true, true};

  /**
   * Maps `session_uid` to the corresponding data-block.
   * The data-block pointer is not used as key in this map, so that it can be modified in
   * #node_foreach_id.
   */
  Map<uint32_t, ID *> ids;

  /** Additional information for object dependencies. */
  Map<uint32_t, ObjectDependencyInfo> objects_info;

  bool needs_own_transform = false;
  bool needs_active_camera = false;
  bool needs_scene_render_params = false;
  bool time_dependent = false;

  /**
   * Adds a generic data-block dependency. Note that this does not add a dependency to e.g. the
   * transform or geometry of an object. If that is desired, use #add_object or
   * #add_generic_id_full instead.
   */
  void add_generic_id(ID *id);

  /**
   * Adds a data-block as dependency. For objects, it also adds a dependency to the transform and
   * geometry.
   */
  void add_generic_id_full(ID *id);

  /**
   * Add an object as dependency. It's customizable whether e.g. the transform and/or geometry is
   * required.
   */
  void add_object(Object *object, const ObjectDependencyInfo &object_deps = all_object_deps);

  /**
   * Add all the given dependencies to this one.
   */
  void merge(const GeometryNodesEvalDependencies &other);

  BLI_STRUCT_EQUALITY_OPERATORS_6(GeometryNodesEvalDependencies,
                                  ids,
                                  objects_info,
                                  needs_own_transform,
                                  needs_active_camera,
                                  needs_scene_render_params,
                                  time_dependent);
};

/**
 * Finds all evaluation dependencies for the given node. This does not include dependencies that
 * are passed into the node group. It also may not contain all data-blocks referenced by the node
 * tree if some of them can statically be detected to not be used by the evaluation.
 */
GeometryNodesEvalDependencies gather_geometry_nodes_eval_dependencies_recursive(
    const bNodeTree &ntree);
/**
 * Same as above, but assumes that dependencies are already cached on the referenced node groups.
 */
GeometryNodesEvalDependencies gather_geometry_nodes_eval_dependencies_with_cache(
    const bNodeTree &ntree);

}  // namespace blender::nodes
