/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/light/light.h"
#include "kernel/light/triangle.h"

CCL_NAMESPACE_BEGIN

/* Simple CDF based sampling over all lights in the scene, without taking into
 * account shading position or normal. */

ccl_device int light_distribution_sample(KernelGlobals kg, const float randn)
{
  /* This is basically std::upper_bound as used by PBRT, to find a point light or
   * triangle to emit from, proportional to area. a good improvement would be to
   * also sample proportional to power, though it's not so well defined with
   * arbitrary shaders. */
  int first = 0;
  int len = kernel_data.integrator.num_distribution + 1;
  float r = randn;

  do {
    int half_len = len >> 1;
    int middle = first + half_len;

    if (r < kernel_data_fetch(light_distribution, middle).totarea) {
      len = half_len;
    }
    else {
      first = middle + 1;
      len = len - half_len - 1;
    }
  } while (len > 0);

  /* Clamping should not be needed but float rounding errors seem to
   * make this fail on rare occasions. */
  int index = clamp(first - 1, 0, kernel_data.integrator.num_distribution - 1);

  return index;
}

template<bool in_volume_segment>
ccl_device_noinline bool light_distribution_sample(KernelGlobals kg,
                                                   const float randn,
                                                   const float randu,
                                                   const float randv,
                                                   const float time,
                                                   const float3 P,
                                                   const int bounce,
                                                   const uint32_t path_flag,
                                                   ccl_private LightSample *ls)
{
  /* Sample light index from distribution. */
  const int index = light_distribution_sample(kg, randn);
  const float pdf_selection = kernel_data.integrator.distribution_pdf_lights;
  return light_sample<in_volume_segment>(
      kg, randu, randv, time, P, bounce, path_flag, index, pdf_selection, ls);
}

ccl_device_inline float light_distribution_pdf_lamp(KernelGlobals kg)
{
  return kernel_data.integrator.distribution_pdf_lights;
}

CCL_NAMESPACE_END
