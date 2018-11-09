/*
 * Copyright 2018, Blender Foundation.
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

#include <embree3/rtcore_ray.h>
#include <embree3/rtcore_scene.h>

#include "kernel/kernel_compat_cpu.h"
#include "kernel/split/kernel_split_data_types.h"
#include "kernel/kernel_globals.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

struct CCLIntersectContext  {
	typedef enum {
		RAY_REGULAR = 0,
		RAY_SHADOW_ALL = 1,
		RAY_SSS = 2,
		RAY_VOLUME_ALL = 3,
		
	} RayType;

	KernelGlobals *kg;
	RayType type;

	/* for shadow rays */
	Intersection *isect_s;
	int max_hits;
	int num_hits;

	/* for SSS Rays: */
	LocalIntersection *ss_isect;
	int sss_object_id;
	uint *lcg_state;

	CCLIntersectContext(KernelGlobals *kg_,  RayType type_)
	{
		kg = kg_;
		type = type_;
		max_hits = 1;
		num_hits = 0;
		isect_s = NULL;
		ss_isect = NULL;
		sss_object_id = -1;
		lcg_state = NULL;
	}
};

class IntersectContext
{
public:
	IntersectContext(CCLIntersectContext* ctx)
	{
		rtcInitIntersectContext(&context);
		userRayExt = ctx;
	}
	RTCIntersectContext context;
	CCLIntersectContext* userRayExt;
};

ccl_device_inline void kernel_embree_setup_ray(const Ray& ray,
                                               RTCRay& rtc_ray,
                                               const uint visibility)
{
	rtc_ray.org_x = ray.P.x;
	rtc_ray.org_y = ray.P.y;
	rtc_ray.org_z = ray.P.z;
	rtc_ray.dir_x = ray.D.x;
	rtc_ray.dir_y = ray.D.y;
	rtc_ray.dir_z = ray.D.z;
	rtc_ray.tnear = 0.0f;
	rtc_ray.tfar = ray.t;
	rtc_ray.time = ray.time;
	rtc_ray.mask = visibility;
}

ccl_device_inline void kernel_embree_setup_rayhit(const Ray& ray,
                                                  RTCRayHit& rayhit,
                                                  const uint visibility)
{
	kernel_embree_setup_ray(ray, rayhit.ray, visibility);
	rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
}

ccl_device_inline void kernel_embree_convert_hit(KernelGlobals *kg,
                                                 const RTCRay *ray,
                                                 const RTCHit *hit,
                                                 Intersection *isect)
{
	bool is_hair = hit->geomID & 1;
	isect->u = is_hair ? hit->u : 1.0f - hit->v - hit->u;
	isect->v = is_hair ? hit->v : hit->u;
	isect->t = ray->tfar;
	isect->Ng = make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z);
	if(hit->instID[0] != RTC_INVALID_GEOMETRY_ID) {
		RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
		        rtcGetGeometry(kernel_data.bvh.scene, hit->instID[0]));
		isect->prim = hit->primID + (intptr_t)rtcGetGeometryUserData(
		        rtcGetGeometry(inst_scene, hit->geomID)) +
		        kernel_tex_fetch(__object_node, hit->instID[0]/2);
		isect->object = hit->instID[0]/2;
	}
	else {
		isect->prim = hit->primID + (intptr_t)rtcGetGeometryUserData(
		        rtcGetGeometry(kernel_data.bvh.scene, hit->geomID));
		isect->object = OBJECT_NONE;
	}
	isect->type = kernel_tex_fetch(__prim_type, isect->prim);
}

ccl_device_inline void kernel_embree_convert_local_hit(KernelGlobals *kg,
                                                       const RTCRay *ray,
                                                       const RTCHit *hit,
                                                       Intersection *isect,
                                                       int local_object_id)
{
	isect->u = 1.0f - hit->v - hit->u;
	isect->v = hit->u;
	isect->t = ray->tfar;
	isect->Ng = make_float3(hit->Ng_x, hit->Ng_y, hit->Ng_z);
	RTCScene inst_scene = (RTCScene)rtcGetGeometryUserData(
	        rtcGetGeometry(kernel_data.bvh.scene, local_object_id * 2));
	isect->prim = hit->primID + (intptr_t)rtcGetGeometryUserData(
	        rtcGetGeometry(inst_scene, hit->geomID)) +
	        kernel_tex_fetch(__object_node, local_object_id);
	isect->object = local_object_id;
	isect->type = kernel_tex_fetch(__prim_type, isect->prim);
}

