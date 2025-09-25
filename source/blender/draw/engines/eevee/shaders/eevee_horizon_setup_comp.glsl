/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This pass reprojects the input radiance if needed, downsample it and output the matching normal.
 *
 * Dispatched as one thread for each trace resolution pixel.
 */

#include "infos/eevee_tracing_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_horizon_setup)

#include "draw_view_lib.glsl"
#include "eevee_colorspace_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 texel_fullres = texel * uniform_buf.raytrace.horizon_resolution_scale +
                       uniform_buf.raytrace.horizon_resolution_bias;

  /* Return early for padding threads so we can use imageStoreFast. */
  if (any(greaterThanEqual(texel, imageSize(out_radiance_img).xy))) {
    return;
  }

  /* Avoid loading texels outside texture range. */
  int2 extent = textureSize(gbuf_header_tx, 0).xy;
  texel_fullres = min(texel_fullres, extent - 1);

  /* Load Gbuffer. */
  const gbuffer::Layers gbuf = gbuffer::read_layers(texel_fullres);

  /* Export normal. */
  float3 N = gbuf.surface_N();
  /* Background has invalid data. */
  /* FIXME: This is zero for opaque layer when we are processing the refraction layer. */
  if (is_zero(N)) {
    /* Avoid NaN. But should be fixed in any case. */
    N = float3(1.0f, 0.0f, 0.0f);
  }
  float3 vN = drw_normal_world_to_view(N);
  /* Tag processed pixel in the normal buffer for denoising speed. */
  bool is_processed = !gbuf.header.is_empty();
  imageStore(out_normal_img, texel, float4(vN * 0.5f + 0.5f, float(is_processed)));

  /* Re-project radiance. */
  float2 uv = (float2(texel_fullres) + 0.5f) / float2(textureSize(depth_tx, 0).xy);
  float depth = reverse_z::read(texelFetch(depth_tx, texel_fullres, 0).r);
  float3 P = drw_point_screen_to_world(float3(uv, depth));

  float3 ssP_prev = drw_ndc_to_screen(project_point(uniform_buf.raytrace.radiance_persmat, P));

  float4 radiance = texture(in_radiance_tx, ssP_prev.xy);
  radiance = colorspace_brightness_clamp_max(radiance, uniform_buf.clamp.surface_indirect);

  imageStore(out_radiance_img, texel, radiance);
}
