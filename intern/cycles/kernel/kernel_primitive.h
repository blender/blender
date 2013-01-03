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

#ifndef __KERNEL_ATTRIBUTE_CL__
#define __KERNEL_ATTRIBUTE_CL__

CCL_NAMESPACE_BEGIN

/* attribute lookup */

__device_inline int find_attribute(KernelGlobals *kg, ShaderData *sd, uint id, AttributeElement *elem)
{
	if(sd->object == ~0)
		return (int)ATTR_STD_NOT_FOUND;

#ifdef __OSL__
	if (kg->osl) {
		return OSLShader::find_attribute(kg, sd, id, elem);
	}
	else
#endif
	{
		/* for SVM, find attribute by unique id */
		uint attr_offset = sd->object*kernel_data.bvh.attributes_map_stride;
		attr_offset = (sd->segment == ~0)? attr_offset: attr_offset + ATTR_PRIM_CURVE;
		uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
		
		while(attr_map.x != id) {
			attr_offset += ATTR_PRIM_TYPES;
			attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
		}

		*elem = (AttributeElement)attr_map.y;
		
		/* return result */
		return (attr_map.y == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : (int)attr_map.z;
	}
}

__device float primitive_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
{
#ifdef __HAIR__
	if(sd->segment == ~0)
#endif
		return triangle_attribute_float(kg, sd, elem, offset, dx, dy);
#ifdef __HAIR__
	else
		return curve_attribute_float(kg, sd, elem, offset, dx, dy);
#endif
}

__device float3 primitive_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float3 *dx, float3 *dy)
{
#ifdef __HAIR__
	if(sd->segment == ~0)
#endif
		return triangle_attribute_float3(kg, sd, elem, offset, dx, dy);
#ifdef __HAIR__
	else
		return curve_attribute_float3(kg, sd, elem, offset, dx, dy);
#endif
}

__device float3 primitive_uv(KernelGlobals *kg, ShaderData *sd)
{
	AttributeElement elem_uv;
	int offset_uv = find_attribute(kg, sd, ATTR_STD_UV, &elem_uv);

	if(offset_uv == ATTR_STD_NOT_FOUND)
		return make_float3(0.0f, 0.0f, 0.0f);

	float3 uv = primitive_attribute_float3(kg, sd, elem_uv, offset_uv, NULL, NULL);
	uv.z = 1.0f;
	return uv;
}

__device float3 primitive_tangent(KernelGlobals *kg, ShaderData *sd)
{
#ifdef __HAIR__
	if(sd->segment != ~0)
		return normalize(sd->dPdu);
#endif

	/* try to create spherical tangent from generated coordinates */
	AttributeElement attr_elem;
	int attr_offset = find_attribute(kg, sd, ATTR_STD_GENERATED, &attr_elem);

	if(attr_offset != ATTR_STD_NOT_FOUND) {
		float3 data = primitive_attribute_float3(kg, sd, attr_elem, attr_offset, NULL, NULL);
		data = make_float3(-(data.y - 0.5f), (data.x - 0.5f), 0.0f);
		object_normal_transform(kg, sd, &data);
		return cross(sd->N, normalize(cross(data, sd->N)));;
	}
	else {
		/* otherwise use surface derivatives */
		return normalize(sd->dPdu);
	}
}

/* motion */

__device float4 primitive_motion_vector(KernelGlobals *kg, ShaderData *sd)
{
	float3 motion_pre = sd->P, motion_post = sd->P;

	/* deformation motion */
	AttributeElement elem_pre, elem_post;
	int offset_pre = find_attribute(kg, sd, ATTR_STD_MOTION_PRE, &elem_pre);
	int offset_post = find_attribute(kg, sd, ATTR_STD_MOTION_POST, &elem_post);

	if(offset_pre != ATTR_STD_NOT_FOUND)
		motion_pre = primitive_attribute_float3(kg, sd, elem_pre, offset_pre, NULL, NULL);
	if(offset_post != ATTR_STD_NOT_FOUND)
		motion_post = primitive_attribute_float3(kg, sd, elem_post, offset_post, NULL, NULL);

	/* object motion. note that depending on the mesh having motion vectors, this
	 * transformation was set match the world/object space of motion_pre/post */
	Transform tfm;
	
	tfm = object_fetch_vector_transform(kg, sd->object, OBJECT_VECTOR_MOTION_PRE);
	motion_pre = transform_point(&tfm, motion_pre);

	tfm = object_fetch_vector_transform(kg, sd->object, OBJECT_VECTOR_MOTION_POST);
	motion_post = transform_point(&tfm, motion_post);

	float3 P;

	/* camera motion, for perspective/orthographic motion.pre/post will be a
	 * world-to-raster matrix, for panorama it's world-to-camera */
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

CCL_NAMESPACE_END

#endif /* __KERNEL_ATTRIBUTE_CL__ */
