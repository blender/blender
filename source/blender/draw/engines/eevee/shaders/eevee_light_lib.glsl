/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_math_geom_lib.glsl"
#include "eevee_ltc_lib.glsl"

/* Attenuation cutoff needs to be the same in the shadow loop and the light eval loop. */
#define LIGHT_ATTENUATION_THRESHOLD 1e-6f

/* ---------------------------------------------------------------------- */
/** \name Light Functions
 * \{ */

struct LightVector {
  /* World space light vector. From the shading point to the light center. Normalized. */
  float3 L;
  /* Distance from the shading point to the light center. */
  float dist;
};

LightVector light_vector_get(LightData light, const bool is_directional, float3 P)
{
  LightVector lv;
  if (is_directional) {
    lv.L = light_sun_data_get(light).direction;
    lv.dist = 1.0f;
  }
  else {
    lv.L = light_position_get(light) - P;
    float inv_distance = inversesqrt(length_squared(lv.L));
    lv.L *= inv_distance;
    lv.dist = 1.0f / inv_distance;
  }
  return lv;
}

/* Light vector to the closest point in the light shape. */
LightVector light_shape_vector_get(LightData light, const bool is_directional, float3 P)
{
  if (!is_directional && is_area_light(light.type)) {
    LightAreaData area = light_area_data_get(light);

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

    float3 L = ws_closest_point - P;
    float inv_distance = inversesqrt(length_squared(L));
    LightVector lv;
    lv.L = L * inv_distance;
    lv.dist = 1.0f / inv_distance;
    return lv;
  }
  /* TODO(@fclem): other light shape? */
  return light_vector_get(light, is_directional, P);
}

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
  LightSpotData spot = light_spot_data_get(light);
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
    return float(dot(L, light_z_axis(light)) > 0.0f);
  }
  return 1.0f;
}

float light_shape_radius(LightData light)
{
  if (is_sun_light(light.type)) {
    return light_sun_data_get(light).shape_radius;
  }
  if (is_area_light(light.type)) {
    return length(light_area_data_get(light).size);
  }
  return light_local_data_get(light).shape_radius;
}

/**
 * Fade light influence when surface is not facing the light.
 * This is needed because LTC leaks light at roughness not 0 or 1
 * when the light is below the horizon.
 * L is normalized vector to light shape center.
 * Ng is ideally the geometric normal.
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
    result *= light_influence_attenuation(
        lv.dist, light_local_data_get(light).influence_radius_invsqr_surface);
  }
  return result;
}

float light_attenuation_volume(LightData light, const bool is_directional, LightVector lv)
{
  float result = light_attenuation_common(light, is_directional, lv.L);
  if (!is_directional) {
    result *= light_influence_attenuation(
        lv.dist, light_local_data_get(light).influence_radius_invsqr_volume);
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
  float r_sqr = square(light_local_data_get(light).shape_radius);
  /* Using reformulation that has better numerical precision. */
  float power = 2.0f / (d_sqr + r_sqr + lv.dist * sqrt(d_sqr + r_sqr));

  if (is_area_light(light.type)) {
    /* Modulate by light plane orientation / solid angle. */
    power *= saturate(dot(light_z_axis(light), lv.L));
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

float light_ltc(
    sampler2DArray utility_tx, LightData light, float3 N, float3 V, LightVector lv, float4 ltc_mat)
{
  if (is_sphere_light(light.type) && lv.dist < light_local_data_get(light).shape_radius) {
    /* Inside the sphere light, integrate over the hemisphere. */
    return 1.0f;
  }

  float3 Px = light_x_axis(light);
  float3 Py = light_y_axis(light);

  if (light.type == LIGHT_RECT) {
    LightAreaData area = light_area_data_get(light);

    float3 corners[4];
    corners[0] = Px * area.size.x + Py * -area.size.y;
    corners[1] = Px * area.size.x + Py * area.size.y;
    corners[2] = -corners[0];
    corners[3] = -corners[1];

    float3 L = lv.L * lv.dist;
    corners[0] += L;
    corners[1] += L;
    corners[2] += L;
    corners[3] += L;

    ltc_transform_quad(N, V, ltc_matrix(ltc_mat), corners);

    return ltc_evaluate_quad(utility_tx, corners, float3(0.0f, 0.0f, 1.0f));
  }
  else {
    if (!is_area_light(light.type)) {
      make_orthonormal_basis(lv.L, Px, Py);
    }

    float2 size;
    if (is_sphere_light(light.type)) {
      /* Spherical omni or spot light. */
      size = float2(light_sphere_disk_radius(light_local_data_get(light).shape_radius, lv.dist));
    }
    else if (is_oriented_disk_light(light.type)) {
      /* View direction-aligned disk. */
      size = float2(light_local_data_get(light).shape_radius);
    }
    else if (is_sun_light(light.type)) {
      size = float2(light_sun_data_get(light).shape_radius);
    }
    else {
      /* Area light. */
      size = float2(light_area_data_get(light).size);
    }

    float3 points[3];
    points[0] = Px * -size.x + Py * -size.y;
    points[1] = Px * size.x + Py * -size.y;
    points[2] = -points[0];

    float3 L = lv.L * lv.dist;
    points[0] += L;
    points[1] += L;
    points[2] += L;

    return ltc_evaluate_disk(utility_tx, N, V, ltc_matrix(ltc_mat), points);
  }
}

/** \} */
