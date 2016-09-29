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

/* Triangle Primitive
 *
 * Basic triangle with 3 vertices is used to represent mesh surfaces. For BVH
 * ray intersection we use a precomputed triangle storage to accelerate
 * intersection at the cost of more memory usage */

CCL_NAMESPACE_BEGIN

/* normal on triangle  */
ccl_device_inline float3 triangle_normal(KernelGlobals *kg, ShaderData *sd)
{
	/* load triangle vertices */
	const uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, ccl_fetch(sd, prim));
	const float3 v0 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+0));
	const float3 v1 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+1));
	const float3 v2 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+2));

	/* return normal */
	if(ccl_fetch(sd, flag) & SD_NEGATIVE_SCALE_APPLIED)
		return normalize(cross(v2 - v0, v1 - v0));
	else
		return normalize(cross(v1 - v0, v2 - v0));
}

/* point and normal on triangle  */
ccl_device_inline void triangle_point_normal(KernelGlobals *kg, int object, int prim, float u, float v, float3 *P, float3 *Ng, int *shader)
{
	/* load triangle vertices */
	const uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);
	float3 v0 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+0));
	float3 v1 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+1));
	float3 v2 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+2));

	/* compute point */
	float t = 1.0f - u - v;
	*P = (u*v0 + v*v1 + t*v2);

	/* get object flags */
	int object_flag = kernel_tex_fetch(__object_flag, object);

	/* compute normal */
	if(object_flag & SD_NEGATIVE_SCALE_APPLIED)
		*Ng = normalize(cross(v2 - v0, v1 - v0));
	else
		*Ng = normalize(cross(v1 - v0, v2 - v0));

	/* shader`*/
	*shader = kernel_tex_fetch(__tri_shader, prim);
}

/* Triangle vertex locations */

ccl_device_inline void triangle_vertices(KernelGlobals *kg, int prim, float3 P[3])
{
	const uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);
	P[0] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+0));
	P[1] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+1));
	P[2] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+2));
}

/* Interpolate smooth vertex normal from vertices */

ccl_device_inline float3 triangle_smooth_normal(KernelGlobals *kg, int prim, float u, float v)
{
	/* load triangle vertices */
	const uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);
	float3 n0 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.x));
	float3 n1 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.y));
	float3 n2 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.z));

	return normalize((1.0f - u - v)*n2 + u*n0 + v*n1);
}

/* Ray differentials on triangle */

ccl_device_inline void triangle_dPdudv(KernelGlobals *kg, int prim, ccl_addr_space float3 *dPdu, ccl_addr_space float3 *dPdv)
{
	/* fetch triangle vertex coordinates */
	const uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);
	const float3 p0 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+0));
	const float3 p1 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+1));
	const float3 p2 = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+2));

	/* compute derivatives of P w.r.t. uv */
	*dPdu = (p0 - p2);
	*dPdv = (p1 - p2);
}

/* Reading attributes on various triangle elements */

