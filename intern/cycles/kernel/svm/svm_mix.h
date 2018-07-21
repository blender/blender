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

/* Node */

ccl_device void svm_node_mix(KernelGlobals *kg, ShaderData *sd, float *stack, uint fac_offset, uint c1_offset, uint c2_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);

	float fac = stack_load_float(stack, fac_offset);
	float3 c1 = stack_load_float3(stack, c1_offset);
	float3 c2 = stack_load_float3(stack, c2_offset);
	float3 result = svm_mix((NodeMix)node1.y, fac, c1, c2);

	stack_store_float3(stack, node1.z, result);
}

CCL_NAMESPACE_END
