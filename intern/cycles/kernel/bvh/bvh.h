/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/bvh/types.h"
#include "kernel/bvh/util.h"

#include "kernel/integrator/state_util.h"

/* Device specific acceleration structures for ray tracing. */

#if defined(__EMBREE__)
#  include "kernel/device/cpu/bvh.h"
#  define __BVH2__
#elif defined(__METALRT__)
#  include "kernel/device/metal/bvh.h"
#elif defined(__KERNEL_OPTIX__)
#  include "kernel/device/optix/bvh.h"
#elif defined(__HIPRT__)
#  include "kernel/device/hiprt/bvh.h"
#else
#  define __BVH2__
#endif

#if defined(__KERNEL_ONEAPI__) && defined(WITH_EMBREE_GPU)
/* bool is apparently not tested for specialization constants:
 * https://github.com/intel/llvm/blob/39d1c65272a786b2b13a6f094facfddf9408406d/sycl/test/basic_tests/SYCL-2020-spec-constants.cpp#L25-L27
 * Instead of adding one more bool specialization constant, we reuse existing embree_features one
 * and use RTC_FEATURE_FLAG_NONE as value to test for avoiding to call Embree on GPU.
 */
/* We set it to RTC_FEATURE_FLAG_NONE by default so AoT binaries contain MNE and ray-trace kernels
 * pre-compiled without Embree.
 * Changing this default value would require updating the logic in oneapi_load_kernels(). */
static constexpr sycl::specialization_id<RTCFeatureFlags> oneapi_embree_features{
    RTC_FEATURE_FLAG_NONE};
#  define IF_USING_EMBREE \
    if (kernel_handler.get_specialization_constant<oneapi_embree_features>() != \
        RTC_FEATURE_FLAG_NONE)
#  define IF_NOT_USING_EMBREE \
    if (kernel_handler.get_specialization_constant<oneapi_embree_features>() == \
        RTC_FEATURE_FLAG_NONE)
#else
#  define IF_USING_EMBREE
#  define IF_NOT_USING_EMBREE
#endif

CCL_NAMESPACE_BEGIN

#ifdef __BVH2__

/* BVH2
 *
 * Bounding volume hierarchy for ray tracing, when no native acceleration
 * structure is available for the device.
 *
 * We compile different variations of the same BVH traversal function for
 * faster rendering when some types of primitives are not needed, using #includes
 * to work around the lack of C++ templates in OpenCL.
 *
 * Originally based on "Understanding the Efficiency of Ray Traversal on GPUs",
 * the code has been extended and modified to support more primitives and work
 * with CPU and various GPU kernel languages. */

#  include "kernel/bvh/nodes.h"

/* Regular BVH traversal */

#  define BVH_FUNCTION_NAME bvh_intersect
#  define BVH_FUNCTION_FEATURES BVH_POINTCLOUD
#  include "kernel/bvh/traversal.h"

#  if defined(__HAIR__)
#    define BVH_FUNCTION_NAME bvh_intersect_hair
#    define BVH_FUNCTION_FEATURES BVH_HAIR | BVH_POINTCLOUD
#    include "kernel/bvh/traversal.h"
#  endif

#  if defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_motion
#    define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_POINTCLOUD
#    include "kernel/bvh/traversal.h"
#  endif

#  if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_hair_motion
#    define BVH_FUNCTION_FEATURES BVH_HAIR | BVH_MOTION | BVH_POINTCLOUD
#    include "kernel/bvh/traversal.h"
#  endif

