/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"

#include "BKE_geometry_set.hh"

namespace blender::bke {

/**
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
GeometrySet object_get_evaluated_geometry_set(const Object &object);

/**
 * Used to keep track of a group of instances using the same geometry data.
 */
struct GeometryInstanceGroup {
  /**
   * The geometry set instanced on each of the transforms. The components are not necessarily
   * owned here. For example, they may be owned by the instanced object. This cannot be a
   * reference because not all instanced data will necessarily have a #geometry_set_eval.
   */
  GeometrySet geometry_set;

  /**
   * As an optimization to avoid copying, the same geometry set can be associated with multiple
   * instances. Each instance is stored as a transform matrix here. Again, these must be owned
   * because they may be transformed from the original data. TODO: Validate that last statement.
   */
  Vector<float4x4> transforms;
};

/**
 * Return flattened vector of the geometry component's recursive instances. I.e. all collection
 * instances and object instances will be expanded into the instances of their geometry components.
 * Even the instances in those geometry components' will be included.
 *
 * \note For convenience (to avoid duplication in the caller), the returned vector also contains
 * the argument geometry set.
 *
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
void geometry_set_gather_instances(const GeometrySet &geometry_set,
                                   Vector<GeometryInstanceGroup> &r_instance_groups);

}  // namespace blender::bke
