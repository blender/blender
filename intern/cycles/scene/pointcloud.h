/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/geometry.h"

CCL_NAMESPACE_BEGIN

class PointCloud : public Geometry {
 public:
  NODE_DECLARE

  enum RenderAs {
    RENDER_AS_POINTS,
    RENDER_AS_GSPLATS,
  };

  /* Default values for Gaussian splat attributes, matching defaults in Blender EEVEE.
   * Default rotation is an identity quaternion. */
  static inline const float4 DEFAULT_GSPLAT_RADIANCE_BASE = make_float4(0.0f, 0.0f, 0.0f, 0.5f);
  static inline const float3 DEFAULT_GSPLAT_SCALE = make_float3(1e-3f, 1e-3f, 1e-3f);

  /* PointCloud Point */
  struct Point {
    int index;

    void bounds_grow(const packed_float3 *points, const float *radius, BoundBox &bounds) const;
    void bounds_grow(const packed_float3 *points,
                     const float *radius,
                     const Transform &aligned_space,
                     BoundBox &bounds) const;
    void bounds_grow(const float4 &point, BoundBox &bounds) const;

    float4 motion_key(const float *radius,
                      const Attribute *attr_P,
                      const Attribute *attr_R,
                      const size_t num_steps,
                      const float time,
                      size_t p) const;
    float4 point_for_step(const float *radius,
                          const Attribute *attr_P,
                          const Attribute *attr_R,
                          const size_t step,
                          size_t p) const;
  };

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
    const Attribute *attr = attributes.find(ATTR_STD_POSITION);
    return attr ? attr->size : 0;
  }

  size_t num_attributes() const
  {
    return 1;
  }

  /* UDIM */
  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;

  PrimitiveType primitive_type() const override;

  /* BVH */
  void pack(Scene *scene, uint *packed_shader);

  /* Recalculate point radius to bound Gaussian splats. */
  void update_gsplat_radii();

  /* Create attributes that are required but missing for rendering the point cloud as a 3D
   * gaussian splat. This includes radiance base, scale, and rotation. The attributes are filled
   * with default values:
   * - Radiance base is a half-opaque, neutral grey color.
   * - Scale is a small value, mainly obtained empirically with the goal to provide a way to get
   *   an idea of the shape of the 3D Gaussian splat without requiring too much render time.
   * - Rotation is defaulted to identity quaternion. */
  void create_missing_gsplat_attributes();

  NODE_SOCKET_API(RenderAs, render_as)

 private:
  void add_builtin_attributes();

  friend class BVH2;
  friend class BVHBuild;
  friend class BVHSpatialSplit;
  friend class DiagSplit;
  friend class EdgeDice;
  friend class GeometryManager;
  friend class ObjectManager;
};

CCL_NAMESPACE_END
