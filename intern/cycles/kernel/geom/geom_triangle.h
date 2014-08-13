/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
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

/* Triangle Primitive
 *
 * Basic triangle with 3 vertices is used to represent mesh surfaces. For BVH
 * ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage */

CCL_NAMESPACE_BEGIN

/* Refine triangle intersection to more precise hit point. For rays that travel
 * far the precision is often not so good, this reintersects the primitive from
 * a closer distance. */

ccl_device_inline float3 triangle_refine(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D*t);
		D = normalize_len(D, &t);
	}

	P = P + D*t;

	float4 v00 = kernel_tex_fetch(__tri_woop, isect->prim*TRI_NODE_SIZE+0);
	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(D.x*v00.x + D.y*v00.y + D.z*v00.z);
	float rt = Oz * invDz;

	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

/* same as above, except that isect->t is assumed to be in object space for instancing */
ccl_device_inline float3 triangle_refine_subsurface(KernelGlobals *kg, ShaderData *sd, const Intersection *isect, const Ray *ray)
{
	float3 P = ray->P;
	float3 D = ray->D;
	float t = isect->t;

#ifdef __INTERSECTION_REFINE__
	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_itfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
		D = transform_direction(&tfm, D);
		D = normalize(D);
	}

	P = P + D*t;

	float4 v00 = kernel_tex_fetch(__tri_woop, isect->prim*TRI_NODE_SIZE+0);
	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(D.x*v00.x + D.y*v00.y + D.z*v00.z);
	float rt = Oz * invDz;

	P = P + D*rt;

	if(isect->object != OBJECT_NONE) {
#ifdef __OBJECT_MOTION__
		Transform tfm = sd->ob_tfm;
#else
		Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#endif

		P = transform_point(&tfm, P);
	}

	return P;
#else
	return P + D*t;
#endif
}

/* normal on triangle  */
ccl_device_inline float3 triangle_normal(KernelGlobals *kg, ShaderData *sd)
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, sd->prim));

	float3 v0 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	float3 v1 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	float3 v2 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));
	
	/* return normal */
	if(sd->flag & SD_NEGATIVE_SCALE_APPLIED)
		return normalize(cross(v2 - v0, v1 - v0));
	else
		return normalize(cross(v1 - v0, v2 - v0));
}

/* point and normal on triangle  */
ccl_device_inline void triangle_point_normal(KernelGlobals *kg, int object, int prim, float u, float v, float3 *P, float3 *Ng, int *shader)
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, prim));

	float3 v0 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	float3 v1 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	float3 v2 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));

	/* compute point */
	float t = 1.0f - u - v;
	*P = (u*v0 + v*v1 + t*v2);

	/* compute normal */
	int object_flag = kernel_tex_fetch(__object_flag, object);
	if(object_flag & SD_NEGATIVE_SCALE_APPLIED)
		*Ng = normalize(cross(v2 - v0, v1 - v0));
	else
		*Ng = normalize(cross(v1 - v0, v2 - v0));

	/* shader`*/
	*shader = __float_as_int(kernel_tex_fetch(__tri_shader, prim));
}

/* Triangle vertex locations */

ccl_device_inline void triangle_vertices(KernelGlobals *kg, int prim, float3 P[3])
{
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, prim));

	P[0] = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	P[1] = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	P[2] = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));
}

/* Interpolate smooth vertex normal from vertices */

ccl_device_inline float3 triangle_smooth_normal(KernelGlobals *kg, int prim, float u, float v)
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, prim));

	float3 n0 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.x)));
	float3 n1 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.y)));
	float3 n2 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.z)));

	return normalize((1.0f - u - v)*n2 + u*n0 + v*n1);
}

/* Ray differentials on triangle */

ccl_device_inline void triangle_dPdudv(KernelGlobals *kg, int prim, float3 *dPdu, float3 *dPdv)
{
	/* fetch triangle vertex coordinates */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, prim));

	float3 p0 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	float3 p1 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	float3 p2 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));

	/* compute derivatives of P w.r.t. uv */
	*dPdu = (p0 - p2);
	*dPdv = (p1 - p2);
}

