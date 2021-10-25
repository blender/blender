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
	uint normal_offset, distance_offset, invert, use_object_space;
	decode_node_uchar4(node.y, &normal_offset, &distance_offset, &invert, &use_object_space);

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
	float distance = stack_load_float(stack, distance_offset);

	if(invert)
		distance *= -1.0f;

	strength = max(strength, 0.0f);

	/* compute and output perturbed normal */
	float3 normal_out = safe_normalize(absdet*normal_in - distance*signf(det)*surfgrad);
	if(is_zero(normal_out)) {
		normal_out = normal_in;
	}
	else {
		normal_out = normalize(strength*normal_out + (1.0f - strength)*normal_in);
	}

	if(use_object_space) {
		object_normal_transform(kg, sd, &normal_out);
	}

	stack_store_float3(stack, node.w, normal_out);
#endif
}

/* Displacement Node */

ccl_device void svm_node_set_displacement(KernelGlobals *kg, ShaderData *sd, float *stack, uint fac_offset)
{
	float d = stack_load_float(stack, fac_offset);

	float3 dP = sd->N;
	object_inverse_normal_transform(kg, sd, &dP);

	dP *= d*0.1f; /* todo: get rid of this factor */

	object_dir_transform(kg, sd, &dP);

	sd->P += dP;
}

CCL_NAMESPACE_END

