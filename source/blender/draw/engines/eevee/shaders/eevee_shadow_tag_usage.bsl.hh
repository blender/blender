/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 */

#pragma once
#pragma create_info

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_view)
COMPUTE_SHADER_CREATE_INFO(draw_view_culling)
COMPUTE_SHADER_CREATE_INFO(eevee_hiz_data)
COMPUTE_SHADER_CREATE_INFO(eevee_light_data)
COMPUTE_SHADER_CREATE_INFO(draw_resource_id_varying)

#include "draw_view_lib.glsl"
#include "eevee_defines.hh"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_lightprobe_shared.hh"
#include "eevee_sampling_lib.glsl"
#include "eevee_shadow_page_ops.bsl.hh"
#include "eevee_shadow_shared.hh"
#include "eevee_shadow_tilemap_lib.glsl"
#include "eevee_volume_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"

namespace eevee::shadow::usage {

struct TagUsage {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_view_culling;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;

  [[resource_table]] srt_t<TileMaps> tilemaps;
  [[resource_table]] srt_t<Tiles> tiles;

 public:
  /**
   * \a radius Radius of the tagging area in world space.
   * Used for downsampled/ray-marched tagging, so all the shadow-map texels covered get correctly
   * tagged.
   */
  void tag_pixel(float3 vP,
                 float3 P,
                 float2 pixel,
                 float3 V = float3(0),
                 float radius = 0.0f,
                 int lod_bias = 0)
  {
    LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
      shadow_tag_usage_tilemap_directional(l_idx, P, V, radius, lod_bias);
    }
    LIGHT_FOREACH_END

    LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx)
    {
      tag_usage_tilemap_punctual(l_idx, P, radius, lod_bias);
    }
    LIGHT_FOREACH_END
  }

  void tag_surfel(Surfel surfel, int directional_lvl)
  {
    float3 P = surfel.position;

    LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
      tag_usage_tilemap_directional_at_level(l_idx, P, directional_lvl);
    }
    LIGHT_FOREACH_END

    LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
    {
      tag_usage_tilemap_punctual(l_idx, P, 0, 0);
    }
    LIGHT_FOREACH_END
  }

 private:
  void tag_usage_tile(LightData light, uint2 tile_co, int lod, int tilemap_index)
  {
    if (tilemap_index > light_tilemap_max_get(light)) {
      return;
    }

    tile_co >>= uint(lod);
    [[resource_table]] Tiles &tiles_ref = tiles;
    [[resource_table]] TileMaps &maps = tilemaps;
    int index = shadow_tile_offset(tile_co, maps.tilemaps_buf[tilemap_index].tiles_index, lod);
    atomicOr(tiles_ref.tiles_buf[index], uint(SHADOW_IS_USED));
  }

  void tag_usage_tilemap_directional_at_level(uint l_idx, float3 P, int level)
  {
    LightData light = light_buf[l_idx];

    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      return;
    }

    float3 lP = light_world_to_local_direction(light, P);

    level = clamp(level, light.sun().clipmap_lod_min, light.sun().clipmap_lod_max);

    ShadowCoordinates coord = shadow_directional_coordinates_at_level(light, lP, level);
    tag_usage_tile(light, coord.tilemap_tile, 0, coord.tilemap_index);
  }

  void shadow_tag_usage_tilemap_directional(
      uint l_idx, float3 P, float3 V, float radius, int lod_bias)
  {
    LightData light = light_buf[l_idx];

    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      return;
    }

    float3 lP = light_world_to_local_direction(light, P);

    LightSunData sun = light.sun();

    if (radius == 0.0f) {
      int level = shadow_directional_level(light, lP - light_position_get(light));
      level = clamp(level + lod_bias, sun.clipmap_lod_min, sun.clipmap_lod_max);
      ShadowCoordinates coord = shadow_directional_coordinates_at_level(light, lP, level);
      tag_usage_tile(light, coord.tilemap_tile, 0, coord.tilemap_index);
    }
    else {
      float3 start_lP = light_world_to_local_direction(light, P - V * radius);
      float3 end_lP = light_world_to_local_direction(light, P + V * radius);
      int min_level = shadow_directional_level(light, start_lP - light_position_get(light));
      int max_level = shadow_directional_level(light, end_lP - light_position_get(light));
      min_level = clamp(min_level + lod_bias, sun.clipmap_lod_min, sun.clipmap_lod_max);
      max_level = clamp(max_level + lod_bias, sun.clipmap_lod_min, sun.clipmap_lod_max);

      for (int level = min_level; level <= max_level; level++) {
        ShadowCoordinates coord_min = shadow_directional_coordinates_at_level(
            light, lP - float3(radius, radius, 0.0f), level);
        ShadowCoordinates coord_max = shadow_directional_coordinates_at_level(
            light, lP + float3(radius, radius, 0.0f), level);

        for (uint x = coord_min.tilemap_tile.x; x <= coord_max.tilemap_tile.x; x++) {
          for (uint y = coord_min.tilemap_tile.y; y <= coord_max.tilemap_tile.y; y++) {
            tag_usage_tile(light, uint2(x, y), 0, coord_min.tilemap_index);
          }
        }
      }
    }
  }

  void tag_usage_tilemap_punctual(uint l_idx, float3 P, float radius, int lod_bias)
  {
    LightData light = light_buf[l_idx];

    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      return;
    }

    float3 lP = light_world_to_local_point(light, P);
    float dist_to_light = max(length(lP) - radius, 1e-5f);
    if (dist_to_light > light.local().local.influence_radius_max) {
      return;
    }
    if (is_spot_light(light.type)) {
      /* Early out if out of cone. */
      float angle_tan = length(lP.xy / dist_to_light);
      if (angle_tan > light.spot().spot_tan) {
        return;
      }
    }
    else if (is_area_light(light.type)) {
      /* Early out if on the wrong side. */
      if (lP.z - radius > 0.0f) {
        return;
      }
    }

    /* Transform to shadow local space. */
    lP -= light.local().local.shadow_position;

    int lod = shadow_punctual_level(light,
                                    lP,
                                    drw_view_is_perspective(),
                                    drw_view_z_distance(P),
                                    uniform_buf.shadow.film_pixel_radius);
    lod = clamp(lod + lod_bias, 0, SHADOW_TILEMAP_LOD);

    if (radius == 0) {
      int face_id = shadow_punctual_face_index_get(lP);
      lP = shadow_punctual_local_position_to_face_local(face_id, lP);
      ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);
      tag_usage_tile(light, coord.tilemap_tile, lod, coord.tilemap_index);
    }
    else {
      uint faces = 0u;
      for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
          for (int z = -1; z <= 1; z += 2) {
            float3 _lP = lP + float3(x, y, z) * radius;
            faces |= 1u << shadow_punctual_face_index_get(_lP);
          }
        }
      }

      for (int face_id = 0; face_id < 6; face_id++) {
        if ((faces & (1u << uint(face_id))) == 0u) {
          continue;
        }

        int tilemap_index = light.tilemap_index + face_id;
        float3 _lP = shadow_punctual_local_position_to_face_local(face_id, lP);

        float3 offset = float3(radius, radius, 0);
        ShadowCoordinates coord_min = shadow_punctual_coordinates(light, _lP - offset, face_id);
        ShadowCoordinates coord_max = shadow_punctual_coordinates(light, _lP + offset, face_id);

        for (uint x = coord_min.tilemap_tile.x; x <= coord_max.tilemap_tile.x; x++) {
          for (uint y = coord_min.tilemap_tile.y; y <= coord_max.tilemap_tile.y; y++) {
            tag_usage_tile(light, uint2(x, y), lod, tilemap_index);
          }
        }
      }
    }
  }
};

struct TagUsageOpaque {
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;

  [[push_constant]] int2 input_depth_extent;
};

/**
 * This pass scan the depth buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */
[[compute, local_size(SHADOW_DEPTH_SCAN_GROUP_SIZE, SHADOW_DEPTH_SCAN_GROUP_SIZE)]]
void tag_usage_opaque([[resource_table]] TagUsageOpaque &srt,
                      [[resource_table]] TagUsage &tag,
                      [[global_invocation_id]] const uint3 global_id)
{
  int2 texel = int2(global_id.xy);
  int2 tex_size = srt.input_depth_extent;

  if (!in_range_inclusive(texel, int2(0), int2(tex_size - 1))) {
    return;
  }

  float depth = texelFetch(hiz_tx, texel, 0).r;
  if (depth == 1.0f) {
    return;
  }

  float2 uv = (float2(texel) + 0.5f) / float2(tex_size);
  float3 vP = drw_point_screen_to_view(float3(uv, depth));
  float3 P = drw_point_view_to_world(vP);
  float2 pixel = float2(global_id.xy);

  tag.tag_pixel(vP, P, pixel);
}

