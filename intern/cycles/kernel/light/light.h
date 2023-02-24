/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/light/area.h"
#include "kernel/light/background.h"
#include "kernel/light/distant.h"
#include "kernel/light/point.h"
#include "kernel/light/spot.h"
#include "kernel/light/triangle.h"

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

/* Light info. */

ccl_device_inline bool light_select_reached_max_bounces(KernelGlobals kg, int index, int bounce)
{
  return (bounce > kernel_data_fetch(lights, index).max_bounces);
}

/* Sample point on an individual light. */

template<bool in_volume_segment>
ccl_device_inline bool light_sample(KernelGlobals kg,
                                    const int lamp,
                                    const float randu,
                                    const float randv,
                                    const float3 P,
                                    const uint32_t path_flag,
                                    ccl_private LightSample *ls)
{
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);
  if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
    if (klight->shader_id & SHADER_EXCLUDE_SHADOW_CATCHER) {
      return false;
    }
  }

  LightType type = (LightType)klight->type;
  ls->type = type;
  ls->shader = klight->shader_id;
  ls->object = PRIM_NONE;
  ls->prim = PRIM_NONE;
  ls->lamp = lamp;
  ls->u = randu;
  ls->v = randv;
  ls->group = lamp_lightgroup(kg, lamp);

  if (in_volume_segment && (type == LIGHT_DISTANT || type == LIGHT_BACKGROUND)) {
    /* Distant lights in a volume get a dummy sample, position will not actually
     * be used in that case. Only when sampling from a specific scatter position
     * do we actually need to evaluate these. */
    ls->P = zero_float3();
    ls->Ng = zero_float3();
    ls->D = zero_float3();
    ls->pdf = 1.0f;
    ls->eval_fac = 0.0f;
    ls->t = FLT_MAX;
    return true;
  }

  if (type == LIGHT_DISTANT) {
    if (!distant_light_sample(klight, randu, randv, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_BACKGROUND) {
    /* infinite area light (e.g. light dome or env light) */
    float3 D = -background_light_sample(kg, P, randu, randv, &ls->pdf);

    ls->P = D;
    ls->Ng = D;
    ls->D = -D;
    ls->t = FLT_MAX;
    ls->eval_fac = 1.0f;
  }
  else if (type == LIGHT_SPOT) {
    if (!spot_light_sample<in_volume_segment>(klight, randu, randv, P, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_POINT) {
    if (!point_light_sample<in_volume_segment>(klight, randu, randv, P, ls)) {
      return false;
    }
  }
  else {
    /* area light */
    if (!area_light_sample<in_volume_segment>(klight, randu, randv, P, ls)) {
      return false;
    }
  }

  return in_volume_segment || (ls->pdf > 0.0f);
}

/* Sample a point on the chosen emitter. */

template<bool in_volume_segment>
ccl_device_noinline bool light_sample(KernelGlobals kg,
                                      const float randu,
                                      const float randv,
                                      const float time,
                                      const float3 P,
                                      const int bounce,
                                      const uint32_t path_flag,
                                      const int emitter_index,
                                      const float pdf_selection,
                                      ccl_private LightSample *ls)
{
  int prim;
  MeshLight mesh_light;
#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    ccl_global const KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                           emitter_index);
    prim = kemitter->prim;
    mesh_light = kemitter->mesh_light;
  }
  else
#endif
  {
    ccl_global const KernelLightDistribution *kdistribution = &kernel_data_fetch(
        light_distribution, emitter_index);
    prim = kdistribution->prim;
    mesh_light = kdistribution->mesh_light;
  }

  /* A different value would be assigned in `triangle_light_sample()` if `!use_light_tree`. */
  ls->pdf_selection = pdf_selection;

  if (prim >= 0) {
    /* Mesh light. */
    const int object = mesh_light.object_id;

    /* Exclude synthetic meshes from shadow catcher pass. */
    if ((path_flag & PATH_RAY_SHADOW_CATCHER_PASS) &&
        !(kernel_data_fetch(object_flag, object) & SD_OBJECT_SHADOW_CATCHER)) {
      return false;
    }

    const int shader_flag = mesh_light.shader_flag;
    if (!triangle_light_sample<in_volume_segment>(kg, prim, object, randu, randv, time, ls, P)) {
      return false;
    }
    ls->shader |= shader_flag;
  }
  else {
    if (UNLIKELY(light_select_reached_max_bounces(kg, ~prim, bounce))) {
      return false;
    }

    if (!light_sample<in_volume_segment>(kg, ~prim, randu, randv, P, path_flag, ls)) {
      return false;
    }
  }

  ls->pdf *= ls->pdf_selection;
  return in_volume_segment || (ls->pdf > 0.0f);
}

/* Intersect ray with individual light. */

ccl_device bool lights_intersect(KernelGlobals kg,
                                 IntegratorState state,
                                 ccl_private const Ray *ccl_restrict ray,
                                 ccl_private Intersection *ccl_restrict isect,
                                 const int last_prim,
                                 const int last_object,
                                 const int last_type,
                                 const uint32_t path_flag)
{
  for (int lamp = 0; lamp < kernel_data.integrator.num_lights; lamp++) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);

    if (path_flag & PATH_RAY_CAMERA) {
      if (klight->shader_id & SHADER_EXCLUDE_CAMERA) {
        continue;
      }
    }
    else {
      if (!(klight->shader_id & SHADER_USE_MIS)) {
        continue;
      }

#ifdef __MNEE__
      /* This path should have been resolved with mnee, it will
       * generate a firefly for small lights since it is improbable. */
      if ((INTEGRATOR_STATE(state, path, mnee) & PATH_MNEE_CULL_LIGHT_CONNECTION) &&
          klight->use_caustics) {
        continue;
      }
#endif
    }

    if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
      if (klight->shader_id & SHADER_EXCLUDE_SHADOW_CATCHER) {
        continue;
      }
    }

    LightType type = (LightType)klight->type;
    float t = 0.0f, u = 0.0f, v = 0.0f;

    if (type == LIGHT_SPOT) {
      if (!spot_light_intersect(klight, ray, &t)) {
        continue;
      }
    }
    else if (type == LIGHT_POINT) {
      if (!point_light_intersect(klight, ray, &t)) {
        continue;
      }
    }
    else if (type == LIGHT_AREA) {
      if (!area_light_intersect(klight, ray, &t, &u, &v)) {
        continue;
      }
    }
    else {
      continue;
    }

    if (t < isect->t &&
        !(last_prim == lamp && last_object == OBJECT_NONE && last_type == PRIMITIVE_LAMP)) {
      isect->t = t;
      isect->u = u;
      isect->v = v;
      isect->type = PRIMITIVE_LAMP;
      isect->prim = lamp;
      isect->object = OBJECT_NONE;
    }
  }

  return isect->prim != PRIM_NONE;
}

