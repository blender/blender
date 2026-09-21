/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_math_geom_lib.glsl"
#include "eevee_light_shared.hh"
#include "eevee_uniform_shared.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

/* Attenuation cutoff needs to be the same in the shadow loop and the light eval loop. */
#define LIGHT_ATTENUATION_THRESHOLD 1e-6f

/* ---------------------------------------------------------------------- */
/** \name Light Functions
 * \{ */

struct LightVector {
  /* Unit world space light vector, from the shading point to the light center. */
  float3 L;
  /* Distance from the shading point to the light center. */
  float dist;

  /* Construct a vector to the light center. */
  static LightVector get(LightData light, const bool is_directional, float3 P)
  {
    if (is_directional) {
      return LightVector{.L = light.sun().direction, .dist = 1.0f};
    }
    LightVector lv;
    lv.L = normalize_and_get_length(light.position() - P, lv.dist);
    return lv;
  }

  /* Construct a vector to the closest point in the light shape. */
  static LightVector get_shape_closest(LightData light, const bool is_directional, float3 P)
  {
    if (!is_directional && is_area_light(light.type)) {
      LightAreaData area = light.area();

      float3 lP = transform_point_inversed(light.object_to_world, P);
      float2 ls_closest_point = lP.xy;
      if (light.type == LIGHT_ELLIPSE) {
        ls_closest_point /= max(1.0f, length(ls_closest_point / area.size));
      }
      else {
        ls_closest_point = clamp(ls_closest_point, -area.size, area.size);
      }
      float3 ws_closest_point = transform_point(light.object_to_world,
                                                float3(ls_closest_point, 0.0f));

      LightVector lv;
      lv.L = normalize_and_get_length(ws_closest_point - P, lv.dist);
      return lv;
    }

    /* TODO(@fclem): other light shape? */
    return LightVector::get(light, is_directional, P);
  }
};

float3 light_world_to_local_direction(LightData light, float3 L)
{
  return transform_direction_transposed(light.object_to_world, L);
}
float3 light_world_to_local_point(LightData light, float3 point)
{
  return transform_point_inversed(light.object_to_world, point);
}

/* From Frostbite PBR Course
 * Distance based attenuation
 * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf */
float light_influence_attenuation(float dist, float inv_sqr_influence)
{
  float factor = square(dist) * inv_sqr_influence;
  float fac = saturate(1.0f - square(factor));
  return square(fac);
}

float light_spot_attenuation(LightData light, float3 L)
{
  LightSpotData spot = light.spot();
  float3 lL = light_world_to_local_direction(light, L);
  float ellipse = inversesqrt(1.0f + length_squared(lL.xy * spot.spot_size_inv / lL.z));
  float spotmask = smoothstep(0.0f, 1.0f, ellipse * spot.spot_mul + spot.spot_bias);
  return (lL.z > 0.0f) ? spotmask : 0.0f;
}

float light_attenuation_common(LightData light, const bool is_directional, float3 L)
{
  if (is_directional) {
    return 1.0f;
  }
  if (is_spot_light(light.type)) {
    return light_spot_attenuation(light, L);
  }
  if (is_area_light(light.type)) {
    return float(dot(L, light.z_axis()) > 0.0f);
  }
  return 1.0f;
}

float light_shape_radius(LightData light)
{
  if (is_sun_light(light.type)) {
    return light.sun().shape_radius;
  }
  if (is_area_light(light.type)) {
    return length(light.area().size);
  }
  return light.local().local.shape_radius;
}

/**
 * Fade light influence when surface is not facing the light.
 * This is used in thickness from shadow.
 * Note: Ng is ideally the geometric normal.
 */
float light_attenuation_facing(
    LightData light, float3 L, float distance_to_light, float3 Ng, const bool is_transmission)
{
  /* Sine of angle between light center and light edge. */
  float sin_solid_angle = light_shape_radius(light) / distance_to_light;
  /* Sine of angle between light center and shading plane. */
  float sin_light_angle = dot(L, is_transmission ? -Ng : Ng);
  /* Do attenuation after the horizon line to avoid harsh cut
   * or biasing of surfaces without light bleeding. */
  float dist = sin_solid_angle + (is_transmission ? -sin_light_angle : sin_light_angle);
  return saturate((dist + 0.1f) * 10.0f);
}

float light_attenuation_surface(LightData light, const bool is_directional, LightVector lv)
{
  float result = light_attenuation_common(light, is_directional, lv.L);
  if (!is_directional) {
    result *= light_influence_attenuation(lv.dist,
                                          light.local().local.influence_radius_invsqr_surface);
  }
  return result;
}