ccl_device_intersect bool scene_intersect(KernelGlobals kg,
                                          ccl_private const Ray *ray,
                                          const uint visibility,
                                          ccl_private Intersection *isect)
{
  if (!intersection_ray_valid(ray)) {
    return false;
  }

#  ifdef __EMBREE__
  IF_USING_EMBREE
  {
    if (kernel_data.device_bvh) {
      return kernel_embree_intersect(kg, ray, visibility, isect);
    }
  }
#  endif

  IF_NOT_USING_EMBREE
  {
#  ifdef __OBJECT_MOTION__
    if (kernel_data.bvh.have_motion) {
#    ifdef __HAIR__
      if (kernel_data.bvh.have_curves) {
        return bvh_intersect_hair_motion(kg, ray, isect, visibility);
      }
#    endif /* __HAIR__ */

      return bvh_intersect_motion(kg, ray, isect, visibility);
    }
#  endif /* __OBJECT_MOTION__ */

#  ifdef __HAIR__
    if (kernel_data.bvh.have_curves) {
      return bvh_intersect_hair(kg, ray, isect, visibility);
    }
#  endif /* __HAIR__ */

    return bvh_intersect(kg, ray, isect, visibility);
  }

  kernel_assert(false);
  return false;
}

ccl_device_intersect bool scene_intersect_shadow(KernelGlobals kg,
                                                 ccl_private const Ray *ray,
                                                 const uint visibility)
{
  Intersection isect;
  return scene_intersect(kg, ray, visibility, &isect);
}

/* Single object BVH traversal, for SSS/AO/bevel. */

#  ifdef __BVH_LOCAL__

#    define BVH_FUNCTION_NAME bvh_intersect_local
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/local.h"

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_local_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_HAIR
#      include "kernel/bvh/local.h"
#    endif

template<bool single_hit = false>
ccl_device_intersect bool scene_intersect_local(KernelGlobals kg,
                                                ccl_private const Ray *ray,
                                                ccl_private LocalIntersection *local_isect,
                                                int local_object,
                                                ccl_private uint *lcg_state,
                                                int max_hits)
{
  if (!intersection_ray_valid(ray)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    return false;
  }

#    ifdef __EMBREE__
  IF_USING_EMBREE
  {
    if (kernel_data.device_bvh) {
      return kernel_embree_intersect_local(
          kg, ray, local_isect, local_object, lcg_state, max_hits);
    }
  }
#    endif

  IF_NOT_USING_EMBREE
  {
#    ifdef __OBJECT_MOTION__
    if (kernel_data.bvh.have_motion) {
      return bvh_intersect_local_motion(kg, ray, local_isect, local_object, lcg_state, max_hits);
    }
#    endif /* __OBJECT_MOTION__ */
    return bvh_intersect_local(kg, ray, local_isect, local_object, lcg_state, max_hits);
  }

  kernel_assert(false);
  return false;
}
#  endif

/* Transparent shadow BVH traversal, recording multiple intersections. */

#  ifdef __SHADOW_RECORD_ALL__

#    define BVH_FUNCTION_NAME bvh_intersect_shadow_all
#    define BVH_FUNCTION_FEATURES BVH_POINTCLOUD
#    include "kernel/bvh/shadow_all.h"

#    if defined(__HAIR__)
#      define BVH_FUNCTION_NAME bvh_intersect_shadow_all_hair
#      define BVH_FUNCTION_FEATURES BVH_HAIR | BVH_POINTCLOUD
#      include "kernel/bvh/shadow_all.h"
#    endif

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_shadow_all_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_POINTCLOUD
#      include "kernel/bvh/shadow_all.h"
#    endif

