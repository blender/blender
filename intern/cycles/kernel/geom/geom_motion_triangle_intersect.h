/*
 * Copyright 2011-2016 Blender Foundation
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

/* Motion Triangle Primitive
 *
 * These are stored as regular triangles, plus extra positions and normals at
 * times other than the frame center. Computing the triangle vertex positions
 * or normals at a given ray time is a matter of interpolation of the two steps
 * between which the ray time lies.
 *
 * The extra positions and normals are stored as ATTR_STD_MOTION_VERTEX_POSITION
 * and ATTR_STD_MOTION_VERTEX_NORMAL mesh attributes.
 */

CCL_NAMESPACE_BEGIN

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance.
 */

ccl_device_inline float3 motion_triangle_refine(KernelGlobals *kg,
                                                ShaderData *sd,
                                                const Intersection *isect,
                                                const Ray *ray,
                                                float3 verts[3])
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
		if(UNLIKELY(t == 0.0f)) {
			return P;
		}
#  ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#  else
		Transform tfm = object_fetch_transform(kg,
		                                       isect->object,
		                                       OBJECT_INVERSE_TRANSFORM);
#  endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D*t);
		D = normalize_len(D, &t);
	}

	P = P + D*t;

	/* Compute refined intersection distance. */
	const float3 e1 = verts[0] - verts[2];
	const float3 e2 = verts[1] - verts[2];
	const float3 s1 = cross(D, e2);

	const float invdivisor = 1.0f/dot(s1, e1);
	const float3 d = P - verts[2];
	const float3 s2 = cross(d, e1);
	float rt = dot(e2, s2)*invdivisor;

	/* Compute refined position. */
	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#  else
		Transform tfm = object_fetch_transform(kg,
		                                       isect->object,
		                                       OBJECT_TRANSFORM);
#  endif

		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

/* Same as above, except that isect->t is assumed to be in object space
 * for instancing.
 */

#ifdef __BVH_LOCAL__
#  if defined(__KERNEL_CUDA__) && (defined(i386) || defined(_M_IX86))
ccl_device_noinline
#  else
ccl_device_inline
#  endif
float3 motion_triangle_refine_local(KernelGlobals *kg,
                                    ShaderData *sd,
                                    const Intersection *isect,
                                    const Ray *ray,
                                    float3 verts[3])
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#  ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
#    ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#    else
		Transform tfm = object_fetch_transform(kg,
		                                       isect->object,
		                                       OBJECT_INVERSE_TRANSFORM);
#    endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D);
		D = normalize(D);
	}

	P = P + D*t;

	/* compute refined intersection distance */
	const float3 e1 = verts[0] - verts[2];
	const float3 e2 = verts[1] - verts[2];
	const float3 s1 = cross(D, e2);

	const float invdivisor = 1.0f/dot(s1, e1);
	const float3 d = P - verts[2];
	const float3 s2 = cross(d, e1);
	float rt = dot(e2, s2)*invdivisor;

	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
#    ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#    else
		Transform tfm = object_fetch_transform(kg,
		                                       isect->object,
		                                       OBJECT_TRANSFORM);
#    endif

		P = transform_point(&tfm, P);
	}

	return P;
#  else  /* __INTERSECTION_REFINE__ */
	return P + D*t;
#  endif  /* __INTERSECTION_REFINE__ */
}
#endif  /* __BVH_LOCAL__ */


/* Ray intersection. We simply compute the vertex positions at the given ray
 * time and do a ray intersection with the resulting triangle.
 */

