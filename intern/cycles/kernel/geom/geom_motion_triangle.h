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

/* Time interpolation of vertex positions and normals */

ccl_device_inline int find_attribute_motion(KernelGlobals *kg, int object, uint id, AttributeElement *elem)
{
	/* todo: find a better (faster) solution for this, maybe store offset per object */
	uint attr_offset = object*kernel_data.bvh.attributes_map_stride;
	uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	
	while(attr_map.x != id) {
		attr_offset += ATTR_PRIM_TYPES;
		attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	}

	*elem = (AttributeElement)attr_map.y;
	
	/* return result */
	return (attr_map.y == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : (int)attr_map.z;
}

ccl_device_inline void motion_triangle_verts_for_step(KernelGlobals *kg, uint4 tri_vindex, int offset, int numverts, int numsteps, int step, float3 verts[3])
{
	if(step == numsteps) {
		/* center step: regular vertex location */
		verts[0] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+0));
		verts[1] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+1));
		verts[2] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+2));
	}
	else {
		/* center step not store in this array */
		if(step > numsteps)
			step--;

		offset += step*numverts;

		verts[0] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.x));
		verts[1] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.y));
		verts[2] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.z));
	}
}

ccl_device_inline void motion_triangle_normals_for_step(KernelGlobals *kg, uint4 tri_vindex, int offset, int numverts, int numsteps, int step, float3 normals[3])
{
	if(step == numsteps) {
		/* center step: regular vertex location */
		normals[0] = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.x));
		normals[1] = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.y));
		normals[2] = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.z));
	}
	else {
		/* center step not stored in this array */
		if(step > numsteps)
			step--;

		offset += step*numverts;

		normals[0] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.x));
		normals[1] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.y));
		normals[2] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.z));
	}
}

ccl_device_inline void motion_triangle_vertices(KernelGlobals *kg, int object, int prim, float time, float3 verts[3])
{
	/* get motion info */
	int numsteps, numverts;
	object_motion_info(kg, object, &numsteps, &numverts, NULL);

	/* figure out which steps we need to fetch and their interpolation factor */
	int maxstep = numsteps*2;
	int step = min((int)(time*maxstep), maxstep-1);
	float t = time*maxstep - step;

	/* find attribute */
	AttributeElement elem;
	int offset = find_attribute_motion(kg, object, ATTR_STD_MOTION_VERTEX_POSITION, &elem);
	kernel_assert(offset != ATTR_STD_NOT_FOUND);

	/* fetch vertex coordinates */
	float3 next_verts[3];
	uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);

	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step, verts);
	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_verts);

	/* interpolate between steps */
	verts[0] = (1.0f - t)*verts[0] + t*next_verts[0];
	verts[1] = (1.0f - t)*verts[1] + t*next_verts[1];
	verts[2] = (1.0f - t)*verts[2] + t*next_verts[2];
}

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance. */

ccl_device_inline float3 motion_triangle_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray, float3 verts[3])
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
		Transform tfm = ccl_fetch(sd, ob_itfm);
#  else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#  endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D*t);
		D = normalize_len(D, &t);
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

	/* compute refined position */
	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
		Transform tfm = ccl_fetch(sd, ob_tfm);
#  else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#  endif

		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

/* Same as above, except that isect->t is assumed to be in object space for instancing */

#ifdef __SUBSURFACE__
#  if defined(__KERNEL_CUDA__) && (defined(i386) || defined(_M_IX86))
ccl_device_noinline
#  else
ccl_device_inline
#  endif
float3 motion_triangle_refine_subsurface(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray, float3 verts[3])
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#  ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
#    ifdef __OBJECT_MOTION__
		Transform tfm = ccl_fetch(sd, ob_itfm);
#    else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
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
		Transform tfm = ccl_fetch(sd, ob_tfm);
#    else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#    endif

		P = transform_point(&tfm, P);
	}

	return P;
#  else
	return P + D*t;
#  endif
}
#endif

/* Setup of motion triangle specific parts of ShaderData, moved into this one
 * function to more easily share computation of interpolated positions and
 * normals */

/* return 3 triangle vertex normals */
ccl_device_noinline void motion_triangle_shader_setup(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray, bool subsurface)
{
	/* get shader */
	ccl_fetch(sd, shader) = kernel_tex_fetch(__tri_shader, ccl_fetch(sd, prim));

	/* get motion info */
	int numsteps, numverts;
	object_motion_info(kg, ccl_fetch(sd, object), &numsteps, &numverts, NULL);

	/* figure out which steps we need to fetch and their interpolation factor */
	int maxstep = numsteps*2;
	int step = min((int)(ccl_fetch(sd, time)*maxstep), maxstep-1);
	float t = ccl_fetch(sd, time)*maxstep - step;

	/* find attribute */
	AttributeElement elem;
	int offset = find_attribute_motion(kg, ccl_fetch(sd, object), ATTR_STD_MOTION_VERTEX_POSITION, &elem);
	kernel_assert(offset != ATTR_STD_NOT_FOUND);

	/* fetch vertex coordinates */
	float3 verts[3], next_verts[3];
	uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, ccl_fetch(sd, prim));

	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step, verts);
	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_verts);

	/* interpolate between steps */
	verts[0] = (1.0f - t)*verts[0] + t*next_verts[0];
	verts[1] = (1.0f - t)*verts[1] + t*next_verts[1];
	verts[2] = (1.0f - t)*verts[2] + t*next_verts[2];

	/* compute refined position */
