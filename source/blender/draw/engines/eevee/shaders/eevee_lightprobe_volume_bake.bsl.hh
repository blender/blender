/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shaders related to lightprobe volume baking.
 * Note that this file only contains part of the implementation.
 * Shaders that are dispatched per surfel are in `eevee_surfel_*` files.
 */
#pragma once

#include "draw_intersect_lib.glsl"
#include "eevee_lightprobe_sphere.bsl.hh"
#include "eevee_lightprobe_volume.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "eevee_surfel_list.bsl.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

namespace eevee::lightprobe::volume {

struct SceneBound {
  [[storage(0, read_write)]] CaptureInfoData &capture_info_buf;
  [[storage(1, read)]] const ObjectBounds (&bounds_buf)[];

  [[push_constant]] const int resource_len;
};

/**
 * Compute the scene axis-aligned bounding box using all instances bounding boxes.
 */
[[compute]] [[local_size(IRRADIANCE_BOUNDS_GROUP_SIZE)]] void scene_bounds(
    [[resource_table]] SceneBound &srt, [[global_invocation_id]] const uint3 global_id)
{
  uint index = global_id.x;
  if (index >= uint(srt.resource_len)) {
    return;
  }

  ObjectBounds bounds = srt.bounds_buf[index];
  if (!drw_bounds_are_valid(bounds)) {
    return;
  }

  IsectBox box = isect_box_setup(bounds.bounding_corners[0].xyz,
                                 bounds.bounding_corners[1].xyz,
                                 bounds.bounding_corners[2].xyz,
                                 bounds.bounding_corners[3].xyz);

  float3 local_min = float3(FLT_MAX);
  float3 local_max = float3(-FLT_MAX);
  for (int i = 0; i < 8; i++) {
    local_min = min(local_min, box.corners[i].xyz);
    local_max = max(local_max, box.corners[i].xyz);
  }

  atomicMin(srt.capture_info_buf.scene_bound_x_min, floatBitsToOrderedInt(local_min.x));
  atomicMax(srt.capture_info_buf.scene_bound_x_max, floatBitsToOrderedInt(local_max.x));

  atomicMin(srt.capture_info_buf.scene_bound_y_min, floatBitsToOrderedInt(local_min.y));
  atomicMax(srt.capture_info_buf.scene_bound_y_max, floatBitsToOrderedInt(local_max.y));

  atomicMin(srt.capture_info_buf.scene_bound_z_min, floatBitsToOrderedInt(local_min.z));
  atomicMax(srt.capture_info_buf.scene_bound_z_max, floatBitsToOrderedInt(local_max.z));
}

struct VolumeOffset {
  [[resource_table]] srt_t<SurfelData> surfels_data;

  [[storage(0, read)]] const int (&list_start_buf)[];
  [[storage(6, read)]] const SurfelListInfoData &list_info_buf;

  [[image(0, read, SINT_32)]] const iimage3DAtomic cluster_list_img;
  [[image(1, read_write, SFLOAT_16_16_16_16)]] image3D virtual_offset_img;

