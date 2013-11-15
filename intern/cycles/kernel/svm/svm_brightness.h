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

ccl_device void svm_node_brightness(ShaderData *sd, float *stack, uint in_color, uint out_color, uint node)
{
	uint bright_offset, contrast_offset;
	float3 color = stack_load_float3(stack, in_color);

	decode_node_uchar4(node, &bright_offset, &contrast_offset, NULL, NULL);
	float brightness = stack_load_float(stack, bright_offset);
	float contrast  = stack_load_float(stack, contrast_offset);

	float a = 1.0f + contrast;
	float b = brightness - contrast*0.5f;

	color.x = max(a*color.x + b, 0.0f);
	color.y = max(a*color.y + b, 0.0f);
	color.z = max(a*color.z + b, 0.0f);

	if (stack_valid(out_color))
		stack_store_float3(stack, out_color, color);
}

CCL_NAMESPACE_END
