/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Process in screen space the diffuse radiance input to mimic subsurface transmission.
 *
 * This implementation follows the technique described in the SIGGRAPH presentation:
 * "Efficient screen space subsurface scattering SIGGRAPH 2018"
 * by Evgenii Golubev
 *
 * But, instead of having all the precomputed weights for all three color primaries,
 * we precompute a weight profile texture to be able to support per pixel AND per channel radius.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_rotation_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

shared vec3 cached_radiance[SUBSURFACE_GROUP_SIZE][SUBSURFACE_GROUP_SIZE];
shared uint cached_sss_id[SUBSURFACE_GROUP_SIZE][SUBSURFACE_GROUP_SIZE];
shared float cached_depth[SUBSURFACE_GROUP_SIZE][SUBSURFACE_GROUP_SIZE];

struct SubSurfaceSample {
  vec3 radiance;
  float depth;
  uint sss_id;
};

void cache_populate(vec2 local_uv)
{
  uvec2 texel = gl_LocalInvocationID.xy;
  cached_radiance[texel.y][texel.x] = texture(radiance_tx, local_uv).rgb;
  cached_sss_id[texel.y][texel.x] = texture(object_id_tx, local_uv).r;
  cached_depth[texel.y][texel.x] = texture(depth_tx, local_uv).r;
}

bool cache_sample(uvec2 texel, out SubSurfaceSample samp)
{
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  /* This can underflow and allow us to only do one upper bound check. */
  texel -= tile_coord * SUBSURFACE_GROUP_SIZE;
  if (any(greaterThanEqual(texel, uvec2(SUBSURFACE_GROUP_SIZE)))) {
    return false;
  }
  samp.radiance = cached_radiance[texel.y][texel.x];
  samp.sss_id = cached_sss_id[texel.y][texel.x];
  samp.depth = cached_depth[texel.y][texel.x];
  return true;
}

SubSurfaceSample sample_neighborhood(vec2 sample_uv)
{
  SubSurfaceSample samp;
  uvec2 sample_texel = uvec2(sample_uv * vec2(textureSize(depth_tx, 0)));
  if (cache_sample(sample_texel, samp)) {
    return samp;
  }
  samp.depth = texture(depth_tx, sample_uv).r;
  samp.sss_id = texture(object_id_tx, sample_uv).r;
  samp.radiance = texture(radiance_tx, sample_uv).rgb;
  return samp;
}

void main(void)
{
  const uint tile_size = SUBSURFACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  vec2 center_uv = (vec2(texel) + 0.5) / vec2(textureSize(gbuf_header_tx, 0));

  cache_populate(center_uv);

  float depth = texelFetch(depth_tx, texel, 0).r;
  vec3 vP = drw_point_screen_to_view(vec3(center_uv, depth));

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  if (gbuf.diffuse.sss_id == 0u) {
    return;
  }

  float max_radius = reduce_max(gbuf.diffuse.sss_radius);

  float homcoord = ProjectionMatrix[2][3] * vP.z + ProjectionMatrix[3][3];
  vec2 sample_scale = vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) *
                      (0.5 * max_radius / homcoord);

  float pixel_footprint = sample_scale.x * textureSize(depth_tx, 0).x;
  if (pixel_footprint <= 1.0) {
    /* Early out, avoid divisions by zero. */
    return;
  }

  /* Avoid too small radii that have float imprecision. */
  vec3 clamped_sss_radius = max(vec3(1e-4), gbuf.diffuse.sss_radius / max_radius) * max_radius;
  /* Scale albedo because we can have HDR value caused by BSDF sampling. */
  vec3 albedo = gbuf.diffuse.color / max(1e-6, reduce_max(gbuf.diffuse.color));
  vec3 d = burley_setup(clamped_sss_radius, albedo);

  /* Do not rotate too much to avoid too much cache misses. */
  float golden_angle = M_PI * (3.0 - sqrt(5.0));
  float theta = interlieved_gradient_noise(vec2(texel), 0, 0.0) * golden_angle;

  mat2 sample_space = from_scale(sample_scale) * from_rotation(Angle(theta));

  vec3 accum_weight = vec3(0.0);
  vec3 accum_radiance = vec3(0.0);

  for (int i = 0; i < uniform_buf.subsurface.sample_len; i++) {
    vec2 sample_uv = center_uv + sample_space * uniform_buf.subsurface.samples[i].xy;
    float pdf_inv = uniform_buf.subsurface.samples[i].z;

    SubSurfaceSample samp = sample_neighborhood(sample_uv);
    /* Reject radiance from other surfaces. Avoids light leak between objects. */
    if (samp.sss_id != gbuf.diffuse.sss_id) {
      continue;
    }
    /* Slide 34. */
    vec3 sample_vP = drw_point_screen_to_view(vec3(sample_uv, samp.depth));
    float r = distance(sample_vP, vP);
    vec3 weight = burley_eval(d, r) * pdf_inv;

    accum_radiance += samp.radiance * weight;
    accum_weight += weight;
  }
  /* Normalize the sum (slide 34). */
  accum_radiance *= safe_rcp(accum_weight);

  /* Put result in direct diffuse. */
  imageStore(out_direct_light_img, texel, vec4(accum_radiance, 0.0));
  /* Clear the indirect pass since its content has been merged and convolved with direct light. */
  imageStore(out_indirect_light_img, texel, vec4(0.0, 0.0, 0.0, 0.0));
}
