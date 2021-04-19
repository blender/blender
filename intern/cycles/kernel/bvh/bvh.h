/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* BVH
 *
 * Bounding volume hierarchy for ray tracing. We compile different variations
 * of the same BVH traversal function for faster rendering when some types of
 * primitives are not needed, using #includes to work around the lack of
 * C++ templates in OpenCL.
 *
 * Originally based on "Understanding the Efficiency of Ray Traversal on GPUs",
 * the code has been extended and modified to support more primitives and work
 * with CPU/CUDA/OpenCL. */

#ifdef __EMBREE__
#  include "kernel/bvh/bvh_embree.h"
#endif

#include "kernel/bvh/bvh_types.h"
#include "kernel/bvh/bvh_util.h"

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_OPTIX__

/* Regular BVH traversal */

#  include "kernel/bvh/bvh_nodes.h"

#  define BVH_FUNCTION_NAME bvh_intersect
#  define BVH_FUNCTION_FEATURES 0
#  include "kernel/bvh/bvh_traversal.h"

#  if defined(__HAIR__)
#    define BVH_FUNCTION_NAME bvh_intersect_hair
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/bvh_traversal.h"
#  endif

#  if defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_motion
#    define BVH_FUNCTION_FEATURES BVH_MOTION
#    include "kernel/bvh/bvh_traversal.h"
#  endif

#  if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_hair_motion
#    define BVH_FUNCTION_FEATURES BVH_HAIR | BVH_MOTION
#    include "kernel/bvh/bvh_traversal.h"
#  endif

/* Subsurface scattering BVH traversal */

#  if defined(__BVH_LOCAL__)
#    define BVH_FUNCTION_NAME bvh_intersect_local
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/bvh_local.h"

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_local_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_HAIR
#      include "kernel/bvh/bvh_local.h"
#    endif
#  endif /* __BVH_LOCAL__ */

/* Volume BVH traversal */

#  if defined(__VOLUME__)
#    define BVH_FUNCTION_NAME bvh_intersect_volume
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/bvh_volume.h"

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_volume_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_HAIR
#      include "kernel/bvh/bvh_volume.h"
#    endif
#  endif /* __VOLUME__ */

/* Record all intersections - Shadow BVH traversal */

#  if defined(__SHADOW_RECORD_ALL__)
#    define BVH_FUNCTION_NAME bvh_intersect_shadow_all
#    define BVH_FUNCTION_FEATURES 0
#    include "kernel/bvh/bvh_shadow_all.h"

#    if defined(__HAIR__)
#      define BVH_FUNCTION_NAME bvh_intersect_shadow_all_hair
#      define BVH_FUNCTION_FEATURES BVH_HAIR
#      include "kernel/bvh/bvh_shadow_all.h"
#    endif

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_shadow_all_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION
#      include "kernel/bvh/bvh_shadow_all.h"
#    endif

#    if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_shadow_all_hair_motion
#      define BVH_FUNCTION_FEATURES BVH_HAIR | BVH_MOTION
#      include "kernel/bvh/bvh_shadow_all.h"
#    endif
#  endif /* __SHADOW_RECORD_ALL__ */

/* Record all intersections - Volume BVH traversal  */

#  if defined(__VOLUME_RECORD_ALL__)
#    define BVH_FUNCTION_NAME bvh_intersect_volume_all
#    define BVH_FUNCTION_FEATURES BVH_HAIR
#    include "kernel/bvh/bvh_volume_all.h"

#    if defined(__OBJECT_MOTION__)
#      define BVH_FUNCTION_NAME bvh_intersect_volume_all_motion
#      define BVH_FUNCTION_FEATURES BVH_MOTION | BVH_HAIR
#      include "kernel/bvh/bvh_volume_all.h"
#    endif
#  endif /* __VOLUME_RECORD_ALL__ */

#  undef BVH_FEATURE
#  undef BVH_NAME_JOIN
#  undef BVH_NAME_EVAL
#  undef BVH_FUNCTION_FULL_NAME

#endif /* __KERNEL_OPTIX__ */

ccl_device_inline bool scene_intersect_valid(const Ray *ray)
{
  /* NOTE: Due to some vectorization code  non-finite origin point might
   * cause lots of false-positive intersections which will overflow traversal
   * stack.
   * This code is a quick way to perform early output, to avoid crashes in
   * such cases.
   * From production scenes so far it seems it's enough to test first element
   * only.
   * Scene intersection may also called with empty rays for conditional trace
   * calls that evaluate to false, so filter those out.
   */
  return isfinite_safe(ray->P.x) && isfinite_safe(ray->D.x) && len_squared(ray->D) != 0.0f;
}

