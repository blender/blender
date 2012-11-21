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

/* Texture Coordinate Node */

__device_inline float3 svm_background_position(KernelGlobals *kg, float3 P)
{
	Transform cameratoworld = kernel_data.cam.cameratoworld;
	float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);

	return camP + P;
}

__device_inline float3 svm_world_to_ndc(KernelGlobals *kg, ShaderData *sd, float3 P)
{
	if(kernel_data.cam.type != CAMERA_PANORAMA) {
		if(sd->object == ~0)
			P = svm_background_position(kg, P);

		Transform tfm = kernel_data.cam.worldtondc;
		return transform_perspective(&tfm, P);
	}
	else {
		Transform tfm = kernel_data.cam.worldtocamera;

		if(sd->object != ~0)
			P = normalize(transform_point(&tfm, P));
		else
			P = normalize(transform_direction(&tfm, P));

		float2 uv = direction_to_panorama(kg, P);

		return make_float3(uv.x, uv.y, 0.0f);
	}
}

__device void svm_node_tex_coord(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				data = sd->P;
				object_inverse_position_transform(kg, sd, &data);
			}
			else
				data = sd->P;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				data = sd->N;
				object_inverse_normal_transform(kg, sd, &data);
			}
			else
				data = sd->N;
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != ~0)
				data = transform_point(&tfm, sd->P);
			else
				data = transform_point(&tfm, svm_background_position(kg, sd->P));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			data = svm_world_to_ndc(kg, sd, sd->P);
			data.z = 0.0f;
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = 2.0f*dot(sd->N, sd->I)*sd->N - sd->I;
			else
				data = sd->I;
			break;
		}
		case NODE_TEXCO_DUPLI_GENERATED: {
			data = object_dupli_generated(kg, sd->object);
			break;
		}
		case NODE_TEXCO_DUPLI_UV: {
			data = object_dupli_uv(kg, sd->object);
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
}

__device void svm_node_tex_coord_bump_dx(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				data = sd->P + sd->dP.dx;
				object_inverse_position_transform(kg, sd, &data);
			}
			else
				data = sd->P + sd->dP.dx;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				data = sd->N;
				object_inverse_normal_transform(kg, sd, &data);
			}
			else
				data = sd->N;
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != ~0)
				data = transform_point(&tfm, sd->P + sd->dP.dx);
			else
				data = transform_point(&tfm, svm_background_position(kg, sd->P + sd->dP.dx));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			data = svm_world_to_ndc(kg, sd, sd->P + sd->dP.dx);
			data.z = 0.0f;
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = 2.0f*dot(sd->N, sd->I)*sd->N - sd->I;
			else
				data = sd->I;
			break;
		}
		case NODE_TEXCO_DUPLI_GENERATED: {
			data = object_dupli_generated(kg, sd->object);
			break;
		}
		case NODE_TEXCO_DUPLI_UV: {
			data = object_dupli_uv(kg, sd->object);
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_tex_coord(kg, sd, stack, type, out_offset);
#endif
}

__device void svm_node_tex_coord_bump_dy(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				data = sd->P + sd->dP.dy;
				object_inverse_position_transform(kg, sd, &data);
			}
			else
				data = sd->P + sd->dP.dy;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				data = sd->N;
				object_inverse_normal_transform(kg, sd, &data);
			}
			else
				data = sd->N;
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != ~0)
				data = transform_point(&tfm, sd->P + sd->dP.dy);
			else
				data = transform_point(&tfm, svm_background_position(kg, sd->P + sd->dP.dy));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			data = svm_world_to_ndc(kg, sd, sd->P + sd->dP.dy);
			data.z = 0.0f;
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = 2.0f*dot(sd->N, sd->I)*sd->N - sd->I;
			else
				data = sd->I;
			break;
		}
		case NODE_TEXCO_DUPLI_GENERATED: {
			data = object_dupli_generated(kg, sd->object);
			break;
		}
		case NODE_TEXCO_DUPLI_UV: {
			data = object_dupli_uv(kg, sd->object);
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_tex_coord(kg, sd, stack, type, out_offset);
#endif
}

