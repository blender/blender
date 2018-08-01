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

CCL_NAMESPACE_BEGIN

/* Texture Coordinate Node */

ccl_device void svm_node_tex_coord(KernelGlobals *kg,
                                   ShaderData *sd,
                                   int path_flag,
                                   float *stack,
                                   uint4 node,
                                   int *offset)
{
	float3 data;
	uint type = node.y;
	uint out_offset = node.z;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			data = sd->P;
			if(node.w == 0) {
				if(sd->object != OBJECT_NONE) {
					object_inverse_position_transform(kg, sd, &data);
				}
			}
			else {
				Transform tfm;
				tfm.x = read_node_float(kg, offset);
				tfm.y = read_node_float(kg, offset);
				tfm.z = read_node_float(kg, offset);
				data = transform_point(&tfm, data);
			}
			break;
		}
		case NODE_TEXCO_NORMAL: {
			data = sd->N;
			object_inverse_normal_transform(kg, sd, &data);
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != OBJECT_NONE)
				data = transform_point(&tfm, sd->P);
			else
				data = transform_point(&tfm, sd->P + camera_position(kg));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			if((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE && kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
				data = camera_world_to_ndc(kg, sd, sd->ray_P);
			else
				data = camera_world_to_ndc(kg, sd, sd->P);
			data.z = 0.0f;
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != OBJECT_NONE)
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
		case NODE_TEXCO_VOLUME_GENERATED: {
			data = sd->P;

#ifdef __VOLUME__
			if(sd->object != OBJECT_NONE)
				data = volume_normalized_position(kg, sd, data);
#endif
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
}

ccl_device void svm_node_tex_coord_bump_dx(KernelGlobals *kg,
                                           ShaderData *sd,
                                           int path_flag,
                                           float *stack,
                                           uint4 node,
                                           int *offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;
	uint type = node.y;
	uint out_offset = node.z;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			data = sd->P + sd->dP.dx;
			if(node.w == 0) {
				if(sd->object != OBJECT_NONE) {
					object_inverse_position_transform(kg, sd, &data);
				}
			}
			else {
				Transform tfm;
				tfm.x = read_node_float(kg, offset);
				tfm.y = read_node_float(kg, offset);
				tfm.z = read_node_float(kg, offset);
				data = transform_point(&tfm, data);
			}
			break;
		}
		case NODE_TEXCO_NORMAL: {
			data = sd->N;
			object_inverse_normal_transform(kg, sd, &data);
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != OBJECT_NONE)
				data = transform_point(&tfm, sd->P + sd->dP.dx);
			else
				data = transform_point(&tfm, sd->P + sd->dP.dx + camera_position(kg));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			if((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE && kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
				data = camera_world_to_ndc(kg, sd, sd->ray_P + sd->ray_dP.dx);
			else
				data = camera_world_to_ndc(kg, sd, sd->P + sd->dP.dx);
			data.z = 0.0f;
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != OBJECT_NONE)
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
		case NODE_TEXCO_VOLUME_GENERATED: {
			data = sd->P + sd->dP.dx;

#ifdef __VOLUME__
			if(sd->object != OBJECT_NONE)
				data = volume_normalized_position(kg, sd, data);
#endif
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
#endif
}

ccl_device void svm_node_tex_coord_bump_dy(KernelGlobals *kg,
                                           ShaderData *sd,
                                           int path_flag,
                                           float *stack,
                                           uint4 node,
                                           int *offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;
	uint type = node.y;
	uint out_offset = node.z;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			data = sd->P + sd->dP.dy;
			if(node.w == 0) {
				if(sd->object != OBJECT_NONE) {
					object_inverse_position_transform(kg, sd, &data);
				}
			}
			else {
				Transform tfm;
				tfm.x = read_node_float(kg, offset);
				tfm.y = read_node_float(kg, offset);
				tfm.z = read_node_float(kg, offset);
				data = transform_point(&tfm, data);
			}
			break;
		}
		case NODE_TEXCO_NORMAL: {
			data = sd->N;
			object_inverse_normal_transform(kg, sd, &data);
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != OBJECT_NONE)
				data = transform_point(&tfm, sd->P + sd->dP.dy);
			else
				data = transform_point(&tfm, sd->P + sd->dP.dy + camera_position(kg));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			if((path_flag & PATH_RAY_CAMERA) && sd->object == OBJECT_NONE && kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
				data = camera_world_to_ndc(kg, sd, sd->ray_P + sd->ray_dP.dy);
			else
				data = camera_world_to_ndc(kg, sd, sd->P + sd->dP.dy);
			data.z = 0.0f;
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != OBJECT_NONE)
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
		case NODE_TEXCO_VOLUME_GENERATED: {
			data = sd->P + sd->dP.dy;

#ifdef __VOLUME__
			if(sd->object != OBJECT_NONE)
				data = volume_normalized_position(kg, sd, data);
#endif
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_tex_coord(kg, sd, path_flag, stack, node, offset);
#endif
}

ccl_device void svm_node_normal_map(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint color_offset, strength_offset, normal_offset, space;
	decode_node_uchar4(node.y, &color_offset, &strength_offset, &normal_offset, &space);

	float3 color = stack_load_float3(stack, color_offset);
	color = 2.0f*make_float3(color.x - 0.5f, color.y - 0.5f, color.z - 0.5f);

	bool is_backfacing = (sd->flag & SD_BACKFACING) != 0;
	float3 N;

	if(space == NODE_NORMAL_MAP_TANGENT) {
		/* tangent space */
		if(sd->object == OBJECT_NONE) {
			stack_store_float3(stack, normal_offset, make_float3(0.0f, 0.0f, 0.0f));
			return;
		}

		/* first try to get tangent attribute */
		const AttributeDescriptor attr = find_attribute(kg, sd, node.z);
		const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.w);
		const AttributeDescriptor attr_normal = find_attribute(kg, sd, ATTR_STD_VERTEX_NORMAL);

		if(attr.offset == ATTR_STD_NOT_FOUND || attr_sign.offset == ATTR_STD_NOT_FOUND || attr_normal.offset == ATTR_STD_NOT_FOUND) {
			stack_store_float3(stack, normal_offset, make_float3(0.0f, 0.0f, 0.0f));
			return;
		}

		/* get _unnormalized_ interpolated normal and tangent */
		float3 tangent = primitive_attribute_float3(kg, sd, attr, NULL, NULL);
		float sign = primitive_attribute_float(kg, sd, attr_sign, NULL, NULL);
		float3 normal;

		if(sd->shader & SHADER_SMOOTH_NORMAL) {
			normal = primitive_attribute_float3(kg, sd, attr_normal, NULL, NULL);
		}
		else {
			normal = sd->Ng;

			/* the normal is already inverted, which is too soon for the math here */
			if(is_backfacing) {
				normal = -normal;
			}

			object_inverse_normal_transform(kg, sd, &normal);
		}

		/* apply normal map */
		float3 B = sign * cross(normal, tangent);
		N = safe_normalize(color.x * tangent + color.y * B + color.z * normal);

		/* transform to world space */
		object_normal_transform(kg, sd, &N);
	}
	else {
		/* strange blender convention */
		if(space == NODE_NORMAL_MAP_BLENDER_OBJECT || space == NODE_NORMAL_MAP_BLENDER_WORLD) {
			color.y = -color.y;
			color.z = -color.z;
		}

		/* object, world space */
		N = color;

		if(space == NODE_NORMAL_MAP_OBJECT || space == NODE_NORMAL_MAP_BLENDER_OBJECT)
			object_normal_transform(kg, sd, &N);
		else
			N = safe_normalize(N);
	}

	/* invert normal for backfacing polygons */
	if(is_backfacing) {
		N = -N;
	}

	float strength = stack_load_float(stack, strength_offset);

	if(strength != 1.0f) {
		strength = max(strength, 0.0f);
		N = safe_normalize(sd->N + (N - sd->N)*strength);
	}

	N = ensure_valid_reflection(sd->Ng, sd->I, N);

	if(is_zero(N)) {
		N = sd->N;
	}

	stack_store_float3(stack, normal_offset, N);
}

ccl_device void svm_node_tangent(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint tangent_offset, direction_type, axis;
	decode_node_uchar4(node.y, &tangent_offset, &direction_type, &axis, NULL);

	float3 tangent;

	if(direction_type == NODE_TANGENT_UVMAP) {
		/* UV map */
		const AttributeDescriptor desc = find_attribute(kg, sd, node.z);

		if(desc.offset == ATTR_STD_NOT_FOUND)
			tangent = make_float3(0.0f, 0.0f, 0.0f);
		else
			tangent = primitive_attribute_float3(kg, sd, desc, NULL, NULL);
	}
	else {
		/* radial */
		const AttributeDescriptor desc = find_attribute(kg, sd, node.z);
		float3 generated;

		if(desc.offset == ATTR_STD_NOT_FOUND)
			generated = sd->P;
		else
			generated = primitive_attribute_float3(kg, sd, desc, NULL, NULL);

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
