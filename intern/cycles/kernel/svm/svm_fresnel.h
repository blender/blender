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

/* Fresnel Node */

ccl_device void svm_node_fresnel(ShaderData *sd, float *stack, uint ior_offset, uint ior_value, uint node)
{
	uint normal_offset, out_offset;
	decode_node_uchar4(node, &normal_offset, &out_offset, NULL, NULL);
	float eta = (stack_valid(ior_offset))? stack_load_float(stack, ior_offset): __uint_as_float(ior_value);
	float3 normal_in = stack_valid(normal_offset)? stack_load_float3(stack, normal_offset): sd->N;

	eta = fmaxf(eta, 1e-5f);
	eta = (sd->flag & SD_BACKFACING)? 1.0f/eta: eta;

	float f = fresnel_dielectric_cos(dot(sd->I, normal_in), eta);

	stack_store_float(stack, out_offset, f);
}

/* Layer Weight Node */

ccl_device void svm_node_layer_weight(ShaderData *sd, float *stack, uint4 node)
{
	uint blend_offset = node.y;
	uint blend_value = node.z;

	uint type, normal_offset, out_offset;
	decode_node_uchar4(node.w, &type, &normal_offset, &out_offset, NULL);

	float blend = (stack_valid(blend_offset))? stack_load_float(stack, blend_offset): __uint_as_float(blend_value);
	float3 normal_in = (stack_valid(normal_offset))? stack_load_float3(stack, normal_offset): sd->N;

	float f;

	if(type == NODE_LAYER_WEIGHT_FRESNEL) {
		float eta = fmaxf(1.0f - blend, 1e-5f);
		eta = (sd->flag & SD_BACKFACING)? eta: 1.0f/eta;

		f = fresnel_dielectric_cos(dot(sd->I, normal_in), eta);
	}
	else {
		f = fabsf(dot(sd->I, normal_in));

		if(blend != 0.5f) {
			blend = clamp(blend, 0.0f, 1.0f-1e-5f);
			blend = (blend < 0.5f)? 2.0f*blend: 0.5f/(1.0f - blend);

			f = powf(f, blend);
		}

		f = 1.0f - f;
	}

	stack_store_float(stack, out_offset, f);
}

CCL_NAMESPACE_END
