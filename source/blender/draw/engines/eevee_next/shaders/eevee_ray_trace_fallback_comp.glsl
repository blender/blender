/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Does not use any tracing method. Only rely on local light probes to get the incoming radiance.
 */

#pragma BLENDER_REQUIRE(eevee_reflection_probe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_trace_screen_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)

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

  vec3 P = get_world_space_from_depth(uv, depth);
  Ray ray;
  ray.origin = P;
  ray.direction = ray_data.xyz;

  int closest_probe_id = reflection_probes_find_closest(P);
  ReflectionProbeData probe = reflection_probe_buf[closest_probe_id];
  vec3 radiance = reflection_probes_sample(ray.direction, 0.0, probe).rgb;
  /* Set point really far for correct reprojection of background. */
  /* TODO(fclem): Could use probe depth / parallax. */
  float hit_time = 10000.0;

  float luma = max(1e-8, max_v3(radiance));
  radiance *= 1.0 - max(0.0, luma - uniform_buf.raytrace.brightness_clamp) / luma;

  imageStore(ray_time_img, texel, vec4(hit_time));
  imageStore(ray_radiance_img, texel, vec4(radiance, 0.0));
}
