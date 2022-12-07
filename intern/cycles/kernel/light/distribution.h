/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/light/light.h"
#include "kernel/light/triangle.h"

CCL_NAMESPACE_BEGIN

/* Simple CDF based sampling over all lights in the scene, without taking into
 * account shading position or normal. */

ccl_device int light_distribution_sample(KernelGlobals kg, ccl_private float *randu)
{
  /* This is basically std::upper_bound as used by PBRT, to find a point light or
   * triangle to emit from, proportional to area. a good improvement would be to
   * also sample proportional to power, though it's not so well defined with
   * arbitrary shaders. */
  int first = 0;
  int len = kernel_data.integrator.num_distribution + 1;
  float r = *randu;

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

  /* Rescale to reuse random number. this helps the 2D samples within
   * each area light be stratified as well. */
  float distr_min = kernel_data_fetch(light_distribution, index).totarea;
  float distr_max = kernel_data_fetch(light_distribution, index + 1).totarea;
  *randu = (r - distr_min) / (distr_max - distr_min);

  return index;
}

template<bool in_volume_segment>
ccl_device_noinline bool light_distribution_sample(KernelGlobals kg,
                                                   float randu,
                                                   const float randv,
                                                   const float time,
                                                   const float3 P,
                                                   const int bounce,
                                                   const uint32_t path_flag,
                                                   ccl_private LightSample *ls)
{
  /* Sample light index from distribution. */
  const int index = light_distribution_sample(kg, &randu);
  ccl_global const KernelLightDistribution *kdistribution = &kernel_data_fetch(light_distribution,
                                                                               index);
  const int prim = kdistribution->prim;

  if (prim >= 0) {
    /* Mesh light. */
    const int object = kdistribution->mesh_light.object_id;

    /* Exclude synthetic meshes from shadow catcher pass. */
    if ((path_flag & PATH_RAY_SHADOW_CATCHER_PASS) &&
        !(kernel_data_fetch(object_flag, object) & SD_OBJECT_SHADOW_CATCHER)) {
      return false;
    }

    const int shader_flag = kdistribution->mesh_light.shader_flag;
    triangle_light_sample<in_volume_segment>(kg, prim, object, randu, randv, time, ls, P);
    ls->shader |= shader_flag;
    return (ls->pdf > 0.0f);
  }

  const int lamp = -prim - 1;

  if (UNLIKELY(light_select_reached_max_bounces(kg, lamp, bounce))) {
    return false;
  }

  if (!light_sample<in_volume_segment>(kg, lamp, randu, randv, P, path_flag, ls)) {
    return false;
  }

  ls->pdf_selection = kernel_data.integrator.distribution_pdf_lights;
  ls->pdf *= ls->pdf_selection;

  return true;
}

ccl_device_inline float light_distribution_pdf_lamp(KernelGlobals kg)
{
  return kernel_data.integrator.distribution_pdf_lights;
}

CCL_NAMESPACE_END
