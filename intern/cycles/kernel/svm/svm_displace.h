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

/* Bump Node */

ccl_device void svm_node_set_bump(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
#ifdef __RAY_DIFFERENTIALS__
	/* get normal input */
	uint normal_offset, scale_offset, invert, use_object_space;
	decode_node_uchar4(node.y, &normal_offset, &scale_offset, &invert, &use_object_space);

	float3 normal_in = stack_valid(normal_offset)? stack_load_float3(stack, normal_offset): sd->N;

	float3 dPdx = sd->dP.dx;
	float3 dPdy = sd->dP.dy;

	if(use_object_space) {
		object_inverse_normal_transform(kg, sd, &normal_in);
		object_inverse_dir_transform(kg, sd, &dPdx);
		object_inverse_dir_transform(kg, sd, &dPdy);
	}

	/* get surface tangents from normal */
	float3 Rx = cross(dPdy, normal_in);
	float3 Ry = cross(normal_in, dPdx);

	/* get bump values */
	uint c_offset, x_offset, y_offset, strength_offset;
	decode_node_uchar4(node.z, &c_offset, &x_offset, &y_offset, &strength_offset);

	float h_c = stack_load_float(stack, c_offset);
	float h_x = stack_load_float(stack, x_offset);
	float h_y = stack_load_float(stack, y_offset);

	/* compute surface gradient and determinant */
	float det = dot(dPdx, Rx);
	float3 surfgrad = (h_x - h_c)*Rx + (h_y - h_c)*Ry;

	float absdet = fabsf(det);

	float strength = stack_load_float(stack, strength_offset);
	float scale = stack_load_float(stack, scale_offset);

	if(invert)
		scale *= -1.0f;

	strength = max(strength, 0.0f);

	/* compute and output perturbed normal */
	float3 normal_out = safe_normalize(absdet*normal_in - scale*signf(det)*surfgrad);
	if(is_zero(normal_out)) {
		normal_out = normal_in;
	}
	else {
		normal_out = normalize(strength*normal_out + (1.0f - strength)*normal_in);
	}

	if(use_object_space) {
		object_normal_transform(kg, sd, &normal_out);
	}

	normal_out = ensure_valid_reflection(sd->Ng, sd->I, normal_out);

	stack_store_float3(stack, node.w, normal_out);
#endif
}

/* Displacement Node */

ccl_device void svm_node_set_displacement(KernelGlobals *kg, ShaderData *sd, float *stack, uint fac_offset)
{
	float3 dP = stack_load_float3(stack, fac_offset);
	sd->P += dP;
}

ccl_device void svm_node_displacement(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint height_offset, midlevel_offset, scale_offset, normal_offset;
	decode_node_uchar4(node.y, &height_offset, &midlevel_offset, &scale_offset, &normal_offset);

	float height = stack_load_float(stack, height_offset);
	float midlevel = stack_load_float(stack, midlevel_offset);
	float scale = stack_load_float(stack, scale_offset);
	float3 normal = stack_valid(normal_offset)? stack_load_float3(stack, normal_offset): sd->N;
	uint space = node.w;

	float3 dP = normal;

	if(space == NODE_NORMAL_MAP_OBJECT) {
		/* Object space. */
		object_inverse_normal_transform(kg, sd, &dP);
		dP *= (height - midlevel) * scale;
		object_dir_transform(kg, sd, &dP);
	}
	else {
		/* World space. */
		dP *= (height - midlevel) * scale;
	}

	stack_store_float3(stack, node.z, dP);
}

ccl_device void svm_node_vector_displacement(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 data_node = read_node(kg, offset);
	uint space = data_node.x;

	uint vector_offset, midlevel_offset,scale_offset, displacement_offset;
	decode_node_uchar4(node.y, &vector_offset, &midlevel_offset, &scale_offset, &displacement_offset);

	float3 vector = stack_load_float3(stack, vector_offset);
	float midlevel = stack_load_float(stack, midlevel_offset);
	float scale = stack_load_float(stack, scale_offset);
	float3 dP = (vector - make_float3(midlevel, midlevel, midlevel)) * scale;

	if(space == NODE_NORMAL_MAP_TANGENT) {
		/* Tangent space. */
		float3 normal = sd->N;
		object_inverse_normal_transform(kg, sd, &normal);

		const AttributeDescriptor attr = find_attribute(kg, sd, node.z);
		float3 tangent;
		if(attr.offset != ATTR_STD_NOT_FOUND) {
			tangent = primitive_attribute_float3(kg, sd, attr, NULL, NULL);
		}
		else {
			tangent = normalize(sd->dPdu);
		}

		float3 bitangent = normalize(cross(normal, tangent));
		const AttributeDescriptor attr_sign = find_attribute(kg, sd, node.w);
		if(attr_sign.offset != ATTR_STD_NOT_FOUND) {
			float sign = primitive_attribute_float(kg, sd, attr_sign, NULL, NULL);
			bitangent *= sign;
		}

		dP = tangent*dP.x + normal*dP.y + bitangent*dP.z;
	}

	if(space != NODE_NORMAL_MAP_WORLD) {
		/* Tangent or object space. */
		object_dir_transform(kg, sd, &dP);
	}

	stack_store_float3(stack, displacement_offset, dP);
}

CCL_NAMESPACE_END