float light_attenuation_volume(LightData light, const bool is_directional, LightVector lv)
{
  float result = light_attenuation_common(light, is_directional, lv.L);
  if (!is_directional) {
    result *= light_influence_attenuation(lv.dist,
                                          light.local().local.influence_radius_invsqr_volume);
  }
  return result;
}

/* Cheaper alternative than evaluating the LTC.
 * The result needs to be multiplied by BSDF or Phase Function. */
float light_point_light(LightData light, const bool is_directional, LightVector lv)
{
  if (is_directional) {
    return 1.0f;
  }
  /* Using "Point Light Attenuation Without Singularity" from Cem Yuksel
   * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
   * http://www.cemyuksel.com/research/pointlightattenuation/
   */
  float d_sqr = square(lv.dist);
  float r_sqr = square(light.local().local.shape_radius);
  /* Using reformulation that has better numerical precision. */
  float power = 2.0f / (d_sqr + r_sqr + lv.dist * sqrt(d_sqr + r_sqr));

  if (is_area_light(light.type)) {
    /* Modulate by light plane orientation / solid angle. */
    power *= saturate(dot(light.z_axis(), lv.L));
  }
  return power;
}

/**
 * Return the radius of the disk at the sphere origin spanning the same solid angle as the sphere
 * from a given distance.
 * Assume `distance_to_sphere > sphere_radius`, otherwise return almost infinite radius.
 */
float light_sphere_disk_radius(float sphere_radius, float distance_to_sphere)
{
  /* The sine of the half-angle spanned by a sphere light is equal to the tangent of the
   * half-angle spanned by a disk light with the same radius. */
  return sphere_radius *
         inversesqrt(max(1e-8f, 1.0f - square(sphere_radius / distance_to_sphere)));
}

/**
 * Stores a world-space polygon/ellipse and some information about the plane this
 * shape embeds on, used in the LTC evaluation.
 */
struct LightShape {
  /* Corner vertices. */
  float3 v[4];
  /* Normal defining light plane. */
  float3 N;
  /* Radius of disk encapsulating shape on light plane,
   * if the disk is projected on the unit sphere. */
  float disk_radius_projected;

  static LightShape get(LightData light, LightVector lv)
  {
    LightShape shape;

    float3 Px = light.x_axis();
    float3 Py = light.y_axis();

    if (light.type == LIGHT_RECT) {
      LightAreaData area = light.area();

      shape.v[0] = Px * area.size.x + Py * -area.size.y;
      shape.v[1] = Px * area.size.x + Py * area.size.y;
      shape.v[2] = -shape.v[0];
      shape.v[3] = -shape.v[1];

      float3 L = lv.L * lv.dist;
      shape.v[0] += L;
      shape.v[1] += L;
      shape.v[2] += L;
      shape.v[3] += L;

      shape.N = cross(Px, Py); /* Why N != light.z_axis()? */
      shape.disk_radius_projected = 0.5f * distance(shape.v[0], shape.v[2]) / lv.dist;
    }
    else {
      if (!is_area_light(light.type)) {
        make_orthonormal_basis(lv.L, Px, Py);
      }

      float2 size;
      if (is_sphere_light(light.type)) {
        /* Spherical omni or spot light. */
        size = float2(light_sphere_disk_radius(light.local().local.shape_radius, lv.dist));
      }
      else if (is_oriented_disk_light(light.type)) {
        /* View direction-aligned disk. */
        size = float2(light.local().local.shape_radius);
      }
      else if (is_sun_light(light.type)) {
        size = float2(light.sun().shape_radius);
      }
      else {
        /* Area light. */
        size = float2(light.area().size);
      }

      shape.v[0] = Px * -size.x + Py * -size.y;
      shape.v[1] = Px * size.x + Py * -size.y;
      shape.v[2] = -shape.v[0];

      float3 L = lv.L * lv.dist;
      shape.v[0] += L;
      shape.v[1] += L;
      shape.v[2] += L;

      shape.N = cross(Px, Py); /* Why N != light.z_axis()? */
      shape.disk_radius_projected = max(size.x, size.y) / lv.dist;
    }

    return shape;
  }
};

namespace eevee::light {

float power_get(LightData light, LightingType type)
{
  /* Mask anything above 3. See LIGHT_TRANSLUCENT_WITH_THICKNESS. */
  return light.power_factor[type & 3u] *
         (type != LIGHT_VOLUME ? light.shape_power : light.point_power);
}

bool light_linking_affects_receiver(uint2 light_set_membership, uchar receiver_light_set)
{
  return bitmask64_test(light_set_membership, receiver_light_set);
}

}  // namespace eevee::light

/** \} */