  int find_closest_surfel(int3 grid_coord, float3 P)
  {
    [[resource_table]] SurfelData &surfels = surfels_data;

    int surfel_first = imageLoad(cluster_list_img, grid_coord).r;
    float search_radius_sqr = square(surfels.capture_info_buf.max_virtual_offset +
                                     surfels.capture_info_buf.min_distance_to_surface);

    int closest_surfel = -1;
    float closest_distance_sqr = 1e10f;
    for (int surfel_id = surfel_first; surfel_id > -1;
         surfel_id = surfels.surfel_buf[surfel_id].next)
    {
      Surfel surfel = surfels.surfel_buf[surfel_id];

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
    [[resource_table]] SurfelData &surfels = surfels_data;

    if (abs(surfel_distance) > surfels.capture_info_buf.min_distance_to_surface) {
      return 0.0f;
    }
    /* NOTE: distance can be negative. */
    return surfel_distance - ((surfel_distance > 0.0f) ?
                                  surfels.capture_info_buf.min_distance_to_surface :
                                  -surfels.capture_info_buf.min_distance_to_surface);
  }

  float back_facing_offset(float surfel_distance)
  {
    [[resource_table]] SurfelData &surfels = surfels_data;

    if (surfel_distance > surfels.capture_info_buf.max_virtual_offset) {
      return 0.0f;
    }
    /* NOTE: distance can be negative. */
    return surfel_distance + ((surfel_distance > 0.0f) ?
                                  surfels.capture_info_buf.min_distance_to_surface :
                                  -surfels.capture_info_buf.min_distance_to_surface);
  }

  float compute_offset_length(int3 grid_coord, float3 P, float3 offset_direction)
  {
    [[resource_table]] SurfelData &surfels = surfels_data;

    int surfel_first = imageLoad(cluster_list_img, grid_coord).r;
    float search_radius = max(surfels.capture_info_buf.max_virtual_offset,
                              surfels.capture_info_buf.min_distance_to_surface);
    /* Scale it a bit to avoid missing surfaces. */
    float ray_radius = surfels.capture_info_buf.surfel_radius * M_SQRT2;

    /* Nearest and farthest surfels in offset direction on both sides. */
    int surfel_pos = -1;
    int surfel_neg = -1;
    float surfel_distance_pos = +1e10f;
    float surfel_distance_neg = -1e10f;
    for (int surfel_id = surfel_first; surfel_id > -1;
         surfel_id = surfels.surfel_buf[surfel_id].next)
    {
      Surfel surfel = surfels.surfel_buf[surfel_id];

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
      bool is_front_facing_pos = dot(offset_direction, surfels.surfel_buf[surfel_pos].normal) <
                                 0.0f;
      bool is_front_facing_neg = dot(-offset_direction, surfels.surfel_buf[surfel_neg].normal) <
                                 0.0f;
      if (is_front_facing_pos && is_front_facing_neg) {
        /* If both sides have same facing. */
        if (is_front_facing_pos) {
          /* If both sides are front facing. */
          float distance_between_neighbors = surfel_distance_pos - surfel_distance_neg;
          if (distance_between_neighbors < surfels.capture_info_buf.min_distance_to_surface * 2.0f)
          {
            /* Choose the middle point. */
            return (surfel_distance_pos + surfel_distance_neg) / 2.0f;
          }
          /* Choose the maximum offset. */
          /* NOTE: The maximum offset is always from positive side since it's the closest. */
          return front_facing_offset(surfel_distance_pos);
        }
        /* If both sides are back facing. */
        /* Choose the minimum offset. */
        /* NOTE: The minimum offset is always from positive side since it's the closest. */
        return back_facing_offset(surfel_distance_pos);
      }
      /* If both sides have different facing. */
      float front_distance = is_front_facing_pos ? surfel_distance_pos : surfel_distance_neg;
      float back_distance = !is_front_facing_pos ? surfel_distance_pos : surfel_distance_neg;
      float front_offset = front_facing_offset(front_distance);
      float back_offset = back_facing_offset(back_distance);
      /* Choose the minimum offset. */
      return (abs(front_offset) < abs(back_offset)) ? front_offset : back_offset;
    }

    if (has_neighbor_pos || has_neighbor_neg) {
      /* Only one side have neighbor. */
      int nearest_surfel_id = has_neighbor_pos ? surfel_pos : surfel_neg;
      float surfel_distance = has_neighbor_pos ? surfel_distance_pos : surfel_distance_neg;
      bool is_front_facing = dot(has_neighbor_pos ? offset_direction : -offset_direction,
                                 surfels.surfel_buf[nearest_surfel_id].normal) < 0.0f;
      if (is_front_facing) {
        return front_facing_offset(surfel_distance);
      }
      return back_facing_offset(surfel_distance);
    }
    /* If no sides has neighbor (should never happen here since we already bailed out). */
    return 0.0f;
  }
};

/**
 * For every irradiance probe sample, check if close to a surrounding surfel and try to offset the
 * irradiance sample position. This is similar to the surfel ray but we do not actually transport
 * the light.
 *
 * Dispatched as 1 thread per irradiance probe sample.
 */
[[compute]] [[local_size(IRRADIANCE_GRID_BRICK_SIZE,
                         IRRADIANCE_GRID_BRICK_SIZE,
                         IRRADIANCE_GRID_BRICK_SIZE)]]
void volume_offset([[resource_table]] VolumeOffset &srt,
                   [[global_invocation_id]] const uint3 global_id)
{
  [[resource_table]] SurfelData &surfels = srt.surfels_data;

  int3 grid_coord = int3(global_id);

  if (any(greaterThanEqual(grid_coord, surfels.capture_info_buf.irradiance_grid_size))) {
    return;
  }

  float3 P = lightprobe::volume::grid_sample_position(
      surfels.capture_info_buf.irradiance_grid_local_to_world,
      surfels.capture_info_buf.irradiance_grid_size,
      grid_coord);

  int closest_surfel_id = srt.find_closest_surfel(grid_coord, P);
  if (closest_surfel_id == -1) {
    imageStoreFast(srt.virtual_offset_img, grid_coord, float4(0.0f));
    return;
  }

  /* Offset direction towards the sampling point. */
  // float3 offset_direction = safe_normalize(surfel_buf[closest_surfel_id].position - P);
  /* NOTE: Use normal direction of the surfel instead for stability reasons. */
  float3 offset_direction = surfels.surfel_buf[closest_surfel_id].normal;
  bool is_front_facing = dot(surfels.surfel_buf[closest_surfel_id].position - P,
                             surfels.surfel_buf[closest_surfel_id].normal) < 0.0f;
  if (is_front_facing) {
    offset_direction = -offset_direction;
  }

  float offset_length = srt.compute_offset_length(grid_coord, P, offset_direction);

  float3 virtual_offset = offset_direction * offset_length;

  imageStoreFast(srt.virtual_offset_img, grid_coord, float4(virtual_offset, 0.0f));
}

struct RayCapture {
  [[resource_table]] srt_t<LightprobeSphereRenderData> lightprobe_sphere;
  [[resource_table]] srt_t<SurfelData> surfels_data;