ccl_device_intersect bool scene_intersect(KernelGlobals *kg,
                                          const Ray *ray,
                                          const uint visibility,
                                          Intersection *isect)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT);

#ifdef __KERNEL_OPTIX__
  uint p0 = 0;
  uint p1 = 0;
  uint p2 = 0;
  uint p3 = 0;
  uint p4 = visibility;
  uint p5 = PRIMITIVE_NONE;

  optixTrace(scene_intersect_valid(ray) ? kernel_data.bvh.scene : 0,
             ray->P,
             ray->D,
             0.0f,
             ray->t,
             ray->time,
             0xF,
             OPTIX_RAY_FLAG_NONE,
             0,  // SBT offset for PG_HITD
             0,
             0,
             p0,
             p1,
             p2,
             p3,
             p4,
             p5);

  isect->t = __uint_as_float(p0);
  isect->u = __uint_as_float(p1);
  isect->v = __uint_as_float(p2);
  isect->prim = p3;
  isect->object = p4;
  isect->type = p5;

  return p5 != PRIMITIVE_NONE;
#else /* __KERNEL_OPTIX__ */
  if (!scene_intersect_valid(ray)) {
    return false;
  }

#  ifdef __EMBREE__
  if (kernel_data.bvh.scene) {
    isect->t = ray->t;
    CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_REGULAR);
    IntersectContext rtc_ctx(&ctx);
    RTCRayHit ray_hit;
    kernel_embree_setup_rayhit(*ray, ray_hit, visibility);
    rtcIntersect1(kernel_data.bvh.scene, &rtc_ctx.context, &ray_hit);
    if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID &&
        ray_hit.hit.primID != RTC_INVALID_GEOMETRY_ID) {
      kernel_embree_convert_hit(kg, &ray_hit.ray, &ray_hit.hit, isect);
      return true;
    }
    return false;
  }
#  endif /* __EMBREE__ */

#  ifdef __OBJECT_MOTION__
  if (kernel_data.bvh.have_motion) {
#    ifdef __HAIR__
    if (kernel_data.bvh.have_curves) {
      return bvh_intersect_hair_motion(kg, ray, isect, visibility);
    }
#    endif /* __HAIR__ */

    return bvh_intersect_motion(kg, ray, isect, visibility);
  }
#  endif   /* __OBJECT_MOTION__ */

#  ifdef __HAIR__
  if (kernel_data.bvh.have_curves) {
    return bvh_intersect_hair(kg, ray, isect, visibility);
  }
#  endif /* __HAIR__ */

  return bvh_intersect(kg, ray, isect, visibility);
#endif   /* __KERNEL_OPTIX__ */
}

#ifdef __BVH_LOCAL__
ccl_device_intersect bool scene_intersect_local(KernelGlobals *kg,
                                                const Ray *ray,
                                                LocalIntersection *local_isect,
                                                int local_object,
                                                uint *lcg_state,
                                                int max_hits)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_LOCAL);

#  ifdef __KERNEL_OPTIX__
  uint p0 = ((uint64_t)lcg_state) & 0xFFFFFFFF;
  uint p1 = (((uint64_t)lcg_state) >> 32) & 0xFFFFFFFF;
  uint p2 = ((uint64_t)local_isect) & 0xFFFFFFFF;
  uint p3 = (((uint64_t)local_isect) >> 32) & 0xFFFFFFFF;
  uint p4 = local_object;
  // Is set to zero on miss or if ray is aborted, so can be used as return value
  uint p5 = max_hits;

  if (local_isect) {
    local_isect->num_hits = 0;  // Initialize hit count to zero
  }
  optixTrace(scene_intersect_valid(ray) ? kernel_data.bvh.scene : 0,
             ray->P,
             ray->D,
             0.0f,
             ray->t,
             ray->time,
             // Skip curves
             0x3,
             // Need to always call into __anyhit__kernel_optix_local_hit
             OPTIX_RAY_FLAG_ENFORCE_ANYHIT,
             2,  // SBT offset for PG_HITL
             0,
             0,
             p0,
             p1,
             p2,
             p3,
             p4,
             p5);

  return p5;
