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

    BLI_STRUCT_EQUALITY_OPERATORS_2(ObjectDependencyInfo, transform, geometry);
  };
  static constexpr ObjectDependencyInfo all_object_deps{true, true};

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
   * Add all the given given dependencies to this one.
   */
  void merge(const GeometryNodesEvalDependencies &other);

  BLI_STRUCT_EQUALITY_OPERATORS_5(GeometryNodesEvalDependencies,
                                  ids,
                                  objects_info,
                                  needs_own_transform,
                                  needs_active_camera,
                                  time_dependent);
};

/**
 * Find all evaluation dependencies for the given node tree.
 * NOTE: It's assumed that all (indirectly) used node groups are updated already.
 */
void gather_geometry_nodes_eval_dependencies(bNodeTree &ntree,
                                             GeometryNodesEvalDependencies &deps);

}  // namespace blender::nodes
