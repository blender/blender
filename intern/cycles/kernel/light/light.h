/* SPDX-FileCopyrightText: 2010-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/integrator/state.h"

#include "kernel/light/area.h"
#include "kernel/light/background.h"
#include "kernel/light/distant.h"
#include "kernel/light/point.h"
#include "kernel/light/spot.h"
#include "kernel/light/triangle.h"
#include "kernel/sample/lcg.h"

CCL_NAMESPACE_BEGIN

/* Light info. */

ccl_device_inline bool light_select_reached_max_bounces(KernelGlobals kg,
                                                        const int index,
                                                        const int bounce)
{
  return (bounce > kernel_data_fetch(lights, index).max_bounces);
}

/* Light linking. */

ccl_device_inline int light_link_receiver_nee(KernelGlobals kg, const ccl_private ShaderData *sd)
{
#ifdef __LIGHT_LINKING__
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_LINKING)) {
    return OBJECT_NONE;
  }

  return sd->object;
#else
  return OBJECT_NONE;
#endif
}

ccl_device_inline int light_link_receiver_forward(KernelGlobals kg, IntegratorState state)
{
#ifdef __LIGHT_LINKING__
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_LINKING)) {
    return OBJECT_NONE;
  }

  return INTEGRATOR_STATE(state, path, mis_ray_object);
#else
  return OBJECT_NONE;
#endif
}

ccl_device_inline bool light_link_light_match(KernelGlobals kg,
                                              const int object_receiver,
                                              const int light_emitter)
{
#ifdef __LIGHT_LINKING__
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_LINKING)) {
    return true;
  }

  const uint64_t set_membership = kernel_data_fetch(lights, light_emitter).light_set_membership;
  const uint receiver_set = (object_receiver != OBJECT_NONE) ?
                                kernel_data_fetch(objects, object_receiver).receiver_light_set :
                                0;
  return ((uint64_t(1) << uint64_t(receiver_set)) & set_membership) != 0;
#else
  return true;
#endif
}

ccl_device_inline bool light_link_object_match(KernelGlobals kg,
                                               const int object_receiver,
                                               const int object_emitter)
{
#ifdef __LIGHT_LINKING__
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_LINKING)) {
    return true;
  }

  /* Emitter is OBJECT_NONE when the emitter is a world volume.
   * It is not explicitly linkable to any object, so assume it is coming from the default light
   * set which affects all objects in the scene. */
  if (object_emitter == OBJECT_NONE) {
    return true;
  }

  const uint64_t set_membership = kernel_data_fetch(objects, object_emitter).light_set_membership;
  const uint receiver_set = (object_receiver != OBJECT_NONE) ?
                                kernel_data_fetch(objects, object_receiver).receiver_light_set :
                                0;
  return ((uint64_t(1) << uint64_t(receiver_set)) & set_membership) != 0;
#else
  return true;
#endif
}

/* Sample point on an individual light. */