ccl_device_inline bool embree_scene_intersect(KernelGlobals *kg,
                                              const Ray *ray,
                                              const uint visibility,
                                              Intersection *isect)
{
	kernel_assert(kernel_data.bvh.scene != NULL);
	isect->t = ray->t;
	CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_REGULAR);
	IntersectContext rtc_ctx(&ctx);
	RTCRayHit ray_hit;
	kernel_embree_setup_rayhit(*ray, ray_hit, visibility);
	rtcIntersect1(kernel_data.bvh.scene, &rtc_ctx.context, &ray_hit);
	if(ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID &&
	   ray_hit.hit.primID != RTC_INVALID_GEOMETRY_ID)
	{
		kernel_embree_convert_hit(kg, &ray_hit.ray, &ray_hit.hit, isect);
		return true;
	}
	return false;
}

#ifdef __BVH_LOCAL__
ccl_device_inline bool embree_scene_intersect_local(
        KernelGlobals *kg,
        const Ray ray,
        LocalIntersection *local_isect,
        int local_object,
        uint *lcg_state,
        int max_hits)
{
	kernel_assert(kernel_data.bvh.scene != NULL);
	CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_SSS);
	ctx.lcg_state = lcg_state;
	ctx.max_hits = max_hits;
	ctx.ss_isect = local_isect;
	local_isect->num_hits = 0;
	ctx.sss_object_id = local_object;
	IntersectContext rtc_ctx(&ctx);
	RTCRay rtc_ray;
	kernel_embree_setup_ray(ray, rtc_ray, PATH_RAY_ALL_VISIBILITY);
	/* Get the Embree scene for this intersection. */
	RTCGeometry geom = rtcGetGeometry(kernel_data.bvh.scene, local_object * 2);
	if(geom) {
		float3 P = ray.P;
		float3 dir = ray.D;
		float3 idir = ray.D;
		const int object_flag = kernel_tex_fetch(__object_flag, local_object);
		if(!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
			Transform ob_itfm;
			rtc_ray.tfar = bvh_instance_motion_push(kg,
													local_object,
													&ray,
													&P,
													&dir,
													&idir,
													ray.t,
													&ob_itfm);
			/* bvh_instance_motion_push() returns the inverse transform but
				* it's not needed here. */
			(void) ob_itfm;

			rtc_ray.org_x = P.x;
			rtc_ray.org_y = P.y;
			rtc_ray.org_z = P.z;
			rtc_ray.dir_x = dir.x;
			rtc_ray.dir_y = dir.y;
			rtc_ray.dir_z = dir.z;
		}
		RTCScene scene = (RTCScene)rtcGetGeometryUserData(geom);
		if(scene) {
			rtcOccluded1(scene, &rtc_ctx.context, &rtc_ray);
		}
	}
	return local_isect->num_hits > 0;
}
#endif  /* __BVH_LOCAL__ */

#ifdef __SHADOW_RECORD_ALL__
ccl_device_inline bool embree_scene_intersect_shadow_all(
        KernelGlobals *kg,
        const Ray *ray,
        Intersection *isect,
        uint max_hits,
        uint *num_hits)
{
	kernel_assert(kernel_data.bvh.scene != NULL);
	CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_SHADOW_ALL);
	ctx.isect_s = isect;
	ctx.max_hits = max_hits;
	ctx.num_hits = 0;
	IntersectContext rtc_ctx(&ctx);
	RTCRay rtc_ray;
	kernel_embree_setup_ray(*ray, rtc_ray, PATH_RAY_SHADOW);
	rtcOccluded1(kernel_data.bvh.scene, &rtc_ctx.context, &rtc_ray);
	if(ctx.num_hits > max_hits) {
		return true;
	}
	*num_hits = ctx.num_hits;
	return rtc_ray.tfar == -INFINITY;
}
#endif  /* __SHADOW_RECORD_ALL__ */

#ifdef __VOLUME_RECORD_ALL__
ccl_device_inline uint embree_scene_intersect_volume_all(
        KernelGlobals *kg,
        const Ray *ray,
        Intersection *isect,
        const uint max_hits,
        const uint visibility)
{
	kernel_assert(kernel_data.bvh.scene != NULL);
	CCLIntersectContext ctx(kg, CCLIntersectContext::RAY_VOLUME_ALL);
	ctx.isect_s = isect;
	ctx.max_hits = max_hits;
	ctx.num_hits = 0;
	IntersectContext rtc_ctx(&ctx);
	RTCRay rtc_ray;
	kernel_embree_setup_ray(*ray, rtc_ray, visibility);
	rtcOccluded1(kernel_data.bvh.scene, &rtc_ctx.context, &rtc_ray);
	return rtc_ray.tfar == -INFINITY;
}
#endif  /* __VOLUME_RECORD_ALL__ */

CCL_NAMESPACE_END
