/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Does not use any tracing method. Only rely on local light probes to get the incoming radiance.
 */

#include "infos/eevee_tracing_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_ray_trace_fallback)

#include "eevee_bxdf_sampling_lib.glsl"
#include "eevee_colorspace_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_lightprobe_eval_lib.glsl"
#include "eevee_ray_trace_screen_lib.glsl"
#include "eevee_ray_types_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"

void main()
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  int2 texel = int2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  int2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                       uniform_buf.raytrace.resolution_bias;

  /* Check if texel is out of bounds,
   * so we can utilize fast texture functions and early-out if not. */
  if (any(greaterThanEqual(texel, imageSize(ray_time_img).xy))) {
    return;
  }

  float depth = reverse_z::read(texelFetch(depth_tx, texel_fullres, 0).r);
  float2 uv = (float2(texel_fullres) + 0.5f) * uniform_buf.raytrace.full_resolution_inv;

  float4 ray_data_im = imageLoadFast(ray_data_img, texel);
  float ray_pdf_inv = ray_data_im.w;

  if (ray_pdf_inv == 0.0f) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStoreFast(ray_time_img, texel, float4(0.0f));
    imageStoreFast(ray_radiance_img, texel, float4(0.0f));
    return;
  }

  float3 P = drw_point_screen_to_world(float3(uv, depth));
  float3 V = drw_world_incident_vector(P);

  Ray ray;
  ray.origin = P;
  ray.direction = ray_data_im.xyz;

  /* Only closure 0 can be a transmission closure. */
  if (closure_index == 0) {
    const gbuffer::Header gbuf_header = gbuffer::read_header(texel_fullres);
    float thickness = gbuffer::read_thickness(gbuf_header, texel_fullres);
    if (thickness != 0.0f) {
      ClosureUndetermined cl = gbuffer::read_bin(texel_fullres, closure_index);
      ray = raytrace_thickness_ray_amend(ray, cl, V, thickness);
    }
  }

  /* Using ray direction as geometric normal to bias the sampling position.
   * This is faster than loading the gbuffer again and averages between reflected and normal
   * direction over many rays. */
  float3 Ng = ray.direction;
  LightProbeSample samp = lightprobe_load(ray.origin, Ng, V);
  /* Clamp SH to have parity with forward evaluation. */
  float clamp_indirect = uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect);

  float3 radiance = lightprobe_eval_direction(samp, ray.origin, ray.direction, ray_pdf_inv);
  /* Set point really far for correct reprojection of background. */
  float hit_time = 1000.0f;

  radiance = colorspace_brightness_clamp_max(radiance, uniform_buf.clamp.surface_indirect);

  imageStoreFast(ray_time_img, texel, float4(hit_time));
  imageStoreFast(ray_radiance_img, texel, float4(radiance, 0.0f));
}