/* Setup light sample from intersection. */

ccl_device bool light_sample_from_intersection(KernelGlobals kg,
                                               ccl_private const Intersection *ccl_restrict isect,
                                               const float3 ray_P,
                                               const float3 ray_D,
                                               ccl_private LightSample *ccl_restrict ls)
{
  const int lamp = isect->prim;
  ccl_global const KernelLight *klight = &kernel_data_fetch(lights, lamp);
  LightType type = (LightType)klight->type;
  ls->type = type;
  ls->shader = klight->shader_id;
  ls->object = isect->object;
  ls->prim = isect->prim;
  ls->lamp = lamp;
  /* todo: missing texture coordinates */
  ls->t = isect->t;
  ls->P = ray_P + ray_D * ls->t;
  ls->D = ray_D;
  ls->group = lamp_lightgroup(kg, lamp);

  if (type == LIGHT_SPOT) {
    if (!spot_light_sample_from_intersection(klight, isect, ray_P, ray_D, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_POINT) {
    if (!point_light_sample_from_intersection(klight, isect, ray_P, ray_D, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_AREA) {
    if (!area_light_sample_from_intersection(klight, isect, ray_P, ray_D, ls)) {
      return false;
    }
  }
  else {
    kernel_assert(!"Invalid lamp type in light_sample_from_intersection");
    return false;
  }

  return true;
}

/* Update light sample for changed new position, for MNEE. */

ccl_device_forceinline void light_update_position(KernelGlobals kg,
                                                  ccl_private LightSample *ls,
                                                  const float3 P)
{
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ls->lamp);

  if (ls->type == LIGHT_POINT) {
    point_light_update_position(klight, ls, P);
  }
  else if (ls->type == LIGHT_SPOT) {
    spot_light_update_position(klight, ls, P);
  }
  else if (ls->type == LIGHT_AREA) {
    area_light_update_position(klight, ls, P);
  }
}

CCL_NAMESPACE_END
