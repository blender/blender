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

CCL_NAMESPACE_BEGIN

#include "kernel/bvh/bvh_types.h"

/* Common QBVH functions. */
#ifdef __QBVH__
#  include "kernel/bvh/qbvh_nodes.h"
#endif

/* Regular BVH traversal */

#include "kernel/bvh/bvh_nodes.h"

#define BVH_FUNCTION_NAME bvh_intersect
#define BVH_FUNCTION_FEATURES 0
#include "kernel/bvh/bvh_traversal.h"

#if defined(__INSTANCING__)
#  define BVH_FUNCTION_NAME bvh_intersect_instancing
#  define BVH_FUNCTION_FEATURES BVH_INSTANCING
#  include "kernel/bvh/bvh_traversal.h"
#endif

#if defined(__HAIR__)
#  define BVH_FUNCTION_NAME bvh_intersect_hair
#  define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR|BVH_HAIR_MINIMUM_WIDTH
#  include "kernel/bvh/bvh_traversal.h"
#endif

#if defined(__OBJECT_MOTION__)
#  define BVH_FUNCTION_NAME bvh_intersect_motion
#  define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_MOTION
#  include "kernel/bvh/bvh_traversal.h"
#endif

#if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#  define BVH_FUNCTION_NAME bvh_intersect_hair_motion
#  define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR|BVH_HAIR_MINIMUM_WIDTH|BVH_MOTION
#  include "kernel/bvh/bvh_traversal.h"
#endif

/* Subsurface scattering BVH traversal */

#if defined(__SUBSURFACE__)
#  define BVH_FUNCTION_NAME bvh_intersect_subsurface
#  define BVH_FUNCTION_FEATURES BVH_HAIR
#  include "kernel/bvh/bvh_subsurface.h"

#  if defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_subsurface_motion
#    define BVH_FUNCTION_FEATURES BVH_MOTION|BVH_HAIR
#    include "kernel/bvh/bvh_subsurface.h"
#  endif
#endif  /* __SUBSURFACE__ */

/* Volume BVH traversal */

#if defined(__VOLUME__)
#  define BVH_FUNCTION_NAME bvh_intersect_volume
#  define BVH_FUNCTION_FEATURES BVH_HAIR
#  include "kernel/bvh/bvh_volume.h"

#  if defined(__INSTANCING__)
#    define BVH_FUNCTION_NAME bvh_intersect_volume_instancing
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR
#    include "kernel/bvh/bvh_volume.h"
#  endif

#  if defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_volume_motion
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_MOTION|BVH_HAIR
#    include "kernel/bvh/bvh_volume.h"
#  endif
#endif  /* __VOLUME__ */

/* Record all intersections - Shadow BVH traversal */

#if defined(__SHADOW_RECORD_ALL__)
#  define BVH_FUNCTION_NAME bvh_intersect_shadow_all
#  define BVH_FUNCTION_FEATURES 0
#  include "kernel/bvh/bvh_shadow_all.h"

#  if defined(__INSTANCING__)
#    define BVH_FUNCTION_NAME bvh_intersect_shadow_all_instancing
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING
#    include "kernel/bvh/bvh_shadow_all.h"
#  endif

#  if defined(__HAIR__)
#    define BVH_FUNCTION_NAME bvh_intersect_shadow_all_hair
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR
#    include "kernel/bvh/bvh_shadow_all.h"
#  endif

#  if defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_shadow_all_motion
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_MOTION
#    include "kernel/bvh/bvh_shadow_all.h"
#  endif

#  if defined(__HAIR__) && defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_shadow_all_hair_motion
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR|BVH_MOTION
#    include "kernel/bvh/bvh_shadow_all.h"
#  endif
#endif  /* __SHADOW_RECORD_ALL__ */

/* Record all intersections - Volume BVH traversal  */

#if defined(__VOLUME_RECORD_ALL__)
#  define BVH_FUNCTION_NAME bvh_intersect_volume_all
#  define BVH_FUNCTION_FEATURES BVH_HAIR
#  include "kernel/bvh/bvh_volume_all.h"

#  if defined(__INSTANCING__)
#    define BVH_FUNCTION_NAME bvh_intersect_volume_all_instancing
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_HAIR
#    include "kernel/bvh/bvh_volume_all.h"
#  endif

#  if defined(__OBJECT_MOTION__)
#    define BVH_FUNCTION_NAME bvh_intersect_volume_all_motion
#    define BVH_FUNCTION_FEATURES BVH_INSTANCING|BVH_MOTION|BVH_HAIR
#    include "kernel/bvh/bvh_volume_all.h"
#  endif
#endif  /* __VOLUME_RECORD_ALL__ */

