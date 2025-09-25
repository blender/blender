/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * For every irradiance probe sample, check if close to a surrounding surfel and try to offset the
 * irradiance sample position. This is similar to the surfel ray but we do not actually transport
 * the light.
 *
 * Dispatched as 1 thread per irradiance probe sample.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_volume_offset)

#include "eevee_lightprobe_lib.glsl"
#include "eevee_surfel_list_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

int find_closest_surfel(int3 grid_coord, float3 P)
{
  int surfel_first = imageLoad(cluster_list_img, grid_coord).r;
  float search_radius_sqr = square(capture_info_buf.max_virtual_offset +
                                   capture_info_buf.min_distance_to_surface);

  int closest_surfel = -1;
  float closest_distance_sqr = 1e10f;
  for (int surfel_id = surfel_first; surfel_id > -1; surfel_id = surfel_buf[surfel_id].next) {
    Surfel surfel = surfel_buf[surfel_id];

    float3 probe_to_surfel = surfel.position - P;
    float surfel_dist_sqr = length_squared(probe_to_surfel);
    /* Do not consider surfels that are outside of search radius. */
    if (surfel_dist_sqr > search_radius_sqr) {
      continue;
    }

    if (closest_distance_sqr > surfel_dist_sqr) {
      closest_distance_sqr = surfel_dist_sqr;
      closest_surfel = surfel_id;
    }
  }
  return closest_surfel;
}

float front_facing_offset(float surfel_distance)
{
  if (abs(surfel_distance) > capture_info_buf.min_distance_to_surface) {
    return 0.0f;
  }
  /* NOTE: distance can be negative. */
  return surfel_distance - ((surfel_distance > 0.0f) ? capture_info_buf.min_distance_to_surface :
                                                       -capture_info_buf.min_distance_to_surface);
}

float back_facing_offset(float surfel_distance)
{
  if (surfel_distance > capture_info_buf.max_virtual_offset) {
    return 0.0f;
  }
  /* NOTE: distance can be negative. */
  return surfel_distance + ((surfel_distance > 0.0f) ? capture_info_buf.min_distance_to_surface :
                                                       -capture_info_buf.min_distance_to_surface);
}