#  else /* __KERNEL_OPTIX__ */
  if (!scene_intersect_valid(ray)) {
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    return false;
  }

#    ifdef __EMBREE__
  if (kernel_data.bvh.scene) {
    const bool has_bvh = !(kernel_tex_fetch(__object_flag, local_object) &
                           SD_OBJECT_TRANSFORM_APPLIED);
    CCLIntersectContext ctx(
        kg, has_bvh ? CCLIntersectContext::RAY_SSS : CCLIntersectContext::RAY_LOCAL);
    ctx.lcg_state = lcg_state;
    ctx.max_hits = max_hits;
    ctx.local_isect = local_isect;
    if (local_isect) {
      local_isect->num_hits = 0;
    }
    ctx.local_object_id = local_object;
    IntersectContext rtc_ctx(&ctx);
    RTCRay rtc_ray;
    kernel_embree_setup_ray(*ray, rtc_ray, PATH_RAY_ALL_VISIBILITY);

    /* If this object has its own BVH, use it. */
    if (has_bvh) {
      RTCGeometry geom = rtcGetGeometry(kernel_data.bvh.scene, local_object * 2);
      if (geom) {
        float3 P = ray->P;
        float3 dir = ray->D;
        float3 idir = ray->D;
        Transform ob_itfm;
        rtc_ray.tfar = bvh_instance_motion_push(
            kg, local_object, ray, &P, &dir, &idir, ray->t, &ob_itfm);
        /* bvh_instance_motion_push() returns the inverse transform but
         * it's not needed here. */
        (void)ob_itfm;

        rtc_ray.org_x = P.x;
        rtc_ray.org_y = P.y;
        rtc_ray.org_z = P.z;
        rtc_ray.dir_x = dir.x;
        rtc_ray.dir_y = dir.y;
        rtc_ray.dir_z = dir.z;
        RTCScene scene = (RTCScene)rtcGetGeometryUserData(geom);
        kernel_assert(scene);
        if (scene) {
          rtcOccluded1(scene, &rtc_ctx.context, &rtc_ray);
        }
      }
    }
    else {
      rtcOccluded1(kernel_data.bvh.scene, &rtc_ctx.context, &rtc_ray);
    }

    /* rtcOccluded1 sets tfar to -inf if a hit was found. */
    return (local_isect && local_isect->num_hits > 0) || (rtc_ray.tfar < 0);
    ;
  }
#    endif /* __EMBREE__ */

#    ifdef __OBJECT_MOTION__
  if (kernel_data.bvh.have_motion) {
    return bvh_intersect_local_motion(kg, ray, local_isect, local_object, lcg_state, max_hits);
  }
#    endif /* __OBJECT_MOTION__ */
  return bvh_intersect_local(kg, ray, local_isect, local_object, lcg_state, max_hits);
#  endif   /* __KERNEL_OPTIX__ */
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals *kg,
                                                     const Ray *ray,
                                                     Intersection *isect,
                                                     uint visibility,
                                                     uint max_hits,
                                                     uint *num_hits)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_SHADOW_ALL);

#  ifdef __KERNEL_OPTIX__
  uint p0 = ((uint64_t)isect) & 0xFFFFFFFF;
  uint p1 = (((uint64_t)isect) >> 32) & 0xFFFFFFFF;
  uint p3 = max_hits;
  uint p4 = visibility;
  uint p5 = false;

  *num_hits = 0;  // Initialize hit count to zero
  optixTrace(scene_intersect_valid(ray) ? kernel_data.bvh.scene : 0,
             ray->P,
             ray->D,
             0.0f,
             ray->t,
             ray->time,
             0xF,
             // Need to always call into __anyhit__kernel_optix_shadow_all_hit
             OPTIX_RAY_FLAG_ENFORCE_ANYHIT,
             1,  // SBT offset for PG_HITS
             0,
             0,
             p0,
             p1,
             *num_hits,
             p3,
             p4,
             p5);

  return p5;
#  else /* __KERNEL_OPTIX__ */
  if (!scene_intersect_valid(ray)) {
    *num_hits = 0;
    return false;
  }