/* Reading attributes on various triangle elements */

ccl_device float triangle_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
{
	if(elem == ATTR_ELEMENT_FACE) {
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;

		return kernel_tex_fetch(__attributes_float, offset + sd->prim);
	}
	else if(elem == ATTR_ELEMENT_VERTEX || elem == ATTR_ELEMENT_VERTEX_MOTION) {
		float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, sd->prim));

		float f0 = kernel_tex_fetch(__attributes_float, offset + __float_as_int(tri_vindex.x));
		float f1 = kernel_tex_fetch(__attributes_float, offset + __float_as_int(tri_vindex.y));
		float f2 = kernel_tex_fetch(__attributes_float, offset + __float_as_int(tri_vindex.z));

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*f0 + sd->dv.dx*f1 - (sd->du.dx + sd->dv.dx)*f2;
		if(dy) *dy = sd->du.dy*f0 + sd->dv.dy*f1 - (sd->du.dy + sd->dv.dy)*f2;
#endif

		return sd->u*f0 + sd->v*f1 + (1.0f - sd->u - sd->v)*f2;
	}
	else if(elem == ATTR_ELEMENT_CORNER) {
		int tri = offset + sd->prim*3;
		float f0 = kernel_tex_fetch(__attributes_float, tri + 0);
		float f1 = kernel_tex_fetch(__attributes_float, tri + 1);
		float f2 = kernel_tex_fetch(__attributes_float, tri + 2);

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*f0 + sd->dv.dx*f1 - (sd->du.dx + sd->dv.dx)*f2;
		if(dy) *dy = sd->du.dy*f0 + sd->dv.dy*f1 - (sd->du.dy + sd->dv.dy)*f2;
#endif

		return sd->u*f0 + sd->v*f1 + (1.0f - sd->u - sd->v)*f2;
	}
	else {
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;

		return 0.0f;
	}
}

ccl_device float3 triangle_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float3 *dx, float3 *dy)
{
	if(elem == ATTR_ELEMENT_FACE) {
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);

		return float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + sd->prim));
	}
	else if(elem == ATTR_ELEMENT_VERTEX || elem == ATTR_ELEMENT_VERTEX_MOTION) {
		float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, sd->prim));

		float3 f0 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + __float_as_int(tri_vindex.x)));
		float3 f1 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + __float_as_int(tri_vindex.y)));
		float3 f2 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + __float_as_int(tri_vindex.z)));

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*f0 + sd->dv.dx*f1 - (sd->du.dx + sd->dv.dx)*f2;
		if(dy) *dy = sd->du.dy*f0 + sd->dv.dy*f1 - (sd->du.dy + sd->dv.dy)*f2;
#endif

		return sd->u*f0 + sd->v*f1 + (1.0f - sd->u - sd->v)*f2;
	}
	else if(elem == ATTR_ELEMENT_CORNER || elem == ATTR_ELEMENT_CORNER_BYTE) {
		int tri = offset + sd->prim*3;
		float3 f0, f1, f2;

		if(elem == ATTR_ELEMENT_CORNER) {
			f0 = float4_to_float3(kernel_tex_fetch(__attributes_float3, tri + 0));
			f1 = float4_to_float3(kernel_tex_fetch(__attributes_float3, tri + 1));
			f2 = float4_to_float3(kernel_tex_fetch(__attributes_float3, tri + 2));
		}
		else {
			f0 = color_byte_to_float(kernel_tex_fetch(__attributes_uchar4, tri + 0));
			f1 = color_byte_to_float(kernel_tex_fetch(__attributes_uchar4, tri + 1));
			f2 = color_byte_to_float(kernel_tex_fetch(__attributes_uchar4, tri + 2));
		}

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*f0 + sd->dv.dx*f1 - (sd->du.dx + sd->dv.dx)*f2;
		if(dy) *dy = sd->du.dy*f0 + sd->dv.dy*f1 - (sd->du.dy + sd->dv.dy)*f2;
#endif

		return sd->u*f0 + sd->v*f1 + (1.0f - sd->u - sd->v)*f2;
	}
	else {
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);

		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