__device void svm_node_normal_map(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint color_offset, strength_offset, normal_offset, space;
	decode_node_uchar4(node.y, &color_offset, &strength_offset, &normal_offset, &space);

	float3 color = stack_load_float3(stack, color_offset);
	color = 2.0f*make_float3(color.x - 0.5f, color.y - 0.5f, color.z - 0.5f);

	float3 N;

	if(space == NODE_NORMAL_MAP_TANGENT) {
		/* tangent space */
		if(sd->object == ~0) {
			stack_store_float3(stack, normal_offset, make_float3(0.0f, 0.0f, 0.0f));
			return;
		}

		/* first try to get tangent attribute */
		int attr_offset = find_attribute(kg, sd, node.z);
		int attr_sign_offset = find_attribute(kg, sd, node.w);

		if(attr_offset == ATTR_STD_NOT_FOUND || attr_sign_offset == ATTR_STD_NOT_FOUND) {
			stack_store_float3(stack, normal_offset, make_float3(0.0f, 0.0f, 0.0f));
			return;
		}

		/* ensure orthogonal and normalized (interpolation breaks it) */
		float3 tangent = triangle_attribute_float3(kg, sd, ATTR_ELEMENT_CORNER, attr_offset, NULL, NULL);
		float sign = triangle_attribute_float(kg, sd, ATTR_ELEMENT_CORNER, attr_sign_offset, NULL, NULL);

		object_normal_transform(kg, sd, &tangent);
		tangent = cross(sd->N, normalize(cross(tangent, sd->N)));;

		float3 B = sign * cross(sd->N, tangent);
		N = normalize(color.x * tangent + color.y * B + color.z * sd->N);
	}
	else {
		/* object, world space */
		N = color;

		if(space == NODE_NORMAL_MAP_OBJECT)
			object_normal_transform(kg, sd, &N);

		N = normalize(N);
	}

	float strength = stack_load_float(stack, strength_offset);

	if(strength != 1.0f) {
		strength = max(strength, 0.0f);
		N = normalize(sd->N + (N - sd->N)*strength);
	}

	stack_store_float3(stack, normal_offset, normalize(N));
}

__device void svm_node_tangent(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint tangent_offset, direction_type, axis;
	decode_node_uchar4(node.y, &tangent_offset, &direction_type, &axis, NULL);

	float3 tangent;

	if(direction_type == NODE_TANGENT_UVMAP) {
		/* UV map */
		int attr_offset = find_attribute(kg, sd, node.z);

		if(attr_offset == ATTR_STD_NOT_FOUND)
			tangent = make_float3(0.0f, 0.0f, 0.0f);
		else
			tangent = triangle_attribute_float3(kg, sd, ATTR_ELEMENT_CORNER, attr_offset, NULL, NULL);
	}
	else {
		/* radial */
		int attr_offset = find_attribute(kg, sd, node.z);
		float3 generated;

		if(attr_offset == ATTR_STD_NOT_FOUND)
			generated = sd->P;
		else
			generated = triangle_attribute_float3(kg, sd, ATTR_ELEMENT_VERTEX, attr_offset, NULL, NULL);

		if(axis == NODE_TANGENT_AXIS_X)
			tangent = make_float3(0.0f, -(generated.z - 0.5f), (generated.y - 0.5f));
		else if(axis == NODE_TANGENT_AXIS_Y)
			tangent = make_float3(-(generated.z - 0.5f), 0.0f, (generated.x - 0.5f));
		else
			tangent = make_float3(-(generated.y - 0.5f), (generated.x - 0.5f), 0.0f);
	}

	object_normal_transform(kg, sd, &tangent);
	tangent = cross(sd->N, normalize(cross(tangent, sd->N)));
	stack_store_float3(stack, tangent_offset, tangent);
}

CCL_NAMESPACE_END

