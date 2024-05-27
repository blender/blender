/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Does not use any tracing method. Only rely on local light probes to get the incoming radiance.
 */

#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_trace_screen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  /* Check if texel is out of bounds, so we can utilize fast texture funcs and early-out if not. */
  if (any(greaterThanEqual(texel, imageSize(ray_time_img).xy))) {
    return;
  }

  float depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec2 uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  vec4 ray_data_im = imageLoadFast(ray_data_img, texel);
  float ray_pdf_inv = ray_data_im.w;

  if (ray_pdf_inv == 0.0) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStoreFast(ray_time_img, texel, vec4(0.0));
    imageStoreFast(ray_radiance_img, texel, vec4(0.0));
    return;
  }

  vec3 P = drw_point_screen_to_world(vec3(uv, depth));
  vec3 V = drw_world_incident_vector(P);

  Ray ray;
  ray.origin = P;
  ray.direction = ray_data_im.xyz;

  /* Only closure 0 can be a transmission closure. */
  if (closure_index == 0) {
    uint gbuf_header = texelFetch(gbuf_header_tx, texel_fullres, 0).r;
    float thickness = gbuffer_read_thickness(gbuf_header, gbuf_normal_tx, texel_fullres);
    if (thickness != 0.0) {
      ClosureUndetermined cl = gbuffer_read_bin(
          gbuf_header, gbuf_closure_tx, gbuf_normal_tx, texel_fullres, closure_index);
      ray = raytrace_thickness_ray_ammend(ray, cl, V, thickness);
    }
  }

  /* Using ray direction as geometric normal to bias the sampling position.
   * This is faster than loading the gbuffer again and averages between reflected and normal
   * direction over many rays. */
  vec3 Ng = ray.direction;
  LightProbeSample samp = lightprobe_load(ray.origin, Ng, V);
  /* Clamp SH to have parity with forward evaluation. */
  float clamp_indirect = uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect);

  vec3 radiance = lightprobe_eval_direction(
      samp, ray.origin, ray.direction, safe_rcp(ray_pdf_inv));
  /* Set point really far for correct reprojection of background. */
  float hit_time = 1000.0;

  radiance = colorspace_brightness_clamp_max(radiance, uniform_buf.clamp.surface_indirect);

  imageStoreFast(ray_time_img, texel, vec4(hit_time));
  imageStoreFast(ray_radiance_img, texel, vec4(radiance, 0.0));
}