ccl_device float triangle_attribute_float(KernelGlobals *kg, const ShaderData *sd, const AttributeDescriptor desc, float *dx, float *dy)
{
	if(desc.element == ATTR_ELEMENT_FACE) {
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;

		return kernel_tex_fetch(__attributes_float, desc.offset + ccl_fetch(sd, prim));
	}
	else if(desc.element == ATTR_ELEMENT_VERTEX || desc.element == ATTR_ELEMENT_VERTEX_MOTION) {
		uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, ccl_fetch(sd, prim));

		float f0 = kernel_tex_fetch(__attributes_float, desc.offset + tri_vindex.x);
		float f1 = kernel_tex_fetch(__attributes_float, desc.offset + tri_vindex.y);
		float f2 = kernel_tex_fetch(__attributes_float, desc.offset + tri_vindex.z);

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = ccl_fetch(sd, du).dx*f0 + ccl_fetch(sd, dv).dx*f1 - (ccl_fetch(sd, du).dx + ccl_fetch(sd, dv).dx)*f2;
		if(dy) *dy = ccl_fetch(sd, du).dy*f0 + ccl_fetch(sd, dv).dy*f1 - (ccl_fetch(sd, du).dy + ccl_fetch(sd, dv).dy)*f2;
#endif

		return ccl_fetch(sd, u)*f0 + ccl_fetch(sd, v)*f1 + (1.0f - ccl_fetch(sd, u) - ccl_fetch(sd, v))*f2;
	}
	else if(desc.element == ATTR_ELEMENT_CORNER) {
		int tri = desc.offset + ccl_fetch(sd, prim)*3;
		float f0 = kernel_tex_fetch(__attributes_float, tri + 0);
		float f1 = kernel_tex_fetch(__attributes_float, tri + 1);
		float f2 = kernel_tex_fetch(__attributes_float, tri + 2);

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = ccl_fetch(sd, du).dx*f0 + ccl_fetch(sd, dv).dx*f1 - (ccl_fetch(sd, du).dx + ccl_fetch(sd, dv).dx)*f2;
		if(dy) *dy = ccl_fetch(sd, du).dy*f0 + ccl_fetch(sd, dv).dy*f1 - (ccl_fetch(sd, du).dy + ccl_fetch(sd, dv).dy)*f2;
#endif

		return ccl_fetch(sd, u)*f0 + ccl_fetch(sd, v)*f1 + (1.0f - ccl_fetch(sd, u) - ccl_fetch(sd, v))*f2;
	}
	else {
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;

		return 0.0f;
	}
}

ccl_device float3 triangle_attribute_float3(KernelGlobals *kg, const ShaderData *sd, const AttributeDescriptor desc, float3 *dx, float3 *dy)
{
	if(desc.element == ATTR_ELEMENT_FACE) {
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);

		return float4_to_float3(kernel_tex_fetch(__attributes_float3, desc.offset + ccl_fetch(sd, prim)));
	}
	else if(desc.element == ATTR_ELEMENT_VERTEX || desc.element == ATTR_ELEMENT_VERTEX_MOTION) {
		uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, ccl_fetch(sd, prim));

		float3 f0 = float4_to_float3(kernel_tex_fetch(__attributes_float3, desc.offset + tri_vindex.x));
		float3 f1 = float4_to_float3(kernel_tex_fetch(__attributes_float3, desc.offset + tri_vindex.y));
		float3 f2 = float4_to_float3(kernel_tex_fetch(__attributes_float3, desc.offset + tri_vindex.z));

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = ccl_fetch(sd, du).dx*f0 + ccl_fetch(sd, dv).dx*f1 - (ccl_fetch(sd, du).dx + ccl_fetch(sd, dv).dx)*f2;
		if(dy) *dy = ccl_fetch(sd, du).dy*f0 + ccl_fetch(sd, dv).dy*f1 - (ccl_fetch(sd, du).dy + ccl_fetch(sd, dv).dy)*f2;
#endif

		return ccl_fetch(sd, u)*f0 + ccl_fetch(sd, v)*f1 + (1.0f - ccl_fetch(sd, u) - ccl_fetch(sd, v))*f2;
	}
	else if(desc.element == ATTR_ELEMENT_CORNER || desc.element == ATTR_ELEMENT_CORNER_BYTE) {
		int tri = desc.offset + ccl_fetch(sd, prim)*3;
		float3 f0, f1, f2;

		if(desc.element == ATTR_ELEMENT_CORNER) {
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
		if(dx) *dx = ccl_fetch(sd, du).dx*f0 + ccl_fetch(sd, dv).dx*f1 - (ccl_fetch(sd, du).dx + ccl_fetch(sd, dv).dx)*f2;
		if(dy) *dy = ccl_fetch(sd, du).dy*f0 + ccl_fetch(sd, dv).dy*f1 - (ccl_fetch(sd, du).dy + ccl_fetch(sd, dv).dy)*f2;
#endif

		return ccl_fetch(sd, u)*f0 + ccl_fetch(sd, v)*f1 + (1.0f - ccl_fetch(sd, u) - ccl_fetch(sd, v))*f2;
	}
	else {
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);

		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

CCL_NAMESPACE_END