template<bool in_volume_segment>
ccl_device_inline bool light_sample(KernelGlobals kg,
                                    const int lamp,
                                    const float2 rand,
                                    const float3 P,
                                    const float3 N,
                                    const int shader_flags,
                                    const uint32_t path_flag,
                                    ccl_private LightSample *ls)
{
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);
  if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
    if (klight->shader_id & SHADER_EXCLUDE_SHADOW_CATCHER) {
      return false;
    }
  }

  const LightType type = (LightType)klight->type;
  ls->type = type;
  ls->shader = klight->shader_id;
  ls->object = PRIM_NONE;
  ls->prim = PRIM_NONE;
  ls->lamp = lamp;
  ls->u = rand.x;
  ls->v = rand.y;
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
    if (!distant_light_sample(klight, rand, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_BACKGROUND) {
    /* infinite area light (e.g. light dome or env light) */
    const float3 D = -background_light_sample(kg, P, rand, &ls->pdf);

    ls->P = D;
    ls->Ng = D;
    ls->D = -D;
    ls->t = FLT_MAX;
    ls->eval_fac = 1.0f;
  }
  else if (type == LIGHT_SPOT) {
    if (!spot_light_sample<in_volume_segment>(klight, rand, P, N, shader_flags, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_POINT) {
    if (!point_light_sample(klight, rand, P, N, shader_flags, ls)) {
      return false;
    }
  }
  else {
    /* area light */
    if (!area_light_sample<in_volume_segment>(klight, rand, P, ls)) {
      return false;
    }
  }

  return in_volume_segment || (ls->pdf > 0.0f);
}

/* Sample a point on the chosen emitter. */

template<bool in_volume_segment>
ccl_device_noinline bool light_sample(KernelGlobals kg,
                                      const float3 rand_light,
                                      const float time,
                                      const float3 P,
                                      const float3 N,
                                      const int object_receiver,
                                      const int shader_flags,
                                      const int bounce,
                                      const uint32_t path_flag,
                                      ccl_private LightSample *ls)
{
  /* The first two dimensions of the Sobol sequence have better stratification, use them to sample
   * position on the light. */
  const float2 rand = make_float2(rand_light);

  int prim;
  MeshLight mesh_light;
#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    const ccl_global KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                           ls->emitter_id);
    prim = kemitter->light.id;
    mesh_light.shader_flag = kemitter->mesh_light.shader_flag;
    mesh_light.object_id = ls->object;
  }
  else
#endif
  {
    const ccl_global KernelLightDistribution *kdistribution = &kernel_data_fetch(
        light_distribution, ls->emitter_id);
    prim = kdistribution->prim;
    mesh_light = kdistribution->mesh_light;
  }

  if (prim >= 0) {
    /* Mesh light. */
    const int object = mesh_light.object_id;

    if (!light_link_object_match(kg, object_receiver, object)) {
      return false;
    }

    /* Exclude synthetic meshes from shadow catcher pass. */
    if ((path_flag & PATH_RAY_SHADOW_CATCHER_PASS) &&
        !(kernel_data_fetch(object_flag, object) & SD_OBJECT_SHADOW_CATCHER))
    {
      return false;
    }

    const int shader_flag = mesh_light.shader_flag;
    if (!triangle_light_sample<in_volume_segment>(kg, prim, object, rand, time, ls, P)) {
      return false;
    }
    ls->shader |= shader_flag;
  }
  else {
    const int light = ~prim;

    if (!light_link_light_match(kg, object_receiver, light)) {
      return false;
    }

    if (UNLIKELY(light_select_reached_max_bounces(kg, light, bounce))) {
      return false;
    }

    if (!light_sample<in_volume_segment>(kg, light, rand, P, N, shader_flags, path_flag, ls)) {
      return false;
    }
  }

  ls->pdf *= ls->pdf_selection;
  return in_volume_segment || (ls->pdf > 0.0f);
}

/* Intersect ray with individual light. */

/* Returns the total number of hits (the input num_hits plus the number of the new intersections).
 */
template<bool is_main_path>
ccl_device_forceinline int lights_intersect_impl(KernelGlobals kg,
                                                 const ccl_private Ray *ccl_restrict ray,
                                                 ccl_private Intersection *ccl_restrict isect,
                                                 const int last_prim,
                                                 const int last_object,
                                                 const int last_type,
                                                 const uint32_t path_flag,
                                                 const uint8_t path_mnee,
                                                 const int receiver_forward,
                                                 ccl_private uint *lcg_state,
                                                 int num_hits)
{
#ifdef __SHADOW_LINKING__
  const bool is_indirect_ray = !(path_flag & PATH_RAY_CAMERA);
#endif

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
      if ((path_mnee & PATH_MNEE_CULL_LIGHT_CONNECTION) && klight->use_caustics) {
        continue;
      }
#endif
    }

    if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
      if (klight->shader_id & SHADER_EXCLUDE_SHADOW_CATCHER) {
        continue;
      }
    }

#ifdef __SHADOW_LINKING__
    /* For the main path exclude shadow-linked lights if intersecting with an indirect light ray.
     * Those lights are handled via dedicated light intersect and shade kernels.
     * For the shadow path used for the dedicated light shading ignore all non-shadow-linked
     * lights. */
    if (kernel_data.kernel_features & KERNEL_FEATURE_SHADOW_LINKING) {
      if (is_main_path) {
        if (is_indirect_ray &&
            kernel_data_fetch(lights, lamp).shadow_set_membership != LIGHT_LINK_MASK_ALL)
        {
          continue;
        }
      }
      else if (kernel_data_fetch(lights, lamp).shadow_set_membership == LIGHT_LINK_MASK_ALL) {
        continue;
      }
    }
#endif

#ifdef __LIGHT_LINKING__
    /* Light linking. */
    if (!light_link_light_match(kg, receiver_forward, lamp) && !(path_flag & PATH_RAY_CAMERA)) {
      continue;
    }
