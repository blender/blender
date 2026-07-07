/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/geometry.h"

CCL_NAMESPACE_BEGIN

struct KernelCurveSegment;

class Hair : public Geometry {
 public:
  NODE_DECLARE

  /* Hair Curve */
  struct Curve {
    int first_key;
    int num_keys;

    int num_segments() const
    {
      return num_keys - 1;
    }

    void bounds_grow(const int k,
                     const float3 *curve_keys,
                     const float *curve_radius,
                     BoundBox &bounds) const;
    void bounds_grow(const int k, const float4 *keys, BoundBox &bounds) const;
    void bounds_grow(const float4 keys[4], BoundBox &bounds) const;
    void bounds_grow(const float3 keys[4], BoundBox &bounds) const;
    void bounds_grow(const int k,
                     const float3 *curve_keys,
                     const float *curve_radius,
                     const Transform &aligned_space,
                     BoundBox &bounds) const;

    void motion_keys(const float3 *curve_keys,
                     const float *curve_radius,
                     const float4 *key_steps,
                     const size_t num_curve_keys,
                     const size_t num_steps,
                     const float time,
                     size_t k0,
                     size_t k1,
                     float4 r_keys[2]) const;
    void cardinal_motion_keys(const float3 *curve_keys,
                              const float *curve_radius,
                              const float4 *key_steps,
                              const size_t num_curve_keys,
                              const size_t num_steps,
                              const float time,
                              size_t k0,
                              size_t k1,
                              size_t k2,
                              size_t k3,
                              float4 r_keys[4]) const;

    void keys_for_step(const float3 *curve_keys,
                       const float *curve_radius,
                       const float4 *key_steps,
                       const size_t num_curve_keys,
                       const size_t num_steps,
                       const size_t step,
                       size_t k0,
                       size_t k1,
                       float4 r_keys[2]) const;
    void cardinal_keys_for_step(const float3 *curve_keys,
                                const float *curve_radius,
                                const float4 *key_steps,
                                const size_t num_curve_keys,
                                const size_t num_steps,
                                const size_t step,
                                size_t k0,
                                size_t k1,
                                size_t k2,
                                size_t k3,
                                float4 r_keys[4]) const;
  };

  NODE_SOCKET_API_ARRAY(array<float3>, curve_keys)
  NODE_SOCKET_API_ARRAY(array<float>, curve_radius)
  NODE_SOCKET_API_ARRAY(array<int>, curve_first_key)
  NODE_SOCKET_API_ARRAY(array<int>, curve_shader)

  /* BVH */
  size_t curve_key_offset;
  size_t curve_segment_offset;
  CurveShapeType curve_shape;

  /* Constructor/Destructor */
  Hair();
  ~Hair() override;

  /* Geometry */
  void clear(bool preserve_shaders = false) override;

  void resize_curves(const int numcurves, const int numkeys);
  void reserve_curves(const int numcurves, const int numkeys);
  void add_curve_key(const float3 co, const float radius);
  void add_curve(const int first_key, const int shader);

  void copy_center_to_motion_step(const int motion_step);

  void compute_bounds() override;
  void apply_transform(const Transform &tfm, const bool apply_to_motion) override;

  /* Curves */
  Curve get_curve(const size_t i) const
  {
    const int first = curve_first_key[i];
    const int next_first = (i + 1 < curve_first_key.size()) ? curve_first_key[i + 1] :
                                                              curve_keys.size();

    Curve curve = {first, next_first - first};
    return curve;
  }

  size_t num_keys() const
  {
    return curve_keys.size();
  }

  size_t num_curves() const
  {
    return curve_first_key.size();
  }

  size_t num_segments() const
  {
    return curve_keys.size() - curve_first_key.size();
  }

  bool is_traceable() const
  {
    return num_segments() > 0;
  }

  /* UDIM */
  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;

  /* BVH */
  void pack_curves(Scene *scene,
                   float4 *curve_key_co,
                   KernelCurve *curve,
                   KernelCurveSegment *curve_segments);

  PrimitiveType primitive_type() const override;

  /* Attributes */
  bool need_shadow_transparency() const;
  bool need_update_shadow_transparency() const;
  bool update_shadow_transparency(Device *device, Scene *scene, Progress &progress);
};

CCL_NAMESPACE_END