struct TagUsageSurfel {
  [[push_constant]] int directional_level;
};

struct SurfelCapture {
  [[storage(SURFEL_BUF_SLOT, read_write)]] Surfel (&surfel_buf)[];
  [[storage(CAPTURE_BUF_SLOT, read)]] CaptureInfoData &capture_info_buf;
};

/**
 * This pass iterates the surfels buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */
[[compute, local_size(SURFEL_GROUP_SIZE)]]
void tag_usage_surfel([[resource_table]] TagUsageSurfel &srt,
                      [[resource_table]] TagUsage &tag,
                      [[resource_table]] SurfelCapture &capture,
                      [[global_invocation_id]] const uint3 global_id)
{
  uint index = global_id.x;
  if (index >= capture.capture_info_buf.surfel_len) {
    return;
  }

  Surfel surfel = capture.surfel_buf[index];

  tag.tag_surfel(surfel, srt.directional_level);
}

struct VolumeProperties {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[image(VOLUME_PROP_SCATTERING_IMG_SLOT, read, UFLOAT_11_11_10)]] image3D in_scattering_img;
  [[image(VOLUME_PROP_EXTINCTION_IMG_SLOT, read, UFLOAT_11_11_10)]] image3D in_extinction_img;
  [[image(VOLUME_PROP_EMISSION_IMG_SLOT, read, UFLOAT_11_11_10)]] image3D in_emission_img;
  [[image(VOLUME_PROP_PHASE_IMG_SLOT, read, SFLOAT_16)]] image3D in_phase_img;
  [[image(VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT, read, SFLOAT_16)]] image3D in_phase_weight_img;
};

struct TagUsageVolume {
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
};

/**
 * This pass scans all volume froxels and tags tiles needed for shadowing.
 */
[[compute, local_size(VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE)]]
void tag_usage_volume([[resource_table]] TagUsageVolume &srt,
                      [[resource_table]] VolumeProperties &volume,
                      [[resource_table]] TagUsage &tag,
                      [[resource_table]] SurfelCapture &capture,
                      [[global_invocation_id]] const uint3 global_id)
{
  int3 froxel = int3(global_id);

  if (any(greaterThanEqual(froxel, uniform_buf.volumes.tex_size))) {
    return;
  }

  float3 extinction = imageLoadFast(volume.in_extinction_img, froxel).rgb;
  float3 scattering = imageLoadFast(volume.in_scattering_img, froxel).rgb;

  if (is_zero(extinction) || is_zero(scattering)) {
    return;
  }

  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = interleaved_gradient_noise(float2(froxel.xy), 0.0f, offset);

  float3 uvw = (float3(froxel) + float3(0.5f, 0.5f, jitter)) * uniform_buf.volumes.inv_tex_size;
  float3 ss_P = volume_resolve_to_screen(uvw);
  float3 vP = drw_point_screen_to_view(float3(ss_P.xy, ss_P.z));
  float3 P = drw_point_view_to_world(vP);

  float depth = texelFetch(hiz_tx, froxel.xy, uniform_buf.volumes.tile_size_lod).r;
  if (depth < ss_P.z) {
    return;
  }

  float2 pixel = ((float2(froxel.xy) + 0.5f) * uniform_buf.volumes.inv_tex_size.xy) *
                 uniform_buf.volumes.main_view_extent;

  int bias = uniform_buf.volumes.tile_size_lod;
  tag.tag_pixel(vP, P, pixel, drw_world_incident_vector(P), 0.01f, bias);
}

}  // namespace eevee::shadow::usage

namespace eevee::shadow {

PipelineCompute tag_usage_opaque(usage::tag_usage_opaque);
PipelineCompute tag_usage_surfels(usage::tag_usage_surfel);
PipelineCompute tag_usage_volume(usage::tag_usage_volume);

}  // namespace eevee::shadow
