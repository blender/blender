/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Intersection and filtering functions for hardware ray-trace style of API.
 *
 * Filter functions are invoked for an intersection to give BVH traversal hints whether
 * traversal is to continue. Returning true from filter functions means the intersection is
 * filtered (ignored) and the traversal is to continue.
 *
 * Note on the template parameters
 * ===============================
 *
 * perform_intersection_tests controls whether checks that are typically are performed during
 * intersection are to be done in the filter function. Intersection checks that are done by the
 * hardware do not perform self-intersection and shadow-linking checks: they are done in the
 * filter function instead. However, if the intersection check uses custom function it performs
 * these checks early on, so skipping them in the filter function will lead to a better
 * performance. */

#pragma once

#include "kernel/bvh/util.h"
#include "kernel/globals.h"
#include "kernel/integrator/state.h"
#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

/* Special tricks to subclass payload.
 * The issue here is Metal does not support subclassing, but HIP-RT had performance issues with
 * composition in the past (see !136823). */
#if defined(__KERNEL_HIPRT__)
#  define BVH_PAYLOAD_SUBCLASS(cls, base_cls) struct cls : base_cls
#  define BVH_PAYLOAD_SUBCLASS_DEFINE(base_cls)
#  define BVH_PAYLOAD_BASE(obj) (obj)
#else
#  define BVH_PAYLOAD_SUBCLASS(cls, base_cls) struct cls
#  define BVH_PAYLOAD_SUBCLASS_DEFINE(base_cls) base_cls base;
#  define BVH_PAYLOAD_BASE(obj) ((obj).base)
#endif

struct BVHPayload {
  /* In OptiX, self-intersection information and ray visibility are passed via Ray's pointer as
   * extra payload data. */
#if !defined(__KERNEL_OPTIX__)
  /* Primitives for the self-intersections. */
  RaySelfPrimitives ray_self;

  /* Ray visibility flags and time. */
  uint ray_visibility;
#endif

#if defined(__KERNEL_HIPRT__)
  float ray_time;
#endif
};

/* OptiX passes various parameters via registers to the tracing calls. No need to store duplicate
 * data for OptiX. This will essentially make it so BVHPayload contains data which is strictly
 * needed for intersection recording and for tracking curve transparency. */
#if defined(__KERNEL_OPTIX__)
#  define BVH_SHADOW_ALL_PAYLOAD_SUBCLASS(cls, base_cls) struct cls
#  define BVH_SHADOW_ALL_PAYLOAD_SUBCLASS_DEFINE(base_cls)
#else
#  define BVH_SHADOW_ALL_PAYLOAD_SUBCLASS(cls, base_cls) BVH_PAYLOAD_SUBCLASS(cls, base_cls)
#  define BVH_SHADOW_ALL_PAYLOAD_SUBCLASS_DEFINE(base_cls) BVH_PAYLOAD_SUBCLASS_DEFINE(base_cls)
#endif

BVH_SHADOW_ALL_PAYLOAD_SUBCLASS(BVHShadowAllPayload, BVHPayload)
{
  BVH_SHADOW_ALL_PAYLOAD_SUBCLASS_DEFINE(BVHPayload);

  /* Using uint16_t is slower on HIP, while it is similar performance but potentially lower memory
   * footprint on other backends. */
#if defined(__KERNEL_HIPRT__)
  using UIntType = uint;
#else
  using UIntType = uint16_t;
#endif

  IntegratorShadowState state;

  /* The maximum number of transparent intersections to consider: if there are more intersections
   * than this value, all light is considered blocked. */
  UIntType max_transparent_hits;
  /* The number of transparent intersections tested during BVH traversal. It might be higher than
   * the number of recorded intersections. */
  UIntType num_transparent_hits = 0;

  /* Maximum intersection distance t for intersections that are to be recorded.
   * If intersection's distance exceeds this value, it is not recoded. */
  float max_record_isect_t;

  /* An index within the shadow_isect array at which the next intersection will be recorded. */
  UIntType record_isect_index = 0;

  /* The number of intersections that has been attempted to be recorded.
   * It might be higher than the shadow_isect size, indicating that more invocations of the
   * intersection kernel are needed. It is different from the num_transparent_hits as it does not
   * include transparent curve intersections that are handled by accumulating throughput in the
   * filter function. */
  UIntType num_recorded_hits = 0;

  /* Accumulated throughput of transparent curve intersections.
   * Curves are using special optimization by baking their transparency and handling it in the
   * filter function. */
  float throughput = 1.0f;
};

