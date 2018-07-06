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

ccl_device void svm_node_normal(KernelGlobals *kg, ShaderData *sd, float *stack, uint in_normal_offset, uint out_normal_offset, uint out_dot_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);
	float3 normal = stack_load_float3(stack, in_normal_offset);

	float3 direction;
	direction.x = __int_as_float(node1.x);
	direction.y = __int_as_float(node1.y);
	direction.z = __int_as_float(node1.z);
	direction = normalize(direction);

	if(stack_valid(out_normal_offset))
		stack_store_float3(stack, out_normal_offset, direction);

	if(stack_valid(out_dot_offset))
		stack_store_float(stack, out_dot_offset, dot(direction, normalize(normal)));
}

CCL_NAMESPACE_END