ccl_device_inline bool motion_triangle_intersect(
        KernelGlobals *kg,
        Intersection *isect,
        float3 P,
        float3 dir,
        float time,
        uint visibility,
        int object,
        int prim_addr)
{
	/* Primitive index for vertex location lookup. */
	int prim = kernel_tex_fetch(__prim_index, prim_addr);
	int fobject = (object == OBJECT_NONE)
	                  ? kernel_tex_fetch(__prim_object, prim_addr)
	                  : object;
	/* Get vertex locations for intersection. */
	float3 verts[3];
	motion_triangle_vertices(kg, fobject, prim, time, verts);
	/* Ray-triangle intersection, unoptimized. */
	float t, u, v;
	if(ray_triangle_intersect(P,
	                          dir,
	                          isect->t,
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
	                          (ssef*)verts,
#else
	                          verts[0], verts[1], verts[2],
#endif
	                          &u, &v, &t))
	{
#ifdef __VISIBILITY_FLAG__
		/* Visibility flag test. we do it here under the assumption
		 * that most triangles are culled by node flags.
		 */
		if(kernel_tex_fetch(__prim_visibility, prim_addr) & visibility)
#endif
		{
			isect->t = t;
			isect->u = u;
			isect->v = v;
			isect->prim = prim_addr;
			isect->object = object;
			isect->type = PRIMITIVE_MOTION_TRIANGLE;
			return true;
		}
	}
	return false;
}

/* Special ray intersection routines for local intersections. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point.
 * Returns whether traversal should be stopped.
 */
#ifdef __BVH_LOCAL__
ccl_device_inline bool motion_triangle_intersect_local(
        KernelGlobals *kg,
        LocalIntersection *local_isect,
        float3 P,
        float3 dir,
        float time,
        int object,
        int local_object,
        int prim_addr,
        float tmax,
        uint *lcg_state,
        int max_hits)
{
	/* Only intersect with matching object, for instanced objects we
	 * already know we are only intersecting the right object. */
	if(object == OBJECT_NONE) {
		if(kernel_tex_fetch(__prim_object, prim_addr) != local_object) {
			return false;
		}
	}

	/* Primitive index for vertex location lookup. */
	int prim = kernel_tex_fetch(__prim_index, prim_addr);
	/* Get vertex locations for intersection. */
	float3 verts[3];
	motion_triangle_vertices(kg, local_object, prim, time, verts);
	/* Ray-triangle intersection, unoptimized. */
	float t, u, v;
	if(!ray_triangle_intersect(P,
	                           dir,
	                           tmax,
#if defined(__KERNEL_SSE2__) && defined(__KERNEL_SSE__)
	                           (ssef*)verts,
#else
	                           verts[0], verts[1], verts[2],
#endif
	                           &u, &v, &t))
	{
		return false;
	}

	/* If no actual hit information is requested, just return here. */
	if(max_hits == 0) {
		return true;
	}

	int hit;
	if(lcg_state) {
		/* Record up to max_hits intersections. */
		for(int i = min(max_hits, local_isect->num_hits) - 1; i >= 0; --i) {
			if(local_isect->hits[i].t == t) {
				return false;
			}
		}

		local_isect->num_hits++;

		if(local_isect->num_hits <= max_hits) {
			hit = local_isect->num_hits - 1;
		}
		else {
			/* Reservoir sampling: if we are at the maximum number of
			 * hits, randomly replace element or skip it.
			 */
			hit = lcg_step_uint(lcg_state) % local_isect->num_hits;

			if(hit >= max_hits)
				return false;
		}
	}
	else {
		/* Record closest intersection only. */
		if(local_isect->num_hits && t > local_isect->hits[0].t) {
			return false;
		}

		hit = 0;
		local_isect->num_hits = 1;
	}

	/* Record intersection. */
	Intersection *isect = &local_isect->hits[hit];
	isect->t = t;
	isect->u = u;
	isect->v = v;
	isect->prim = prim_addr;
	isect->object = object;
	isect->type = PRIMITIVE_MOTION_TRIANGLE;

	/* Record geometric normal. */
	local_isect->Ng[hit] = normalize(cross(verts[1] - verts[0],
	                                       verts[2] - verts[0]));

	return false;
}
#endif  /* __BVH_LOCAL__ */

CCL_NAMESPACE_END