/* Filter intersection with possibly transparent surface.
 *
 * Designed to be used from the any-hit type of traversal:
 * - If an opaque surface is hit, returns false, stopping traversal. The scene intersection
 *   function will consider the shadow ray to be blocked.
 * - If a transparent surface is hit, the intersection is recorded into the shadow_isect array in
 *   the state. The closest N intersections are recorded. */
template<bool perform_intersection_tests, uint enabled_primitive_types = PRIMITIVE_ALL>
ccl_device_forceinline bool bvh_shadow_all_anyhit_filter(
    KernelGlobals kg,
    IntegratorShadowState state,
    ccl_ray_data BVHShadowAllPayload &ccl_restrict payload,
    const ccl_ray_data RaySelfPrimitives &ccl_restrict ray_self,
    const uint ray_visibility,
    const Intersection isect)

{
  if constexpr (perform_intersection_tests) {
#if defined(__VISIBILITY_FLAG__)
    if ((kernel_data_fetch(objects, isect.object).visibility & ray_visibility) == 0) {
      return true;
    }
#endif

#if defined(__SHADOW_LINKING__)
    if (intersection_skip_shadow_link(kg, ray_self, isect.object)) {
      return true;
    }
#endif

    if (intersection_skip_self_shadow(ray_self, isect.object, isect.prim)) {
      return true;
    }
  }

#if !defined(__TRANSPARENT_SHADOWS__)
  /* No transparent shadows in the scene, all light is blocked and we can stop immediately. */
  payload.throughput = 0.0f;
  return false;
#else
  /* Detect if this surface has a shader with transparent shadows. */
  /* TODO: optimize so primitive visibility flag indicates if the primitive has a transparent
   * shadow shader? */
  const int shader_flags = intersection_get_shader_flags(kg, isect.prim, isect.type);
  if ((shader_flags & SD_HAS_TRANSPARENT_SHADOW) == 0) {
    /* No transparent shadows for the shader, all light is blocked, and we can stop immediately. */
    payload.throughput = 0.0f;
    return false;
  }

  /* Fetch commonly accessed payload data, ensuring that it is used from either register to a
   * stack, without going to the global memory. */
  uint num_recorded_hits = payload.num_recorded_hits;

  /* If the intersection is already recorded, ignore it completely: don't update throughput as it
   * has already been updated. But also don't count it for num_hits as that could result in a
   * situation when the same ray will be considered transparent when spatial split is off and be
   * opaque when spatial split is on. Since curves do not record intersections, there is
   * a possibility for optimization:
   * - Don't compile this code if the filter is only used for curve primitives.
   * - Don't run the check if the current intersection comes from the curve, as it will not match
   *   any recorded intersection anyway.
   *
   * NOTE: Currently, spatial splits are not used with OptiX, so there is no need to check whether
   * the intersection has been already recorded. */
#  if !defined(__KERNEL_OPTIX__)
  if constexpr (enabled_primitive_types & (PRIMITIVE_ALL & ~PRIMITIVE_CURVE)) {
    if ((isect.type & PRIMITIVE_CURVE) == 0) {
      if (intersection_skip_shadow_already_recoded(
              state, isect.object, isect.prim, num_recorded_hits))
      {
        return true;
      }
    }
  }
#  endif

  /* Only count transparent bounces, volume bounds bounces are counted when shading. */
  payload.num_transparent_hits += !(shader_flags & SD_HAS_ONLY_VOLUME);
  if (payload.num_transparent_hits > payload.max_transparent_hits) {
    /* The maximum number of intersections has been reached, consider that all light has been
     * blocked. */
    payload.throughput = 0.0f;
    return false;
  }

#  if defined(__HAIR__)
  if constexpr (enabled_primitive_types & PRIMITIVE_CURVE) {
    /* Always use baked shadow transparency for curves. */
    if (isect.type & PRIMITIVE_CURVE) {
      payload.throughput *= intersection_curve_shadow_transparency(
          kg, isect.object, isect.prim, isect.type, isect.u);

      if (payload.throughput < CURVE_SHADOW_TRANSPARENCY_CUTOFF) {
        /* Light attenuated too much through the curve intersections, assume all light is blocked
         * and do early output. */
        payload.throughput = 0.0f;
        return false;
      }

      /* Don't record the intersection as the throughput has been already modified here.
       * Simply continue BVH traversal for other intersections. */
      return true;
    }
  }
#  endif

  /* If the filter function only handles curves, it is known for the fact that nothing is to be
   * recorded: curves accumulated baked transparency. Skip this code for a curve-only case. */
  if constexpr (enabled_primitive_types & (PRIMITIVE_ALL & ~PRIMITIVE_CURVE)) {
    /* Always increase the number of recorded hits, even beyond the maximum, so that we can detect
     * this and trace another ray if needed. */
    num_recorded_hits += 1;
    payload.num_recorded_hits = num_recorded_hits;

    constexpr uint max_record_hits = INTEGRATOR_SHADOW_ISECT_SIZE;
    if (num_recorded_hits <= max_record_hits || isect.t < payload.max_record_isect_t) {
      integrator_state_write_shadow_isect(state, &isect, payload.record_isect_index);

      if (num_recorded_hits >= max_record_hits) {
        /* If the maximum number of hits is reached, find the furthest intersection to replace it
         * with the next closer one. We want the N closest intersections. */
        uint record_isect_index = 0;
        float tmax_hits = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, t);
        for (uint i = 1; i < max_record_hits; ++i) {
          const float isect_t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, i, t);
          if (isect_t > tmax_hits) {
            record_isect_index = i;
            tmax_hits = isect_t;
          }
        }
        payload.max_record_isect_t = tmax_hits;
        payload.record_isect_index = record_isect_index;
      }
      else {
        payload.record_isect_index = num_recorded_hits;
      }
    }
  }

  return true;
