/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_uniform_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)
SHADER_LIBRARY_CREATE_INFO(eevee_utility_texture)

#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"

packed_float3 g_emission;
packed_float3 g_transmittance;
float g_holdout;

packed_float3 g_volume_scattering;
float g_volume_anisotropy;
packed_float3 g_volume_absorption;

/* The Closure type is never used. Use float as dummy type. */
#define Closure float
#define CLOSURE_DEFAULT 0.0f

/* Maximum number of picked closure. */
#ifndef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT 1
#endif
/* Sampled closure parameters. */
ClosureUndetermined g_closure_bins[CLOSURE_BIN_COUNT];
/* Random number per sampled closure type. */
float g_closure_rand[CLOSURE_BIN_COUNT];

ClosureUndetermined g_closure_get(uchar i)
{
  switch (i) {
    case 0:
      return g_closure_bins[0];
#if CLOSURE_BIN_COUNT > 1
    case 1:
      return g_closure_bins[1];
#endif
#if CLOSURE_BIN_COUNT > 2
    case 2:
      return g_closure_bins[2];
#endif
  }
  /* Unreachable. */
  assert(false);
  return g_closure_bins[0];
}

ClosureUndetermined g_closure_get_resolved(uchar i, float weight_fac)
{
  ClosureUndetermined cl = g_closure_get(i);
  cl.color *= cl.weight * weight_fac;
  return cl;
}

ClosureType closure_type_get(ClosureDiffuse cl)
{
  return CLOSURE_BSDF_DIFFUSE_ID;
}

ClosureType closure_type_get(ClosureTranslucent cl)
{
  return CLOSURE_BSDF_TRANSLUCENT_ID;
}

ClosureType closure_type_get(ClosureReflection cl)
{
  return CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
}

ClosureType closure_type_get(ClosureRefraction cl)
{
  return CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
}

ClosureType closure_type_get(ClosureSubsurface cl)
{
  return CLOSURE_BSSRDF_BURLEY_ID;
}

/**
 * Returns true if the closure is to be selected based on the input weight.
 */
bool closure_select_check(float weight, inout float total_weight, inout float r)
{
  if (weight < 1e-5f) {
    return false;
  }
  total_weight += weight;
  float x = weight / total_weight;
  bool chosen = (r < x);
  /* Assuming that if r is in the interval [0,x] or [x,1], it's still uniformly distributed within
   * that interval, so remapping to [0,1] again to explore this space of probability. */
  r = (chosen) ? (r / x) : ((r - x) / (1.0f - x));
  return chosen;
}

/**
 * Assign `candidate` to `destination` based on a random value and the respective weights.
 */
void closure_select(inout ClosureUndetermined destination,
                    inout float random,
                    ClosureUndetermined candidate)
{
  float candidate_color_weight = average(abs(candidate.color));
  if (closure_select_check(candidate.weight * candidate_color_weight, destination.weight, random))
  {
    float total_weight = destination.weight;
    destination = candidate;
    destination.color /= candidate_color_weight;
    destination.weight = total_weight;
  }
}

void closure_weights_reset(float closure_rand)
{
  g_closure_rand[0] = closure_rand;
  g_closure_bins[0].weight = 0.0f;
#if CLOSURE_BIN_COUNT > 1
  g_closure_rand[1] = closure_rand;
  g_closure_bins[1].weight = 0.0f;
#endif
#if CLOSURE_BIN_COUNT > 2
  g_closure_rand[2] = closure_rand;
  g_closure_bins[2].weight = 0.0f;
#endif

  g_volume_scattering = float3(0.0f);
  g_volume_anisotropy = 0.0f;
  g_volume_absorption = float3(0.0f);

  g_emission = float3(0.0f);
  g_transmittance = float3(0.0f);
  g_volume_scattering = float3(0.0f);
  g_volume_absorption = float3(0.0f);
  g_holdout = 0.0f;
}
