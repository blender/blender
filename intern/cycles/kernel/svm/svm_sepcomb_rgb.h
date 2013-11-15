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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

ccl_device void svm_node_combine_rgb(ShaderData *sd, float *stack, uint in_offset, uint color_index, uint out_offset)
{
	float color = stack_load_float(stack, in_offset);

	if (stack_valid(out_offset))
		stack_store_float(stack, out_offset+color_index, color);
}

ccl_device void svm_node_separate_rgb(ShaderData *sd, float *stack, uint icolor_offset, uint color_index, uint out_offset)
{
	float3 color = stack_load_float3(stack, icolor_offset);

	if (stack_valid(out_offset)) {
		if (color_index == 0)
			stack_store_float(stack, out_offset, color.x);
		else if (color_index == 1)
			stack_store_float(stack, out_offset, color.y);
		else
			stack_store_float(stack, out_offset, color.z);
	}
}

CCL_NAMESPACE_END

