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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* Point on triangle for Moller-Trumbore triangles */
ccl_device_inline float3 triangle_point_MT(KernelGlobals *kg, int tri_index, float u, float v)
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, tri_index));

	float3 v0 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	float3 v1 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	float3 v2 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));

	/* compute point */
	float t = 1.0f - u - v;
	return (u*v0 + v*v1 + t*v2);
}

/* Normal for Moller-Trumbore triangles */
ccl_device_inline float3 triangle_normal_MT(KernelGlobals *kg, int tri_index, int *shader)
{
#if 0
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, tri_index));

	float3 v0 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	float3 v1 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	float3 v2 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));

	/* compute normal */
	return normalize(cross(v2 - v0, v1 - v0));
#else
	float4 Nm = kernel_tex_fetch(__tri_normal, tri_index);
	*shader = __float_as_int(Nm.w);
	return make_float3(Nm.x, Nm.y, Nm.z);
#endif
}

/* Return 3 triangle vertex locations */
ccl_device_inline void triangle_vertices(KernelGlobals *kg, int tri_index, float3 P[3])
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, tri_index));

	P[0] = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	P[1] = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	P[2] = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));
}

ccl_device_inline float3 triangle_smooth_normal(KernelGlobals *kg, int tri_index, float u, float v)
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, tri_index));

	float3 n0 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.x)));
	float3 n1 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.y)));
	float3 n2 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.z)));

	return normalize((1.0f - u - v)*n2 + u*n0 + v*n1);
}

ccl_device_inline void triangle_dPdudv(KernelGlobals *kg, float3 *dPdu, float3 *dPdv, int tri)
{
	/* fetch triangle vertex coordinates */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, tri));

	float3 p0 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.x)));
	float3 p1 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.y)));
	float3 p2 = float4_to_float3(kernel_tex_fetch(__tri_verts, __float_as_int(tri_vindex.z)));

	/* compute derivatives of P w.r.t. uv */
	*dPdu = (p0 - p2);
	*dPdv = (p1 - p2);
}

/* attributes */

ccl_device float triangle_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
{
	if(elem == ATTR_ELEMENT_FACE) {
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;

		return kernel_tex_fetch(__attributes_float, offset + sd->prim);
	}
	else if(elem == ATTR_ELEMENT_VERTEX) {
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
	else if(elem == ATTR_ELEMENT_VERTEX) {
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
	else if(elem == ATTR_ELEMENT_CORNER) {
		int tri = offset + sd->prim*3;
		float3 f0 = float4_to_float3(kernel_tex_fetch(__attributes_float3, tri + 0));
		float3 f1 = float4_to_float3(kernel_tex_fetch(__attributes_float3, tri + 1));
		float3 f2 = float4_to_float3(kernel_tex_fetch(__attributes_float3, tri + 2));

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

CCL_NAMESPACE_END