#endif
}

/* Filter intersection to intersections with only primitives with volume shader.
 *
 * Expected to be called only on a triangle primitive. The caller is to filter out intersections
 * with non-triangle primitives.
 *
 * Returns false if the primitive is not to be filtered out (accepted), true if the primitive is to
 * be ignored. */
template<bool do_visibility_check = true>
ccl_device_forceinline bool bvh_volume_anyhit_triangle_filter(
    KernelGlobals kg,
    const int object,
    const int prim,
    const ccl_ray_data RaySelfPrimitives &ccl_restrict ray_self,
    const uint ray_visibility)
{
#ifdef __VISIBILITY_FLAG__
  if constexpr (do_visibility_check) {
    if ((kernel_data_fetch(objects, object).visibility & ray_visibility) == 0) {
      return true;
    }
  }
#endif

  if ((kernel_data_fetch(object_flag, object) & SD_OBJECT_HAS_VOLUME) == 0) {
    return true;
  }

  if (intersection_skip_self(ray_self, object, prim)) {
    return true;
  }

  const int shader = kernel_data_fetch(tri_shader, prim);
  const int shader_flag = kernel_data_fetch(shaders, (shader & SHADER_MASK)).flags;
  if (!(shader_flag & SD_HAS_VOLUME)) {
    return true;
  }

  return false;
}

CCL_NAMESPACE_END