float compute_offset_length(int3 grid_coord, float3 P, float3 offset_direction)
{
  int surfel_first = imageLoad(cluster_list_img, grid_coord).r;
  float search_radius = max(capture_info_buf.max_virtual_offset,
                            capture_info_buf.min_distance_to_surface);
  /* Scale it a bit to avoid missing surfaces. */
  float ray_radius = capture_info_buf.surfel_radius * M_SQRT2;

  /* Nearest and farthest surfels in offset direction on both sides. */
  int surfel_pos = -1;
  int surfel_neg = -1;
  float surfel_distance_pos = +1e10f;
  float surfel_distance_neg = -1e10f;
  for (int surfel_id = surfel_first; surfel_id > -1; surfel_id = surfel_buf[surfel_id].next) {
    Surfel surfel = surfel_buf[surfel_id];

    float3 probe_to_surfel = surfel.position - P;
    float surf_dist_signed = dot(offset_direction, probe_to_surfel);
    /* Do not consider surfels that are outside of search radius. */
    if (abs(surf_dist_signed) > search_radius) {
      continue;
    }
    /* Emulate ray cast with any hit shader by discarding surfels outside of the ray radius. */
    float ray_dist = distance(surf_dist_signed * offset_direction, probe_to_surfel);
    if (ray_dist > ray_radius) {
      continue;
    }

    if (surf_dist_signed > 0.0f) {
      if (surfel_distance_pos > surf_dist_signed) {
        surfel_distance_pos = surf_dist_signed;
        surfel_pos = surfel_id;
      }
    }
    else {
      if (surfel_distance_neg < surf_dist_signed) {
        surfel_distance_neg = surf_dist_signed;
        surfel_neg = surfel_id;
      }
    }
  }

  bool has_neighbor_pos = surfel_pos != -1;
  bool has_neighbor_neg = surfel_neg != -1;

  if (has_neighbor_pos && has_neighbor_neg) {
    /* If both sides have neighbors. */
    bool is_front_facing_pos = dot(offset_direction, surfel_buf[surfel_pos].normal) < 0.0f;
    bool is_front_facing_neg = dot(-offset_direction, surfel_buf[surfel_neg].normal) < 0.0f;
    if (is_front_facing_pos && is_front_facing_neg) {
      /* If both sides have same facing. */
      if (is_front_facing_pos) {
        /* If both sides are front facing. */
        float distance_between_neighbors = surfel_distance_pos - surfel_distance_neg;
        if (distance_between_neighbors < capture_info_buf.min_distance_to_surface * 2.0f) {
          /* Choose the middle point. */
          return (surfel_distance_pos + surfel_distance_neg) / 2.0f;
        }
        else {
          /* Choose the maximum offset. */
          /* NOTE: The maximum offset is always from positive side since it's the closest. */
          return front_facing_offset(surfel_distance_pos);
        }
      }
      else {
        /* If both sides are back facing. */
        /* Choose the minimum offset. */
        /* NOTE: The minimum offset is always from positive side since it's the closest. */
        return back_facing_offset(surfel_distance_pos);
      }
    }
    else {
      /* If both sides have different facing. */
      float front_distance = is_front_facing_pos ? surfel_distance_pos : surfel_distance_neg;
      float back_distance = !is_front_facing_pos ? surfel_distance_pos : surfel_distance_neg;
      float front_offset = front_facing_offset(front_distance);
      float back_offset = back_facing_offset(back_distance);
      /* Choose the minimum offset. */
      return (abs(front_offset) < abs(back_offset)) ? front_offset : back_offset;
    }
  }

  if (has_neighbor_pos || has_neighbor_neg) {
    /* Only one side have neighbor. */
    int nearest_surfel_id = has_neighbor_pos ? surfel_pos : surfel_neg;
    float surfel_distance = has_neighbor_pos ? surfel_distance_pos : surfel_distance_neg;
    bool is_front_facing = dot(has_neighbor_pos ? offset_direction : -offset_direction,
                               surfel_buf[nearest_surfel_id].normal) < 0.0f;
    if (is_front_facing) {
      return front_facing_offset(surfel_distance);
    }
    else {
      return back_facing_offset(surfel_distance);
    }
  }
  /* If no sides has neighbor (should never happen here since we already bailed out). */
  return 0.0f;
}

void main()
{
  int3 grid_coord = int3(gl_GlobalInvocationID);

  if (any(greaterThanEqual(grid_coord, capture_info_buf.irradiance_grid_size))) {
    return;
  }

  float3 P = lightprobe_volume_grid_sample_position(
      capture_info_buf.irradiance_grid_local_to_world,
      capture_info_buf.irradiance_grid_size,
      grid_coord);

  int closest_surfel_id = find_closest_surfel(grid_coord, P);
  if (closest_surfel_id == -1) {
    imageStoreFast(virtual_offset_img, grid_coord, float4(0.0f));
    return;
  }

  /* Offset direction towards the sampling point. */
  // float3 offset_direction = safe_normalize(surfel_buf[closest_surfel_id].position - P);
  /* NOTE: Use normal direction of the surfel instead for stability reasons. */
  float3 offset_direction = surfel_buf[closest_surfel_id].normal;
  bool is_front_facing = dot(surfel_buf[closest_surfel_id].position - P,
                             surfel_buf[closest_surfel_id].normal) < 0.0f;
  if (is_front_facing) {
    offset_direction = -offset_direction;
  }

  float offset_length = compute_offset_length(grid_coord, P, offset_direction);

  float3 virtual_offset = offset_direction * offset_length;

  imageStoreFast(virtual_offset_img, grid_coord, float4(virtual_offset, 0.0f));
}