  [[storage(0, read)]] const int (&list_start_buf)[];
  [[storage(6, read)]] const SurfelListInfoData &list_info_buf;

  [[push_constant]] const int radiance_src;
  [[image(0, read_write, SFLOAT_32_32_32_32)]] image3D irradiance_L0_img;
  [[image(1, read_write, SFLOAT_32_32_32_32)]] image3D irradiance_L1_a_img;
  [[image(2, read_write, SFLOAT_32_32_32_32)]] image3D irradiance_L1_b_img;
  [[image(3, read_write, SFLOAT_32_32_32_32)]] image3D irradiance_L1_c_img;
  [[image(4, read, SFLOAT_16_16_16_16)]] const image3D virtual_offset_img;
  [[image(5, read_write, SFLOAT_32)]] image3D validity_img;

  void irradiance_capture(float3 L,
                          float3 irradiance,
                          float visibility,
                          SphericalHarmonicL1<float4> &sh)
  {
    [[resource_table]] SurfelData &surfels = surfels_data;

    float3 lL = transform_direction(
        surfels.capture_info_buf.irradiance_grid_world_to_local_rotation, L);

    /* Spherical harmonics need to be weighted by sphere area. */
    irradiance *= 4.0f * M_PI;
    visibility *= 4.0f * M_PI;

    sh.encode_signal_sample(lL, float4(irradiance, visibility));
  }

