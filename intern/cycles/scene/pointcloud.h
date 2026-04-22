/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/geometry.h"

CCL_NAMESPACE_BEGIN

class PointCloud : public Geometry {
 public:
  NODE_DECLARE

  /* PointCloud Point */
  struct Point {
    int index;

    void bounds_grow(const packed_float3 *points, const float *radius, BoundBox &bounds) const;
    void bounds_grow(const packed_float3 *points,
                     const float *radius,
                     const Transform &aligned_space,
                     BoundBox &bounds) const;
    void bounds_grow(const float4 &point, BoundBox &bounds) const;

    float4 motion_key(const packed_float3 *points,
                      const float *radius,
                      const float4 *point_steps,
                      const size_t num_points,
                      const size_t num_steps,
                      const float time,
                      size_t p) const;
    float4 point_for_step(const packed_float3 *points,
                          const float *radius,
                          const float4 *point_steps,
                          const size_t num_points,
                          const size_t num_steps,
                          const size_t step,
                          size_t p) const;
  };

  NODE_SOCKET_API_ARRAY(array<packed_float3>, points)
  array<float> radius;
  NODE_SOCKET_API_ARRAY(array<int>, shader)

  /* Constructor/Destructor */
  PointCloud();
  ~PointCloud() override;

  /* Geometry */
  void clear_non_sockets();
  void clear(const bool preserve_shaders = false) override;

  void resize(const int numpoints);

  void copy_center_to_motion_step(const int motion_step);

  void compute_bounds() override;
  void apply_transform(const Transform &tfm, const bool apply_to_motion) override;

  /* Points */
  Point get_point(const int i) const
  {
    Point point = {i};
    return point;
  }

  size_t num_points() const
  {
    return points.size();
  }

  const packed_float3 *get_position() const
  {
    return get_points().data();
  }

  packed_float3 *get_position_for_write()
  {
    tag_points_modified();
    return get_points().data();
  }

  const float *get_radius() const
  {
    return radius.data();
  }

  float *get_radius_for_write()
  {
    tag_radius_modified();
    return radius.data();
  }

  size_t num_attributes() const
  {
    return 1;
  }

  /* UDIM */
  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;

  PrimitiveType primitive_type() const override;

  /* BVH */
  void pack(Scene *scene, float4 *packed_points, uint *packed_shader);

 private:
  friend class BVH2;
  friend class BVHBuild;
  friend class BVHSpatialSplit;
  friend class DiagSplit;
  friend class EdgeDice;
  friend class GeometryManager;
  friend class ObjectManager;
};

CCL_NAMESPACE_END
