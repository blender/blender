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

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  float depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec2 uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  vec4 ray_data = imageLoad(ray_data_img, texel);
  float ray_pdf_inv = ray_data.w;

  if (ray_pdf_inv == 0.0) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStore(ray_time_img, texel, vec4(0.0));
    imageStore(ray_radiance_img, texel, vec4(0.0));
    return;
  }

  vec3 P = drw_point_screen_to_world(vec3(uv, depth));
  vec3 V = drw_world_incident_vector(P);

  Ray ray;
  ray.origin = P;
  ray.direction = ray_data.xyz;

  /* Only closure 0 can be a transmission closure. */
  if (closure_index == 0) {
    uint gbuf_header = texelFetch(gbuf_header_tx, texel_fullres, 0).r;
    float thickness = gbuffer_read_thickness(gbuf_header, gbuf_normal_tx, texel_fullres);
    if (thickness > 0.0) {
      vec3 surface_N = gbuffer_read_normal(gbuf_normal_tx, texel_fullres);
      ClosureType cl_type = gbuffer_closure_type_get_by_bin(gbuf_header, closure_index);
      ray = raytrace_thickness_ray_ammend(ray, cl_type, surface_N, thickness);
    }
  }

  /* Using ray direction as geometric normal to bias the sampling position.
   * This is faster than loading the gbuffer again and averages between reflected and normal
   * direction over many rays. */
  vec3 Ng = ray.direction;
  LightProbeSample samp = lightprobe_load(ray.origin, Ng, V);
  vec3 radiance = lightprobe_eval_direction(
      samp, ray.origin, ray.direction, safe_rcp(ray_pdf_inv));
  /* Set point really far for correct reprojection of background. */
  float hit_time = 1000.0;

  radiance = colorspace_brightness_clamp_max(radiance, uniform_buf.raytrace.brightness_clamp);

  imageStore(ray_time_img, texel, vec4(hit_time));
  imageStore(ray_radiance_img, texel, vec4(radiance, 0.0));
}
