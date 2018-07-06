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

/* Mapping Node */

ccl_device void svm_node_mapping(KernelGlobals *kg, ShaderData *sd, float *stack, uint vec_offset, uint out_offset, int *offset)
{
	float3 v = stack_load_float3(stack, vec_offset);

	Transform tfm;
	tfm.x = read_node_float(kg, offset);
	tfm.y = read_node_float(kg, offset);
	tfm.z = read_node_float(kg, offset);

	float3 r = transform_point(&tfm, v);
	stack_store_float3(stack, out_offset, r);
}

ccl_device void svm_node_min_max(KernelGlobals *kg, ShaderData *sd, float *stack, uint vec_offset, uint out_offset, int *offset)
{
	float3 v = stack_load_float3(stack, vec_offset);

	float3 mn = float4_to_float3(read_node_float(kg, offset));
	float3 mx = float4_to_float3(read_node_float(kg, offset));

	float3 r = min(max(mn, v), mx);
	stack_store_float3(stack, out_offset, r);
}

CCL_NAMESPACE_END
