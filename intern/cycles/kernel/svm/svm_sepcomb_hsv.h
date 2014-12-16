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

ccl_device void svm_node_combine_hsv(KernelGlobals *kg, ShaderData *sd, float *stack, uint hue_in, uint saturation_in, uint value_in, int *offset)
{
	uint4 node1 = read_node(kg, offset);
	uint color_out = node1.y;
	
	float hue = stack_load_float(stack, hue_in);
	float saturation = stack_load_float(stack, saturation_in);
	float value = stack_load_float(stack, value_in);
	
	/* Combine, and convert back to RGB */
	float3 color = color_srgb_to_scene_linear(
	        hsv_to_rgb(make_float3(hue, saturation, value)));

	if (stack_valid(color_out))
		stack_store_float3(stack, color_out, color);
}

ccl_device void svm_node_separate_hsv(KernelGlobals *kg, ShaderData *sd, float *stack, uint color_in, uint hue_out, uint saturation_out, int *offset)
{
	uint4 node1 = read_node(kg, offset);
	uint value_out = node1.y;
	
	float3 color = stack_load_float3(stack, color_in);
	
	/* Convert to HSV */
	color = rgb_to_hsv(color_scene_linear_to_srgb(color));

	if (stack_valid(hue_out))
		stack_store_float(stack, hue_out, color.x);
	if (stack_valid(saturation_out))
		stack_store_float(stack, saturation_out, color.y);
	if (stack_valid(value_out))
		stack_store_float(stack, value_out, color.z);
}

CCL_NAMESPACE_END