  void irradiance_capture_surfel(Surfel surfel, float3 P, SphericalHarmonicL1<float4> &sh)
  {
    [[resource_table]] SurfelData &surfels = surfels_data;

    float3 L = safe_normalize(surfel.position - P);
    bool facing = dot(-L, surfel.normal) > 0.0f;
    SurfelRadiance surfel_radiance_indirect = surfel.radiance_indirect[radiance_src];

    float4 irradiance_vis = float4(0.0f);
    irradiance_vis += facing ? surfel.radiance_direct.front : surfel.radiance_direct.back;

    /* Clamped brightness. */
    float luma = max(1e-8f, reduce_max(irradiance_vis.rgb));
    irradiance_vis.rgb *= 1.0f - max(0.0f, luma - surfels.capture_info_buf.clamp_direct) / luma;

    /* NOTE: The indirect radiance is already normalized and this is wanted, because we are not
     * integrating the same signal and we would have the SH lagging behind the surfel integration
     * otherwise. */
    irradiance_vis += facing ? surfel_radiance_indirect.front : surfel_radiance_indirect.back;

    irradiance_capture(L, irradiance_vis.rgb, irradiance_vis.a, sh);
  }

  void validity_capture_surfel(Surfel surfel, float3 P, float &validity)
  {
    float3 L = safe_normalize(surfel.position - P);
    bool facing = surfel.double_sided || dot(-L, surfel.normal) > 0.0f;
    validity += float(facing);
  }

  void validity_capture_world(float3 /*L*/, float &validity)
  {
    validity += 1.0f;
  }

