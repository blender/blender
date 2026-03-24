/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

enum class ThicknessMode : bool { Slab = false, Sphere = true };

/* Storage for object thickness, which packs both thickness value and
 * an associated model mode (slab, sphere) through the sign bit. */
class Thickness {
  float data;

 public:
  static Thickness from(float value, ThicknessMode mode)
  {
    Thickness thickness;
    thickness.data = (mode == ThicknessMode::Sphere) ? value : -value;
    return thickness;
  }

  static Thickness zero()
  {
    Thickness thickness;
    thickness.data = 0.0f;
    return thickness;
  }

  float value() const
  {
    return abs(data);
  }

  ThicknessMode mode() const
  {
    return data > 0.0 ? ThicknessMode::Sphere : ThicknessMode::Slab;
  }
};

struct ThicknessIsect {
  /* Normal at the intersection point on the sphere. */
  float3 hit_N;
  /* Position of the intersection point on the sphere. */
  float3 hit_P;
};

/**
 * Model sub-surface ray interaction with a sphere of the given diameter tangent to the shading
 * point. This allows to model 2 refraction events quite cheaply.
 * Assumes N and L are normalized.
 * Everything is relative to the entrance shading point.
 */
ThicknessIsect thickness_sphere_intersect(float diameter, float3 N, float3 L)
{
  ThicknessIsect isect;
  float cos_alpha = dot(L, -N);
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
ThicknessIsect thickness_plane_intersect(float plane_distance, float3 N, float3 L)
{
  ThicknessIsect isect;
  float distance_from_shading_plane = dot(L, -N);
  isect.hit_N = -N;
  isect.hit_P = L * (plane_distance / distance_from_shading_plane);
  return isect;
}

ThicknessIsect thickness_shape_intersect(Thickness thickness, float3 N, float3 L)
{
  if (thickness.mode() == ThicknessMode::Sphere) {
    return thickness_sphere_intersect(thickness.value(), N, L);
  }
  return thickness_plane_intersect(thickness.value(), N, L);
}