#undef BVH_FEATURE
#undef BVH_NAME_JOIN
#undef BVH_NAME_EVAL
#undef BVH_FUNCTION_FULL_NAME

/* Note: ray is passed by value to work around a possible CUDA compiler bug. */
ccl_device_intersect bool scene_intersect(KernelGlobals *kg,
                                          const Ray ray,
                                          const uint visibility,
                                          Intersection *isect,
                                          uint *lcg_state,
                                          float difl,
                                          float extmax)
{
#ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
#  ifdef __HAIR__
		if(kernel_data.bvh.have_curves)
			return bvh_intersect_hair_motion(kg, &ray, isect, visibility, lcg_state, difl, extmax);
#  endif /* __HAIR__ */

		return bvh_intersect_motion(kg, &ray, isect, visibility);
	}
#endif /* __OBJECT_MOTION__ */

#ifdef __HAIR__
	if(kernel_data.bvh.have_curves)
		return bvh_intersect_hair(kg, &ray, isect, visibility, lcg_state, difl, extmax);
#endif /* __HAIR__ */

#ifdef __KERNEL_CPU__

#  ifdef __INSTANCING__
	if(kernel_data.bvh.have_instancing)
		return bvh_intersect_instancing(kg, &ray, isect, visibility);
#  endif /* __INSTANCING__ */

	return bvh_intersect(kg, &ray, isect, visibility);
#else /* __KERNEL_CPU__ */

#  ifdef __INSTANCING__
	return bvh_intersect_instancing(kg, &ray, isect, visibility);
#  else
	return bvh_intersect(kg, &ray, isect, visibility);
#  endif /* __INSTANCING__ */

#endif /* __KERNEL_CPU__ */
}

#ifdef __SUBSURFACE__
/* Note: ray is passed by value to work around a possible CUDA compiler bug. */
ccl_device_intersect void scene_intersect_subsurface(KernelGlobals *kg,
                                                     const Ray ray,
                                                     SubsurfaceIntersection *ss_isect,
                                                     int subsurface_object,
                                                     uint *lcg_state,
                                                     int max_hits)
{
#ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
		return bvh_intersect_subsurface_motion(kg,
		                                       &ray,
		                                       ss_isect,
		                                       subsurface_object,
		                                       lcg_state,
		                                       max_hits);
	}
#endif /* __OBJECT_MOTION__ */
	return bvh_intersect_subsurface(kg,
	                                &ray,
	                                ss_isect,
	                                subsurface_object,
	                                lcg_state,
	                                max_hits);
}
#endif

#ifdef __SHADOW_RECORD_ALL__
ccl_device_intersect bool scene_intersect_shadow_all(KernelGlobals *kg,
                                                     const Ray *ray,
                                                     Intersection *isect,
                                                     int skip_object,
                                                     uint max_hits,
                                                     uint *num_hits)
{
#  ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
#    ifdef __HAIR__
		if(kernel_data.bvh.have_curves) {
			return bvh_intersect_shadow_all_hair_motion(kg,
			                                            ray,
			                                            isect,
			                                            skip_object,
			                                            max_hits,
			                                            num_hits);
		}
#    endif /* __HAIR__ */

		return bvh_intersect_shadow_all_motion(kg,
		                                       ray,
		                                       isect,
		                                       skip_object,
		                                       max_hits,
		                                       num_hits);
	}
#  endif /* __OBJECT_MOTION__ */

#  ifdef __HAIR__
	if(kernel_data.bvh.have_curves) {
		return bvh_intersect_shadow_all_hair(kg,
		                                     ray,
		                                     isect,
		                                     skip_object,
		                                     max_hits,
		                                     num_hits);
	}
#  endif /* __HAIR__ */

#  ifdef __INSTANCING__
	if(kernel_data.bvh.have_instancing) {
		return bvh_intersect_shadow_all_instancing(kg,
		                                           ray,
		                                           isect,
		                                           skip_object,
		                                           max_hits,
		                                           num_hits);
	}
#  endif /* __INSTANCING__ */

	return bvh_intersect_shadow_all(kg,
	                                ray,
	                                isect,
	                                skip_object,
	                                max_hits,
	                                num_hits);
}
#endif  /* __SHADOW_RECORD_ALL__ */

#ifdef __VOLUME__
ccl_device_intersect bool scene_intersect_volume(KernelGlobals *kg,
                                                 const Ray *ray,
                                                 Intersection *isect,
                                                 const uint visibility)
{
#  ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
		return bvh_intersect_volume_motion(kg, ray, isect, visibility);
	}
