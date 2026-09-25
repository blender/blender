/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_pipeline.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_reduce.bsl.hh"

/* The Closure type is never used. Use float as dummy type. */
#define Closure float
#define CLOSURE_DEFAULT 0.0f

template<typename T> struct Reservoir {
  T data;
  float rand;
  float chosen_weight;
  float total_weight;

  void reset(float rng)
  {
    this->rand = rng;
    this->chosen_weight = 0.0f;
    this->total_weight = 0.0f;
    data = T{};
  }

  void add(T candidate, float candidate_weight)
  {
    if (candidate_weight < 1e-5f) {
      return;
    }
    total_weight += candidate_weight;

    float x = candidate_weight / total_weight;
    bool chosen = (rand < x);

    if (chosen) {
      data = candidate;
      chosen_weight = candidate_weight;
    }
    /* Assuming that if r is in the interval [0,x] or [x,1], it's still uniformly distributed
     * within that interval, so remapping to [0,1] again to explore this space of probability. */
    rand = (chosen) ? (rand / x) : ((rand - x) / (1.0f - x));
  }

  float get_final_weight() const
  {
    if (chosen_weight <= 0.0f) {
      return 0.0f;
    }
    return total_weight / chosen_weight;
  }
};

template struct Reservoir<ClosureUndetermined>;

ClosureType closure_type_get(ClosureDiffuse /*cl*/)
{
  return CLOSURE_BSDF_DIFFUSE_ID;
}

ClosureType closure_type_get(ClosureTranslucent /*cl*/)
{
  return CLOSURE_BSDF_TRANSLUCENT_ID;
}

ClosureType closure_type_get(ClosureReflection /*cl*/)
{
  return CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID;
}

ClosureType closure_type_get(ClosureRefraction /*cl*/)
{
  return CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
}

ClosureType closure_type_get(ClosureSubsurface /*cl*/)
{
  return CLOSURE_BSSRDF_BURLEY_ID;
}

ClosureType closure_type_get(ClosureThinRefraction /*cl*/)
{
  return CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID;
}

/**
 * Assign `candidate` to `destination` based on a random value and the respective weights.
 */
void closure_select(Reservoir<ClosureUndetermined> &reservoir, ClosureUndetermined candidate)
{
  reservoir.add(candidate, candidate.weight());
}
