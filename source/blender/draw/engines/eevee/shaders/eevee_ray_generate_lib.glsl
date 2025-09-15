/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Ray generation routines for each BSDF types.
 */

#include "eevee_bxdf_sampling_lib.glsl"
#include "eevee_ray_types_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_thickness_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

#include "gpu_shader_math_matrix_construct_lib.glsl"

/* Returns view-space ray. */
BsdfSample ray_generate_direction(float2 noise, ClosureUndetermined cl, float3 V, float thickness)
{
  float3 random_point_on_cylinder = sample_cylinder(noise);
  /* Bias the rays so we never get really high energy rays almost parallel to the surface. */
  constexpr float rng_bias = 0.08f;
  /* When modeling object thickness as a sphere, the outgoing rays are distributed uniformly
   * over the sphere. We don't want the RAY_BIAS in this case. */
  if (cl.type != CLOSURE_BSDF_TRANSLUCENT_ID || thickness <= 0.0f) {
    random_point_on_cylinder.x = 1.0f - random_point_on_cylinder.x * (1.0f - rng_bias);
  }

  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      bxdf_ggx_context_amend_transmission(cl, V, thickness);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
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
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }
  samp.direction = tangent_to_world * float3(samp.direction);

  return samp;
}