#    if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_shadow_all_hair_motion
#      define BVH_FUNCTION_FEATURES BVH_HAIR | BVH_MOTION | BVH_POINTCLOUD
#      include "kernel/bvh/shadow_all.h"
#    endif

ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals kg,
                                                     IntegratorShadowState state,
                                                     ccl_private const Ray *ray,
                                                     uint visibility,
                                                     uint max_hits,
                                                     ccl_private uint *num_recorded_hits,
                                                     ccl_private float *throughput)
{
  if (!intersection_ray_valid(ray)) {
    *num_recorded_hits = 0;
    *throughput = 1.0f;
    return false;
  }

#    ifdef __EMBREE__
  IF_USING_EMBREE
  {
    if (kernel_data.device_bvh) {
      return kernel_embree_intersect_shadow_all(
          kg, state, ray, visibility, max_hits, num_recorded_hits, throughput);
    }
  }
#    endif

  IF_NOT_USING_EMBREE
  {
#    ifdef __OBJECT_MOTION__
    if (kernel_data.bvh.have_motion) {
#      ifdef __HAIR__
      if (kernel_data.bvh.have_curves) {
        return bvh_intersect_shadow_all_hair_motion(
            kg, ray, state, visibility, max_hits, num_recorded_hits, throughput);
      }
#      endif /* __HAIR__ */

      return bvh_intersect_shadow_all_motion(
          kg, ray, state, visibility, max_hits, num_recorded_hits, throughput);
    }
#    endif /* __OBJECT_MOTION__ */

#    ifdef __HAIR__
    if (kernel_data.bvh.have_curves) {
      return bvh_intersect_shadow_all_hair(
          kg, ray, state, visibility, max_hits, num_recorded_hits, throughput);
    }
#    endif /* __HAIR__ */

    return bvh_intersect_shadow_all(
        kg, ray, state, visibility, max_hits, num_recorded_hits, throughput);
  }

  kernel_assert(false);
  return false;
}
#  endif /* __SHADOW_RECORD_ALL__ */

/* Volume BVH traversal, for initializing or updating the volume stack. */

#  if defined(__VOLUME__) && !defined(__VOLUME_RECORD_ALL__)

#    define BVH_FUNCTION_NAME bvh_intersect_volume
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/volume.h"

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_volume_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_HAIR
#      include "kernel/bvh/volume.h"
#    endif

ccl_device_intersect bool scene_intersect_volume(KernelGlobals kg,
                                                 ccl_private const Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint visibility)
{
  if (!intersection_ray_valid(ray)) {
    return false;
  }

#    ifdef __EMBREE__
  IF_USING_EMBREE
  {
    if (kernel_data.device_bvh) {
      return kernel_embree_intersect_volume(kg, ray, isect, visibility);
    }
  }
#    endif

  IF_NOT_USING_EMBREE
  {
#    ifdef __OBJECT_MOTION__
    if (kernel_data.bvh.have_motion) {
      return bvh_intersect_volume_motion(kg, ray, isect, visibility);
    }
#    endif /* __OBJECT_MOTION__ */

    return bvh_intersect_volume(kg, ray, isect, visibility);
  }

  kernel_assert(false);
  return false;
}
#  endif /* defined(__VOLUME__) && !defined(__VOLUME_RECORD_ALL__) */

/* Volume BVH traversal, for initializing or updating the volume stack.
 * Variation that records multiple intersections at once. */

#  if defined(__VOLUME__) && defined(__VOLUME_RECORD_ALL__)

#    define BVH_FUNCTION_NAME bvh_intersect_volume_all
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/volume_all.h"

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_volume_all_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_HAIR
#      include "kernel/bvh/volume_all.h"
#    endif

ccl_device_intersect uint scene_intersect_volume(KernelGlobals kg,
                                                 ccl_private const Ray *ray,
                                                 ccl_private Intersection *isect,
                                                 const uint max_hits,
                                                 const uint visibility)
{
  if (!intersection_ray_valid(ray)) {
    return false;
  }

#    ifdef __EMBREE__
  IF_USING_EMBREE
  {
    if (kernel_data.device_bvh) {
      return kernel_embree_intersect_volume(kg, ray, isect, max_hits, visibility);
    }
  }
#    endif

  IF_NOT_USING_EMBREE
  {
#    ifdef __OBJECT_MOTION__
    if (kernel_data.bvh.have_motion) {
      return bvh_intersect_volume_all_motion(kg, ray, isect, max_hits, visibility);
    }
#    endif /* __OBJECT_MOTION__ */

    return bvh_intersect_volume_all(kg, ray, isect, max_hits, visibility);
  }

  kernel_assert(false);
  return false;
}

#  endif /* defined(__VOLUME__) && defined(__VOLUME_RECORD_ALL__) */

#  undef BVH_FEATURE
#  undef BVH_NAME_JOIN
#  undef BVH_NAME_EVAL
#  undef BVH_FUNCTION_FULL_NAME

#endif /* __BVH2__ */

CCL_NAMESPACE_END
