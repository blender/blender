/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(draw_view)

#include "draw_view_lib.glsl"
#include "eevee_bxdf.bsl.hh"
#include "eevee_gbuffer_read.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_uniform.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"

namespace eevee {

/* Returns view-space ray. */
BsdfSample ray_generate_direction(float2 noise,
                                  ClosureUndetermined cl,
                                  float3 V,
                                  Thickness thickness)
{
  float3 random_point_on_cylinder = sample_cylinder(noise);
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  constexpr float rng_bias = 0.08f;
  /* When modeling object thickness as a sphere, the outgoing rays are distributed uniformly
   * over the sphere. We don't want the RAY_BIAS in this case. */
  if (cl.type != CLOSURE_BSDF_TRANSLUCENT_ID || thickness.mode() == ThicknessMode::Slab) {
    random_point_on_cylinder.x = 1.0f - random_point_on_cylinder.x * (1.0f - rng_bias);
  }

  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      bxdf_ggx_context_amend_transmission(cl, V, thickness);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
    case CLOSURE_BSDF_TRANSLUCENT_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      break;
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }

  float3x3 tangent_to_world = from_up_axis(cl.N);

  BsdfSample samp;
  samp.pdf = 0.0f;
  samp.direction = float3(0.0f);
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      samp = bxdf_translucent_sample(random_point_on_cylinder, thickness);
      break;
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      samp = bxdf_diffuse_sample(random_point_on_cylinder);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID: {
      samp = bxdf_ggx_sample_reflection(random_point_on_cylinder,
                                        V * tangent_to_world,
                                        square(to_closure_reflection(cl).roughness),
                                        true);
      break;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      samp = bxdf_ggx_sample_refraction(random_point_on_cylinder,
                                        V * tangent_to_world,
                                        square(to_closure_refraction(cl).roughness),
                                        to_closure_refraction(cl).ior,
                                        thickness,
                                        true);
      break;
    }
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID: {
      samp = bxdf_ggx_sample_reflection(random_point_on_cylinder,
                                        V * tangent_to_world,
                                        square(to_closure_thin_refraction(cl).roughness),
                                        true);
      samp.direction.z = -samp.direction.z;
      break;
    }
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }
  samp.direction = tangent_to_world * float3(samp.direction);

  return samp;
}

}  // namespace eevee

namespace eevee::raytracing {

struct RayGenerate {
  [[specialization_constant(0)]] int closure_index;
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[storage(4, read)]] const uint (&tiles_coord_buf)[];
  [[image(0, write, SFLOAT_16_16_16_16)]] image2D out_ray_data_img;
};

/**
 * Generate Ray direction along with other data that are then used
 * by the next pass to trace the rays.
 */
[[compute, local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void generate_rays([[resource_table]] RayGenerate &srt,
                   [[resource_table]] const Uniform &uni,
                   [[resource_table]] const Sampling &sampling,
                   [[resource_table]] const UtilityTexture &util_tx,
                   [[resource_table]] const gbuffer::Reader &reader,
                   [[work_group_id]] const uint3 group_id,
                   [[local_invocation_id]] const uint3 local_id)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(srt.tiles_coord_buf[group_id.x]);
  int2 texel = int2(local_id.xy + tile_coord * tile_size);

  int2 texel_fullres = texel * uni.raytrace_buf.trace_pixel_scale +
                       uni.raytrace_buf.trace_pixel_offset;

  gbuffer::Header gbuf_header = reader.read_header(texel_fullres);
  ClosureUndetermined closure = reader.read_bin(texel_fullres, srt.closure_index);

  if (closure.type == CLOSURE_NONE_ID) {
    imageStore(srt.out_ray_data_img, texel, float4(0.0f));
    return;
  }

  float2 uv = (float2(texel_fullres) + 0.5f) / float2(textureSize(reader.gbuf_header_tx, 0).xy);
  float3 P = drw_point_screen_to_world(float3(uv, 0.5f));
  float3 V = drw_world_incident_vector(P);
  float2 noise = util_tx.fetch(float2(texel), UTIL_BLUE_NOISE_LAYER).rg;
  noise = fract(noise + sampling.rng_2D_get(SAMPLING_RAYTRACE_U));

  Thickness thickness = reader.read_thickness(gbuf_header, texel_fullres);

  BsdfSample samp = ray_generate_direction(noise.xy, closure, V, thickness);

  /* Store inverse pdf to speedup denoising.
   * Limit to the smallest non-0 value that the format can encode.
   * Strangely it does not correspond to the IEEE spec. */
  float inv_pdf = (samp.pdf == 0.0f) ? 0.0f : max(6e-8f, 1.0f / samp.pdf);
  imageStoreFast(srt.out_ray_data_img, texel, float4(samp.direction, inv_pdf));
}

}  // namespace eevee::raytracing

PipelineCompute eevee_ray_generate(eevee::raytracing::generate_rays);