#ifdef __SUBSURFACE__
	if(!subsurface)
#endif
		ccl_fetch(sd, P) = motion_triangle_refine(kg, sd, isect, ray, verts);
#ifdef __SUBSURFACE__
	else
		ccl_fetch(sd, P) = motion_triangle_refine_subsurface(kg, sd, isect, ray, verts);
#endif

	/* compute face normal */
	float3 Ng;
	if(ccl_fetch(sd, flag) & SD_NEGATIVE_SCALE_APPLIED)
		Ng = normalize(cross(verts[2] - verts[0], verts[1] - verts[0]));
	else
		Ng = normalize(cross(verts[1] - verts[0], verts[2] - verts[0]));

	ccl_fetch(sd, Ng) = Ng;
	ccl_fetch(sd, N) = Ng;

	/* compute derivatives of P w.r.t. uv */
#ifdef __DPDU__
	ccl_fetch(sd, dPdu) = (verts[0] - verts[2]);
	ccl_fetch(sd, dPdv) = (verts[1] - verts[2]);
#endif

	/* compute smooth normal */
	if(ccl_fetch(sd, shader) & SHADER_SMOOTH_NORMAL) {
		/* find attribute */
		AttributeElement elem;
		int offset = find_attribute_motion(kg, ccl_fetch(sd, object), ATTR_STD_MOTION_VERTEX_NORMAL, &elem);
		kernel_assert(offset != ATTR_STD_NOT_FOUND);

		/* fetch vertex coordinates */
		float3 normals[3], next_normals[3];
		motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step, normals);
		motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_normals);

		/* interpolate between steps */
		normals[0] = (1.0f - t)*normals[0] + t*next_normals[0];
		normals[1] = (1.0f - t)*normals[1] + t*next_normals[1];
		normals[2] = (1.0f - t)*normals[2] + t*next_normals[2];

		/* interpolate between vertices */
		float u = ccl_fetch(sd, u);
		float v = ccl_fetch(sd, v);
		float w = 1.0f - u - v;
		ccl_fetch(sd, N) = (u*normals[0] + v*normals[1] + w*normals[2]);
	}
}

/* Ray intersection. We simply compute the vertex positions at the given ray
 * time and do a ray intersection with the resulting triangle */

ccl_device_inline bool motion_triangle_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 dir, float time, uint visibility, int object, int triAddr)
{
	/* primitive index for vertex location lookup */
	int prim = kernel_tex_fetch(__prim_index, triAddr);
	int fobject = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, triAddr): object;

	/* get vertex locations for intersection */
	float3 verts[3];
	motion_triangle_vertices(kg, fobject, prim, time, verts);

	/* ray-triangle intersection, unoptimized */
	float t, u, v;

	if(ray_triangle_intersect_uv(P, dir, isect->t, verts[2], verts[0], verts[1], &u, &v, &t)) {
#ifdef __VISIBILITY_FLAG__
		/* visibility flag test. we do it here under the assumption
		 * that most triangles are culled by node flags */
		if(kernel_tex_fetch(__prim_visibility, triAddr) & visibility)
#endif
		{
			isect->t = t;
			isect->u = u;
			isect->v = v;
			isect->prim = triAddr;
			isect->object = object;
			isect->type = PRIMITIVE_MOTION_TRIANGLE;
		
			return true;
		}
	}

	return false;
}

/* Special ray intersection routines for subsurface scattering. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point. */

#ifdef __SUBSURFACE__
ccl_device_inline void motion_triangle_intersect_subsurface(
        KernelGlobals *kg,
        SubsurfaceIntersection *ss_isect,
        float3 P,
        float3 dir,
        float time,
        int object,
        int triAddr,
        float tmax,
        uint *lcg_state,
        int max_hits)
{
	/* primitive index for vertex location lookup */
	int prim = kernel_tex_fetch(__prim_index, triAddr);
	int fobject = (object == OBJECT_NONE)? kernel_tex_fetch(__prim_object, triAddr): object;

	/* get vertex locations for intersection */
	float3 verts[3];
	motion_triangle_vertices(kg, fobject, prim, time, verts);

	/* ray-triangle intersection, unoptimized */
	float t, u, v;

	if(ray_triangle_intersect_uv(P, dir, tmax, verts[2], verts[0], verts[1], &u, &v, &t)) {
		for(int i = min(max_hits, ss_isect->num_hits) - 1; i >= 0; --i) {
			if(ss_isect->hits[i].t == t) {
				return;
			}
		}

		ss_isect->num_hits++;

		int hit;

		if(ss_isect->num_hits <= max_hits) {
			hit = ss_isect->num_hits - 1;
		}
		else {
			/* reservoir sampling: if we are at the maximum number of
			 * hits, randomly replace element or skip it */
			hit = lcg_step_uint(lcg_state) % ss_isect->num_hits;

			if(hit >= max_hits)
				return;
		}

		/* record intersection */
		Intersection *isect = &ss_isect->hits[hit];
		isect->t = t;
		isect->u = u;
		isect->v = v;
		isect->prim = triAddr;
		isect->object = object;
		isect->type = PRIMITIVE_MOTION_TRIANGLE;

		/* Record geometric normal. */
		ss_isect->Ng[hit] = normalize(cross(verts[1] - verts[0],
		                                    verts[2] - verts[0]));
	}
}
#endif

CCL_NAMESPACE_END