/* Ray-Triangle intersection for BVH traversal
 *
 * Based on Sven Woop's algorithm with precomputed triangle storage */

ccl_device_inline bool triangle_intersect(KernelGlobals *kg, Intersection *isect,
	float3 P, float3 dir, uint visibility, int object, int triAddr)
{
	/* compute and check intersection t-value */
	float4 v00 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+0);
	float4 v11 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+1);

	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(dir.x*v00.x + dir.y*v00.y + dir.z*v00.z);
	float t = Oz * invDz;

	if(t > 0.0f && t < isect->t) {
		/* compute and check barycentric u */
		float Ox = v11.w + P.x*v11.x + P.y*v11.y + P.z*v11.z;
		float Dx = dir.x*v11.x + dir.y*v11.y + dir.z*v11.z;
		float u = Ox + t*Dx;

		if(u >= 0.0f) {
			/* compute and check barycentric v */
			float4 v22 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+2);
			float Oy = v22.w + P.x*v22.x + P.y*v22.y + P.z*v22.z;
			float Dy = dir.x*v22.x + dir.y*v22.y + dir.z*v22.z;
			float v = Oy + t*Dy;

			if(v >= 0.0f && u + v <= 1.0f) {
#ifdef __VISIBILITY_FLAG__
				/* visibility flag test. we do it here under the assumption
				 * that most triangles are culled by node flags */
				if(kernel_tex_fetch(__prim_visibility, triAddr) & visibility)
#endif
				{
					/* record intersection */
					isect->prim = triAddr;
					isect->object = object;
					isect->type = PRIMITIVE_TRIANGLE;
					isect->u = u;
					isect->v = v;
					isect->t = t;
					return true;
				}
			}
		}
	}

	return false;
}

/* Special ray intersection routines for subsurface scattering. In that case we
 * only want to intersect with primitives in the same object, and if case of
 * multiple hits we pick a single random primitive as the intersection point. */

#ifdef __SUBSURFACE__
ccl_device_inline void triangle_intersect_subsurface(KernelGlobals *kg, Intersection *isect_array,
	float3 P, float3 dir, int object, int triAddr, float tmax, uint *num_hits, uint *lcg_state, int max_hits)
{
	/* compute and check intersection t-value */
	float4 v00 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+0);
	float4 v11 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+1);

	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(dir.x*v00.x + dir.y*v00.y + dir.z*v00.z);
	float t = Oz * invDz;

	if(t > 0.0f && t < tmax) {
		/* compute and check barycentric u */
		float Ox = v11.w + P.x*v11.x + P.y*v11.y + P.z*v11.z;
		float Dx = dir.x*v11.x + dir.y*v11.y + dir.z*v11.z;
		float u = Ox + t*Dx;

		if(u >= 0.0f) {
			/* compute and check barycentric v */
			float4 v22 = kernel_tex_fetch(__tri_woop, triAddr*TRI_NODE_SIZE+2);
			float Oy = v22.w + P.x*v22.x + P.y*v22.y + P.z*v22.z;
			float Dy = dir.x*v22.x + dir.y*v22.y + dir.z*v22.z;
			float v = Oy + t*Dy;

			if(v >= 0.0f && u + v <= 1.0f) {
				(*num_hits)++;

				int hit;

				if(*num_hits <= max_hits) {
					hit = *num_hits - 1;
				}
				else {
					/* reservoir sampling: if we are at the maximum number of
					 * hits, randomly replace element or skip it */
					hit = lcg_step_uint(lcg_state) % *num_hits;

					if(hit >= max_hits)
						return;
				}

				/* record intersection */
				Intersection *isect = &isect_array[hit];
				isect->prim = triAddr;
				isect->object = object;
				isect->type = PRIMITIVE_TRIANGLE;
				isect->u = u;
				isect->v = v;
				isect->t = t;
			}
		}
	}
}
#endif

CCL_NAMESPACE_END

