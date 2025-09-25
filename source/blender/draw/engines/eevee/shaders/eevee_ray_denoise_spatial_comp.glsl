/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Spatial ray reuse. Denoise raytrace result using ratio estimator.
 *
 * Input: Ray direction * hit time, Ray radiance, Ray hit depth
 * Output: Ray radiance reconstructed, Mean Ray hit depth, Radiance Variance
 *
 * Shader is specialized depending on the type of ray to denoise.
 *
 * Following "Stochastic All The Things: Raytracing in Hybrid Real-Time Rendering"
 * by Tomasz Stachowiak
 * https://www.ea.com/seed/news/seed-dd18-presentation-slides-raytracing
 */

#include "infos/eevee_tracing_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_ray_denoise_spatial)

#include "draw_view_lib.glsl"
#include "eevee_closure_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void transmission_thickness_amend_closure(inout ClosureUndetermined cl,
                                          inout float3 V,
                                          float thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      bxdf_ggx_context_amend_transmission(cl, V, thickness);
      break;
    case CLOSURE_NONE_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
    case CLOSURE_BSDF_TRANSLUCENT_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
      break;
  }
}

/* Tag pixel radiance as invalid. */
void invalid_pixel_write(int2 texel)
{
  imageStoreFast(out_radiance_img, texel, float4(FLT_11_11_10_MAX, 0.0f));
  imageStoreFast(out_variance_img, texel, float4(0.0f));
  imageStoreFast(out_hit_depth_img, texel, float4(0.0f));
}

void main()
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);

  int2 texel_fullres = int2(gl_LocalInvocationID.xy + tile_coord * tile_size);
  int2 texel = (texel_fullres) / raytrace_resolution_scale;

  if (skip_denoise) {
    imageStore(out_radiance_img, texel_fullres, imageLoad(ray_radiance_img, texel));
    return;
  }

  /* Clear neighbor tiles that will not be processed. */
  /* TODO(fclem): Optimize this. We don't need to clear the whole ring. */
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      if (x == 0 && y == 0) {
        continue;
      }

      int2 tile_coord_neighbor = int2(tile_coord) + int2(x, y);
      if (!in_image_range(tile_coord_neighbor, tile_mask_img)) {
        continue;
      }

      int3 sample_tile = int3(tile_coord_neighbor, closure_index);

      uint tile_mask = imageLoadFast(tile_mask_img, sample_tile).r;
      bool tile_is_unused = !flag_test(tile_mask, 1u << 0u);
      if (tile_is_unused) {
        int2 texel_fullres_neighbor = texel_fullres + int2(x, y) * int(tile_size);
        invalid_pixel_write(texel_fullres_neighbor);
      }
    }
  }

  bool valid_texel = in_texture_range(texel_fullres, gbuf_header_tx);
  if (!valid_texel) {
    invalid_pixel_write(texel_fullres);
    return;
  }

  gbuffer::Header gbuf_header = gbuffer::read_header(texel_fullres);

  ClosureUndetermined closure = gbuffer::read_bin(texel_fullres, closure_index);

  if (closure.type == CLOSURE_NONE_ID) {
    invalid_pixel_write(texel_fullres);
    return;
  }

  float2 uv = (float2(texel_fullres) + 0.5f) * uniform_buf.raytrace.full_resolution_inv;
  float3 P = drw_point_screen_to_world(float3(uv, 0.5f));
  float3 V = drw_world_incident_vector(P);

  float thickness = gbuffer::read_thickness(gbuf_header, texel_fullres);
  if (thickness != 0.0f) {
    transmission_thickness_amend_closure(closure, V, thickness);
  }

  /* Compute filter size and needed sample count */
  float apparent_roughness = closure_apparent_roughness_get(closure);
  float filter_size_factor = saturate(apparent_roughness * 8.0f);
  uint sample_count = 1u + uint(15.0f * filter_size_factor + 0.5f);
  /* NOTE: filter_size should never be greater than twice RAYTRACE_GROUP_SIZE. Otherwise, the
   * reconstruction can becomes ill defined since we don't know if further tiles are valid. */
  float filter_size = 12.0f * sqrt(filter_size_factor);
  if (raytrace_resolution_scale > 1) {
    /* Filter at least 1 trace pixel to fight the undersampling. */
    filter_size = max(filter_size, 3.0f);
    sample_count = max(sample_count, 5u);
  }

  float2 noise = utility_tx_fetch(utility_tx, float2(texel_fullres), UTIL_BLUE_NOISE_LAYER).ba;
  noise += sampling_rng_1D_get(SAMPLING_CLOSURE);

  float3 rgb_moment = float3(0.0f);
  float3 radiance_accum = float3(0.0f);
  float weight_accum = 0.0f;
  float closest_hit_time = 1.0e10f;

  for (uint i = 0u; i < sample_count; i++) {
    float2 offset_f = (fract(hammersley_2d(i, sample_count) + noise) - 0.5f) * filter_size;
    int2 offset = int2(floor(offset_f + 0.5f));
    int2 sample_texel = texel + offset;

    float4 ray_data = imageLoad(ray_data_img, sample_texel);
    float ray_time = imageLoad(ray_time_img, sample_texel).r;
    float4 ray_radiance = imageLoad(ray_radiance_img, sample_texel);

    float3 ray_direction = ray_data.xyz;
    float ray_pdf_inv = abs(ray_data.w);
    /* Skip invalid pixels. */
    if (ray_pdf_inv == 0.0f) {
      continue;
    }

    closest_hit_time = min(closest_hit_time, ray_time);

    /* Slide 54. */
    /* The reference is wrong.
     * The ratio estimator is `pdf_local / pdf_ray` instead of `bsdf_local / pdf_ray`. */
    float pdf = closure_evaluate_pdf(closure, ray_direction, V, thickness);
    float weight = pdf * ray_pdf_inv;

    radiance_accum += ray_radiance.rgb * weight;
    weight_accum += weight;

    rgb_moment += square(ray_radiance.rgb) * weight;
  }
  float inv_weight = safe_rcp(weight_accum);

  radiance_accum *= inv_weight;
  /* Use radiance sum as signal mean. */
  float3 rgb_mean = radiance_accum;
  rgb_moment *= inv_weight;

  float3 rgb_variance = abs(rgb_moment - square(rgb_mean));
  float hit_variance = reduce_max(rgb_variance);

  float depth = reverse_z::read(texelFetch(depth_tx, texel_fullres, 0).r);
  float scene_z = drw_depth_screen_to_view(depth);
  float hit_depth = drw_depth_view_to_screen(scene_z - closest_hit_time);

  imageStoreFast(out_radiance_img, texel_fullres, float4(radiance_accum, 0.0f));
  imageStoreFast(out_variance_img, texel_fullres, float4(hit_variance));
  imageStoreFast(out_hit_depth_img, texel_fullres, float4(hit_depth));
}
