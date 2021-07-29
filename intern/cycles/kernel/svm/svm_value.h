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

/* Value Nodes */

ccl_device void svm_node_value_f(KernelGlobals *kg, ShaderData *sd, float *stack, uint ivalue, uint out_offset)
{
	stack_store_float(stack, out_offset, __uint_as_float(ivalue));
}

ccl_device void svm_node_value_v(KernelGlobals *kg, ShaderData *sd, float *stack, uint out_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);
	float3 p = make_float3(__uint_as_float(node1.y), __uint_as_float(node1.z), __uint_as_float(node1.w));

	stack_store_float3(stack, out_offset, p);
}

CCL_NAMESPACE_END