#endif

    const LightType type = (LightType)klight->type;
    float t = 0.0f;
    float u = 0.0f;
    float v = 0.0f;

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
    else if (type == LIGHT_DISTANT) {
      if (is_main_path || ray->tmax != FLT_MAX) {
        continue;
      }
      if (!distant_light_intersect(klight, ray, &t, &u, &v)) {
        continue;
      }
    }
    else {
      continue;
    }

    /* Avoid self-intersections. */
    if (last_prim == lamp && last_object == OBJECT_NONE && last_type == PRIMITIVE_LAMP) {
      continue;
    }

    ++num_hits;

#ifdef __SHADOW_LINKING__
    if (!is_main_path) {
      /* The non-main rays are only raced by the dedicated light kernel, after the shadow linking
       * feature check. */
      kernel_assert(kernel_data.kernel_features & KERNEL_FEATURE_SHADOW_LINKING);

      if ((isect->prim != PRIM_NONE) && (lcg_step_float(lcg_state) > 1.0f / num_hits)) {
        continue;
      }
    }
    else
#endif
        if (t >= isect->t)
    {
      continue;
    }

    isect->t = t;
    isect->u = u;
    isect->v = v;
    isect->type = PRIMITIVE_LAMP;
    isect->prim = lamp;
    isect->object = OBJECT_NONE;
  }

  return num_hits;
}

/* Lights intersection for the main path.
 * Intersects spot, point, and area lights. */
ccl_device bool lights_intersect(KernelGlobals kg,
                                 IntegratorState state,
                                 const ccl_private Ray *ccl_restrict ray,
                                 ccl_private Intersection *ccl_restrict isect,
                                 const int last_prim,
                                 const int last_object,
                                 const int last_type,
                                 const uint32_t path_flag)
{
  const uint8_t path_mnee = INTEGRATOR_STATE(state, path, mnee);
  const int receiver_forward = light_link_receiver_forward(kg, state);

  lights_intersect_impl<true>(kg,
                              ray,
                              isect,
                              last_prim,
                              last_object,
                              last_type,
                              path_flag,
                              path_mnee,
                              receiver_forward,
                              nullptr,
                              0);

  return isect->prim != PRIM_NONE;
}

/* Lights intersection for the shadow linking.
 * Intersects spot, point, area, and distant lights.
 *
 * Returns the total number of hits (the input num_hits plus the number of the new intersections).
 */
ccl_device int lights_intersect_shadow_linked(KernelGlobals kg,
                                              const ccl_private Ray *ccl_restrict ray,
                                              ccl_private Intersection *ccl_restrict isect,
                                              const int last_prim,
                                              const int last_object,
                                              const int last_type,
                                              const uint32_t path_flag,
                                              const int receiver_forward,
                                              ccl_private uint *lcg_state,
                                              const int num_hits)
{
  return lights_intersect_impl<false>(kg,
                                      ray,
                                      isect,
                                      last_prim,
                                      last_object,
                                      last_type,
                                      path_flag,
                                      PATH_MNEE_NONE,
                                      receiver_forward,
                                      lcg_state,
                                      num_hits);
}

/* Setup light sample from intersection. */

ccl_device bool light_sample_from_intersection(KernelGlobals kg,
                                               const ccl_private Intersection *ccl_restrict isect,
                                               const float3 ray_P,
                                               const float3 ray_D,
                                               const float3 N,
                                               const uint32_t path_flag,
                                               ccl_private LightSample *ccl_restrict ls)
{
  const int lamp = isect->prim;
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);
  const LightType type = (LightType)klight->type;
  ls->type = type;
  ls->shader = klight->shader_id;
  ls->object = isect->object;
  ls->prim = isect->prim;
  ls->lamp = lamp;
  ls->t = isect->t;
  ls->P = ray_P + ray_D * ls->t;
  ls->D = ray_D;
  ls->group = lamp_lightgroup(kg, lamp);

  if (type == LIGHT_SPOT) {
    if (!spot_light_sample_from_intersection(klight, ray_P, ray_D, N, path_flag, ls)) {
      return false;
    }
  }
  else if (type == LIGHT_POINT) {
    if (!point_light_sample_from_intersection(klight, ray_P, ray_D, N, path_flag, ls)) {
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

CCL_NAMESPACE_END