#  endif /* __OBJECT_MOTION__ */
#  ifdef __KERNEL_CPU__
#    ifdef __INSTANCING__
	if(kernel_data.bvh.have_instancing)
		return bvh_intersect_volume_instancing(kg, ray, isect, visibility);
#    endif /* __INSTANCING__ */
	return bvh_intersect_volume(kg, ray, isect, visibility);
#  else /* __KERNEL_CPU__ */
#    ifdef __INSTANCING__
	return bvh_intersect_volume_instancing(kg, ray, isect, visibility);
#    else
	return bvh_intersect_volume(kg, ray, isect, visibility);
#    endif /* __INSTANCING__ */
#  endif /* __KERNEL_CPU__ */
}
#endif  /* __VOLUME__ */

#ifdef __VOLUME_RECORD_ALL__
ccl_device_intersect uint scene_intersect_volume_all(KernelGlobals *kg,
                                                     const Ray *ray,
                                                     Intersection *isect,
                                                     const uint max_hits,
                                                     const uint visibility)
{
#  ifdef __OBJECT_MOTION__
	if(kernel_data.bvh.have_motion) {
		return bvh_intersect_volume_all_motion(kg, ray, isect, max_hits, visibility);
	}
#  endif /* __OBJECT_MOTION__ */
#  ifdef __INSTANCING__
	if(kernel_data.bvh.have_instancing)
		return bvh_intersect_volume_all_instancing(kg, ray, isect, max_hits, visibility);
#  endif /* __INSTANCING__ */
	return bvh_intersect_volume_all(kg, ray, isect, max_hits, visibility);
}
#endif  /* __VOLUME_RECORD_ALL__ */


/* Ray offset to avoid self intersection.
 *
 * This function should be used to compute a modified ray start position for
 * rays leaving from a surface. */

ccl_device_inline float3 ray_offset(float3 P, float3 Ng)
{
#ifdef __INTERSECTION_REFINE__
	const float epsilon_f = 1e-5f;
	/* ideally this should match epsilon_f, but instancing and motion blur
	 * precision makes it problematic */
	const float epsilon_test = 1.0f;
	const int epsilon_i = 32;

	float3 res;

	/* x component */
	if(fabsf(P.x) < epsilon_test) {
		res.x = P.x + Ng.x*epsilon_f;
	}
	else {
		uint ix = __float_as_uint(P.x);
		ix += ((ix ^ __float_as_uint(Ng.x)) >> 31)? -epsilon_i: epsilon_i;
		res.x = __uint_as_float(ix);
	}

	/* y component */
	if(fabsf(P.y) < epsilon_test) {
		res.y = P.y + Ng.y*epsilon_f;
	}
	else {
		uint iy = __float_as_uint(P.y);
		iy += ((iy ^ __float_as_uint(Ng.y)) >> 31)? -epsilon_i: epsilon_i;
		res.y = __uint_as_float(iy);
	}

	/* z component */
	if(fabsf(P.z) < epsilon_test) {
		res.z = P.z + Ng.z*epsilon_f;
	}
	else {
		uint iz = __float_as_uint(P.z);
		iz += ((iz ^ __float_as_uint(Ng.z)) >> 31)? -epsilon_i: epsilon_i;
		res.z = __uint_as_float(iz);
	}

	return res;
#else
	const float epsilon_f = 1e-4f;
	return P + epsilon_f*Ng;
#endif
}

#if defined(__VOLUME_RECORD_ALL__) || (defined(__SHADOW_RECORD_ALL__) && defined(__KERNEL_CPU__))
/* ToDo: Move to another file? */
ccl_device int intersections_compare(const void *a, const void *b)
{
	const Intersection *isect_a = (const Intersection*)a;
	const Intersection *isect_b = (const Intersection*)b;

	if(isect_a->t < isect_b->t)
		return -1;
	else if(isect_a->t > isect_b->t)
		return 1;
	else
		return 0;
}
#endif

#if defined(__SHADOW_RECORD_ALL__)
ccl_device_inline void sort_intersections(Intersection *hits, uint num_hits)
{
#ifdef __KERNEL_GPU__
	/* Use bubble sort which has more friendly memory pattern on GPU. */
	bool swapped;
	do {
		swapped = false;
		for(int j = 0; j < num_hits - 1; ++j) {
			if(hits[j].t > hits[j + 1].t) {
				struct Intersection tmp = hits[j];
				hits[j] = hits[j + 1];
				hits[j + 1] = tmp;
				swapped = true;
			}
		}
		--num_hits;
	} while(swapped);
#else
	qsort(hits, num_hits, sizeof(Intersection), intersections_compare);
#endif
}
#endif  /* __SHADOW_RECORD_ALL__ | __VOLUME_RECORD_ALL__ */

CCL_NAMESPACE_END