#    ifdef __EMBREE__
  if (kernel_data.bvh.scene) {
    CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_SHADOW_ALL);
    ctx.isect_s = isect;
    ctx.max_hits = max_hits;
    ctx.num_hits = 0;
    IntersectContext rtc_ctx(&ctx);
    RTCRay rtc_ray;
    kernel_embree_setup_ray(*ray, rtc_ray, visibility);
    rtcOccluded1(kernel_data.bvh.scene, &rtc_ctx.context, &rtc_ray);

    if (ctx.num_hits > max_hits) {
      return true;
    }
    *num_hits = ctx.num_hits;
    return rtc_ray.tfar == -INFINITY;
  }
#    endif /* __EMBREE__ */

#    ifdef __OBJECT_MOTION__
  if (kernel_data.bvh.have_motion) {
#      ifdef __HAIR__
    if (kernel_data.bvh.have_curves) {
      return bvh_intersect_shadow_all_hair_motion(kg, ray, isect, visibility, max_hits, num_hits);
    }
#      endif /* __HAIR__ */

    return bvh_intersect_shadow_all_motion(kg, ray, isect, visibility, max_hits, num_hits);
  }
#    endif   /* __OBJECT_MOTION__ */

#    ifdef __HAIR__
  if (kernel_data.bvh.have_curves) {
    return bvh_intersect_shadow_all_hair(kg, ray, isect, visibility, max_hits, num_hits);
  }
#    endif /* __HAIR__ */

  return bvh_intersect_shadow_all(kg, ray, isect, visibility, max_hits, num_hits);
#  endif   /* __KERNEL_OPTIX__ */
}
#endif /* __SHADOW_RECORD_ALL__ */

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals *kg,
                                                 const Ray *ray,
                                                 Intersection *isect,
                                                 const uint visibility)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_VOLUME);

#  ifdef __KERNEL_OPTIX__
  uint p0 = 0;
  uint p1 = 0;
  uint p2 = 0;
  uint p3 = 0;
  uint p4 = visibility;
  uint p5 = PRIMITIVE_NONE;

  optixTrace(scene_intersect_valid(ray) ? kernel_data.bvh.scene : 0,
             ray->P,
             ray->D,
             0.0f,
             ray->t,
             ray->time,
             // Skip everything but volumes
             0x2,
             OPTIX_RAY_FLAG_NONE,
             0,  // SBT offset for PG_HITD
             0,
             0,
             p0,
             p1,
             p2,
             p3,
             p4,
             p5);

  isect->t = __uint_as_float(p0);
  isect->u = __uint_as_float(p1);
  isect->v = __uint_as_float(p2);
  isect->prim = p3;
  isect->object = p4;
  isect->type = p5;

  return p5 != PRIMITIVE_NONE;
#  else /* __KERNEL_OPTIX__ */
  if (!scene_intersect_valid(ray)) {
    return false;
  }

#    ifdef __OBJECT_MOTION__
  if (kernel_data.bvh.have_motion) {
    return bvh_intersect_volume_motion(kg, ray, isect, visibility);
  }
#    endif /* __OBJECT_MOTION__ */

  return bvh_intersect_volume(kg, ray, isect, visibility);
#  endif   /* __KERNEL_OPTIX__ */
}
#endif /* __VOLUME__ */

#ifdef __VOLUME_RECORD_ALL__
ccl_device_intersect uint scene_intersect_volume_all(KernelGlobals *kg,
                                                     const Ray *ray,
                                                     Intersection *isect,
                                                     const uint max_hits,
                                                     const uint visibility)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_VOLUME_ALL);

  if (!scene_intersect_valid(ray)) {
    return false;
  }

#  ifdef __EMBREE__
  if (kernel_data.bvh.scene) {
    CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_VOLUME_ALL);
    ctx.isect_s = isect;
    ctx.max_hits = max_hits;
    ctx.num_hits = 0;
    IntersectContext rtc_ctx(&ctx);
    RTCRay rtc_ray;
    kernel_embree_setup_ray(*ray, rtc_ray, visibility);
    rtcOccluded1(kernel_data.bvh.scene, &rtc_ctx.context, &rtc_ray);
    return ctx.num_hits;
  }
#  endif /* __EMBREE__ */

#  ifdef __OBJECT_MOTION__
  if (kernel_data.bvh.have_motion) {
    return bvh_intersect_volume_all_motion(kg, ray, isect, max_hits, visibility);
  }
#  endif /* __OBJECT_MOTION__ */

  return bvh_intersect_volume_all(kg, ray, isect, max_hits, visibility);
}
#endif /* __VOLUME_RECORD_ALL__ */

CCL_NAMESPACE_END
