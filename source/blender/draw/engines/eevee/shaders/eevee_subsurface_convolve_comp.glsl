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

#include "infos/eevee_subsurface_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_subsurface_convolve)

#include "draw_view_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

#include "gpu_shader_math_angle_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_shared_exponent_lib.glsl"

struct SubSurfaceSample {
  float3 radiance;
  float depth;
  uint sss_id;
};

/* TODO(fclem): These need to be outside the check because of MSL backend glue.
 * This likely will contribute to register usage. Better get rid of if or make it working. */
shared float3 cached_radiance[SUBSURFACE_GROUP_SIZE][SUBSURFACE_GROUP_SIZE];
shared uint cached_sss_id[SUBSURFACE_GROUP_SIZE][SUBSURFACE_GROUP_SIZE];
shared float cached_depth[SUBSURFACE_GROUP_SIZE][SUBSURFACE_GROUP_SIZE];

void cache_populate(float2 local_uv)
{
  uint2 texel = gl_LocalInvocationID.xy;
  cached_radiance[texel.y][texel.x] = texture(radiance_tx, local_uv).rgb;
  cached_sss_id[texel.y][texel.x] = texture(object_id_tx, local_uv).r;
  cached_depth[texel.y][texel.x] = reverse_z::read(texture(depth_tx, local_uv).r);
}

bool cache_sample(uint2 texel, out SubSurfaceSample samp)
{
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  /* This can underflow and allow us to only do one upper bound check. */
  texel -= tile_coord * SUBSURFACE_GROUP_SIZE;
  if (any(greaterThanEqual(texel, uint2(SUBSURFACE_GROUP_SIZE)))) {
    return false;
  }
  samp.radiance = cached_radiance[texel.y][texel.x];
  samp.sss_id = cached_sss_id[texel.y][texel.x];
  samp.depth = cached_depth[texel.y][texel.x];
  return true;
}

SubSurfaceSample sample_neighborhood(float2 sample_uv)
{
  SubSurfaceSample samp;
  uint2 sample_texel = uint2(sample_uv * float2(textureSize(depth_tx, 0)));
  if (cache_sample(sample_texel, samp)) {
    return samp;
  }
  samp.depth = reverse_z::read(texture(depth_tx, sample_uv).r);
  samp.sss_id = texture(object_id_tx, sample_uv).r;
  samp.radiance = texture(radiance_tx, sample_uv).rgb;
  return samp;
}

void main()
{
  constexpr uint tile_size = SUBSURFACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  int2 texel = int2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  float2 center_uv = (float2(texel) + 0.5f) / float2(textureSize(gbuf_header_tx, 0).xy);

  cache_populate(center_uv);
  barrier();

  float depth = reverse_z::read(texelFetch(depth_tx, texel, 0).r);
  float3 vP = drw_point_screen_to_view(float3(center_uv, depth));

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel);
  if (gbuf.layer[0].type != CLOSURE_BSSRDF_BURLEY_ID) {
    return;
  }

  const uint object_id = gbuffer::read_object_id(texel);

  const ClosureSubsurface closure = to_closure_subsurface(gbuf.layer[0]);
  float max_radius = reduce_max(closure.sss_radius);

  float homcoord = drw_view().winmat[2][3] * vP.z + drw_view().winmat[3][3];
  float2 sample_scale = float2(drw_view().winmat[0][0], drw_view().winmat[1][1]) *
                        (0.5f * max_radius / homcoord);

  float pixel_footprint = sample_scale.x * textureSize(depth_tx, 0).x;
  if (pixel_footprint <= 1.0f) {
    /* Early out, avoid divisions by zero. */
    return;
  }

  /* Avoid too small radii that have float imprecision. */
  float3 clamped_sss_radius = max(float3(uniform_buf.subsurface.min_radius),
                                  closure.sss_radius / max_radius) *
                              max_radius;
  /* Scale albedo because we can have HDR value caused by BSDF sampling. */
  float3 albedo = closure.color / max(1e-6f, reduce_max(closure.color));
  float3 d = burley_setup(clamped_sss_radius, albedo);

  /* Do not rotate too much to avoid too much cache misses. */
  float golden_angle = M_PI * (3.0f - sqrt(5.0f));
  float theta = interleaved_gradient_noise(float2(texel), 0, 0.0f) * golden_angle;

  float2x2 sample_space = from_scale(sample_scale) * from_rotation(AngleRadian(theta));

  float3 accum_weight = float3(0.0f);
  float3 accum_radiance = float3(0.0f);

  for (int i = 0; i < uniform_buf.subsurface.sample_len; i++) {
    float2 sample_uv = center_uv + sample_space * uniform_buf.subsurface.samples[i].xy;
    float pdf_inv = uniform_buf.subsurface.samples[i].z;

    SubSurfaceSample samp = sample_neighborhood(sample_uv);
    /* Reject radiance from other surfaces. Avoids light leak between objects. */
    if (samp.sss_id != object_id) {
      continue;
    }
    /* Slide 34. */
    float3 sample_vP = drw_point_screen_to_view(float3(sample_uv, samp.depth));
    float r = distance(sample_vP, vP);
    float3 weight = burley_eval(d, r) * pdf_inv;

    accum_radiance += samp.radiance * weight;
    accum_weight += weight;
  }
  /* Normalize the sum (slide 34). */
  accum_radiance *= safe_rcp(accum_weight);

  /* Put result in direct diffuse. */
  imageStoreFast(out_direct_light_img, texel, uint4(rgb9e5_encode(accum_radiance)));
  /* Clear the indirect pass since its content has been merged and convolved with direct light. */
  imageStoreFast(out_indirect_light_img, texel, float4(0.0f, 0.0f, 0.0f, 0.0f));
}