  void irradiance_capture_world(float3 L, SphericalHarmonicL1<float4> &sh)
  {
    [[resource_table]] const LightprobeSphereRenderData &lp_sphere = lightprobe_sphere;
    [[resource_table]] SurfelData &surfels = surfels_data;

    float3 radiance = float3(0.0f);
    float visibility = 0.0f;

    if (surfels.capture_info_buf.capture_world_direct) {
      SphereProbeUvArea atlas_coord = surfels.capture_info_buf.world_atlas_coord;
      radiance = lp_sphere.sample_probe(L, 0.0f, atlas_coord).rgb;

      /* Clamped brightness. */
      float luma = max(1e-8f, reduce_max(radiance));
      radiance *= 1.0f - max(0.0f, luma - surfels.capture_info_buf.clamp_direct) / luma;
    }

    if (surfels.capture_info_buf.capture_visibility_direct) {
      visibility = 1.0f;
    }

    irradiance_capture(L, radiance, visibility, sh);
  }
};

/**
 * For every irradiance probe sample, compute the incoming radiance from both side.
 * This is the same as the surfel ray but we do not actually transport the light, we only capture
 * the irradiance as spherical harmonic coefficients.
 *
 * Dispatched as 1 thread per irradiance probe sample.
 */
[[compute]] [[local_size(IRRADIANCE_GRID_GROUP_SIZE,
                         IRRADIANCE_GRID_GROUP_SIZE,
                         IRRADIANCE_GRID_GROUP_SIZE)]]
void ray_capture([[resource_table]] RayCapture &srt,
                 [[resource_table]] const draw::View &views,
                 [[global_invocation_id]] const uint3 global_id)
{
  [[resource_table]] SurfelData &surfels = srt.surfels_data;

  int3 grid_coord = int3(global_id);

  if (any(greaterThanEqual(grid_coord, surfels.capture_info_buf.irradiance_grid_size))) {
    return;
  }

  const ViewMatrices view = views.get(0);

  float3 P = lightprobe::volume::grid_sample_position(
      surfels.capture_info_buf.irradiance_grid_local_to_world,
      surfels.capture_info_buf.irradiance_grid_size,
      grid_coord);

  /* Add virtual offset to avoid baking inside of geometry as much as possible. */
  P += imageLoadFast(srt.virtual_offset_img, grid_coord).xyz;

  /* Project to get ray linked list. */
  float irradiance_sample_ray_distance;
  int list_index = eevee::surfel::list_index_get(
      view, srt.list_info_buf.ray_grid_size, P, irradiance_sample_ray_distance);

  /* Walk the ray to get which surfels the irradiance sample is between. */
  int surfel_prev = -1;
  int surfel_next = srt.list_start_buf[list_index];
  /* Avoid spinning for eternity. */
  for (int i = 0; i < 9999; i++) {
    if (surfel_next <= -1) {
      break;
    }
    /* Reminder: List is sorted with highest value first. */
    if (surfels.surfel_buf[surfel_next].ray_distance < irradiance_sample_ray_distance) {
      break;
    }
    surfel_prev = surfel_next;
    surfel_next = surfels.surfel_buf[surfel_next].next;
    assert(surfel_prev != surfel_next);
  }

  float3 sky_L = view.world_incident_vector(P);

  SphericalHarmonicL1<float4> sh;
  sh.L0.M0 = imageLoadFast(srt.irradiance_L0_img, grid_coord);
  sh.L1.Mn1 = imageLoadFast(srt.irradiance_L1_a_img, grid_coord);
  sh.L1.M0 = imageLoadFast(srt.irradiance_L1_b_img, grid_coord);
  sh.L1.Mp1 = imageLoadFast(srt.irradiance_L1_c_img, grid_coord);
  float validity = imageLoadFast(srt.validity_img, grid_coord).r;

  /* Un-normalize for accumulation. */
  float weight_captured = surfels.capture_info_buf.sample_index * 2.0f;
  sh.L0.M0 *= weight_captured;
  sh.L1.Mn1 *= weight_captured;
  sh.L1.M0 *= weight_captured;
  sh.L1.Mp1 *= weight_captured;
  validity *= weight_captured;

  if (surfel_next > -1) {
    Surfel surfel = surfels.surfel_buf[surfel_next];
    srt.irradiance_capture_surfel(surfel, P, sh);
    srt.validity_capture_surfel(surfel, P, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(surfel.position, P, float4(0, 1, 0, 1), drw_debug_persistent_lifetime);
#endif
  }
  else {
    srt.irradiance_capture_world(-sky_L, sh);
    srt.validity_capture_world(-sky_L, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(P - sky_L, P, float4(0, 1, 1, 1), drw_debug_persistent_lifetime);
#endif
  }

  if (surfel_prev > -1) {
    Surfel surfel = surfels.surfel_buf[surfel_prev];
    srt.irradiance_capture_surfel(surfel, P, sh);
    srt.validity_capture_surfel(surfel, P, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(surfel.position, P, float4(1, 0, 1, 1), drw_debug_persistent_lifetime);
#endif
  }
  else {
    srt.irradiance_capture_world(sky_L, sh);
    srt.validity_capture_world(sky_L, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(P + sky_L, P, float4(1, 1, 0, 1), drw_debug_persistent_lifetime);
#endif
  }

  /* Normalize for storage. We accumulated 2 samples. */
  weight_captured += 2.0f;
  sh.L0.M0 /= weight_captured;
  sh.L1.Mn1 /= weight_captured;
  sh.L1.M0 /= weight_captured;
  sh.L1.Mp1 /= weight_captured;
  validity /= weight_captured;

  imageStoreFast(srt.irradiance_L0_img, grid_coord, sh.L0.M0);
  imageStoreFast(srt.irradiance_L1_a_img, grid_coord, sh.L1.Mn1);
  imageStoreFast(srt.irradiance_L1_b_img, grid_coord, sh.L1.M0);
  imageStoreFast(srt.irradiance_L1_c_img, grid_coord, sh.L1.Mp1);
  imageStoreFast(srt.validity_img, grid_coord, float4(validity));
}

}  // namespace eevee::lightprobe::volume

PipelineCompute eevee_lightprobe_volume_offset(eevee::lightprobe::volume::volume_offset);
PipelineCompute eevee_lightprobe_volume_bounds(eevee::lightprobe::volume::scene_bounds);
PipelineCompute eevee_lightprobe_volume_ray(eevee::lightprobe::volume::ray_capture);
