/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* Follow eMaterial_ThicknessMode. */
enum class ThicknessMode : bool { Sphere = false, Slab = true };

struct ThicknessIsect {
  /* Normal at the intersection point on the sphere. */
  float3 hit_N;
  /* Position of the intersection point on the sphere. */
  float3 hit_P;
};

/* Storage for object thickness, which packs both thickness value and
 * an associated model mode (slab, sphere) through the sign bit. */
struct Thickness {
  float data;

  static Thickness from(float value, ThicknessMode mode)
  {
    return {.data = (mode == ThicknessMode::Sphere) ? value : -value};
  }

  static Thickness zero()
  {
    return {.data = 0.0f};
  }

  float value() const
  {
    return abs(data);
  }

  bool is_sphere() const
  {
    return data > 0.0f;
  }

  ThicknessMode mode() const
  {
    return is_sphere() ? ThicknessMode::Sphere : ThicknessMode::Slab;
  }

  ThicknessIsect shape_intersect(float3 N, float3 L) const
  {
    if (is_sphere()) {
      return sphere_intersect(value(), N, L);
    }
    return plane_intersect(value(), N, L);
  }

 private:
  /**
   * Model sub-surface ray interaction with a sphere of the given diameter tangent to the shading
   * point. This allows to model 2 refraction events quite cheaply.
   * Assumes N and L are normalized.
   * Everything is relative to the entrance shading point.
   */
  ThicknessIsect sphere_intersect(float diameter, float3 N, float3 L) const
  {
    ThicknessIsect isect;
    const float cos_alpha = dot(L, -N);
    isect.hit_N = normalize(N + L * (cos_alpha * 2.0f));
    isect.hit_P = L * (cos_alpha * diameter);
    return isect;
  }

  /**
   * Model sub-surface ray interaction with an infinite plane of the given diameter tangent to the
   * shading point. This allows to model 2 refraction events quite cheaply.
   * Assumes N and L are normalized.
   * Everything is relative to the entrance shading point.
   */
  ThicknessIsect plane_intersect(float plane_distance, float3 N, float3 L) const
  {
    ThicknessIsect isect;
    const float distance_from_shading_plane = dot(L, -N);
    isect.hit_N = -N;
    isect.hit_P = L * (plane_distance / distance_from_shading_plane);
    return isect;
  }
};
