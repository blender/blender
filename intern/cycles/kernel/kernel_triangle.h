/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

/* Point on triangle for Moller-Trumbore triangles */
__device_inline float3 triangle_point_MT(KernelGlobals *kg, int tri_index, float u, float v)
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

/* Sample point on triangle */
__device_inline float3 triangle_sample_MT(KernelGlobals *kg, int tri_index, float randu, float randv)
{
	/* compute point */
	randu = sqrtf(randu);

	float u = 1.0f - randu;
	float v = randv*randu;

	return triangle_point_MT(kg, tri_index, u, v);
}

/* Normal for Moller-Trumbore triangles */
__device_inline float3 triangle_normal_MT(KernelGlobals *kg, int tri_index, int *shader)
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

__device_inline float3 triangle_smooth_normal(KernelGlobals *kg, int tri_index, float u, float v)
{
	/* load triangle vertices */
	float3 tri_vindex = float4_to_float3(kernel_tex_fetch(__tri_vindex, tri_index));

	float3 n0 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.x)));
	float3 n1 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.y)));
	float3 n2 = float4_to_float3(kernel_tex_fetch(__tri_vnormal, __float_as_int(tri_vindex.z)));

	return normalize((1.0f - u - v)*n2 + u*n0 + v*n1);
}

__device_inline void triangle_dPdudv(KernelGlobals *kg, float3 *dPdu, float3 *dPdv, int tri)
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

__device float triangle_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
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

__device float3 triangle_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float3 *dx, float3 *dy)
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

/* motion */

__device int triangle_find_attribute(KernelGlobals *kg, ShaderData *sd, uint id)
{
	/* find attribute by unique id */
	uint attr_offset = sd->object*kernel_data.bvh.attributes_map_stride;
	uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);

	while(attr_map.x != id)
		attr_map = kernel_tex_fetch(__attributes_map, ++attr_offset);

	/* return result */
	return (attr_map.y == ATTR_ELEMENT_NONE)? ATTR_STD_NOT_FOUND: attr_map.z;
}

__device float4 triangle_motion_vector(KernelGlobals *kg, ShaderData *sd)
{
	float3 motion_pre = sd->P, motion_post = sd->P;

	/* deformation motion */
	int offset_pre = triangle_find_attribute(kg, sd, ATTR_STD_MOTION_PRE);
	int offset_post = triangle_find_attribute(kg, sd, ATTR_STD_MOTION_POST);

	if(offset_pre != ATTR_STD_NOT_FOUND)
		motion_pre = triangle_attribute_float3(kg, sd, ATTR_ELEMENT_VERTEX, offset_pre, NULL, NULL);
	if(offset_post != ATTR_STD_NOT_FOUND)
		motion_post = triangle_attribute_float3(kg, sd, ATTR_ELEMENT_VERTEX, offset_post, NULL, NULL);

	/* object motion. note that depending on the mesh having motion vectors, this
	   transformation was set match the world/object space of motion_pre/post */
	Transform tfm;
	
	tfm = object_fetch_transform(kg, sd->object, TIME_INVALID, OBJECT_TRANSFORM_MOTION_PRE);
	motion_pre = transform_point(&tfm, motion_pre);

	tfm = object_fetch_transform(kg, sd->object, TIME_INVALID, OBJECT_TRANSFORM_MOTION_POST);
	motion_post = transform_point(&tfm, motion_post);

	float3 P;

	/* camera motion, for perspective/orthographic motion.pre/post will be a
	   world-to-raster matrix, for panorama it's world-to-camera */
	if (kernel_data.cam.type != CAMERA_PANORAMA) {
		tfm = kernel_data.cam.worldtoraster;
		P = transform_perspective(&tfm, sd->P);

		tfm = kernel_data.cam.motion.pre;
		motion_pre = transform_perspective(&tfm, motion_pre);

		tfm = kernel_data.cam.motion.post;
		motion_post = transform_perspective(&tfm, motion_post);
	}
	else {
		tfm = kernel_data.cam.worldtocamera;
		P = normalize(transform_point(&tfm, sd->P));
		P = float2_to_float3(direction_to_panorama(kg, P));
		P.x *= kernel_data.cam.width;
		P.y *= kernel_data.cam.height;

		tfm = kernel_data.cam.motion.pre;
		motion_pre = normalize(transform_point(&tfm, motion_pre));
		motion_pre = float2_to_float3(direction_to_panorama(kg, motion_pre));
		motion_pre.x *= kernel_data.cam.width;
		motion_pre.y *= kernel_data.cam.height;

		tfm = kernel_data.cam.motion.post;
		motion_post = normalize(transform_point(&tfm, motion_post));
		motion_post = float2_to_float3(direction_to_panorama(kg, motion_post));
		motion_post.x *= kernel_data.cam.width;
		motion_post.y *= kernel_data.cam.height;
	}

	motion_pre = motion_pre - P;
	motion_post = P - motion_post;

	return make_float4(motion_pre.x, motion_pre.y, motion_post.x, motion_post.y);
}

__device float3 triangle_uv(KernelGlobals *kg, ShaderData *sd)
{
	int offset_uv = triangle_find_attribute(kg, sd, ATTR_STD_UV);

	if(offset_uv == ATTR_STD_NOT_FOUND)
		return make_float3(0.0f, 0.0f, 0.0f);

	float3 uv = triangle_attribute_float3(kg, sd, ATTR_ELEMENT_CORNER, offset_uv, NULL, NULL);
	uv.z = 1.0f;
	return uv;
}

CCL_NAMESPACE_END

