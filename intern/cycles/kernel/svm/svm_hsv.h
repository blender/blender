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

#ifndef __SVM_HSV_H__
#define __SVM_HSV_H__

CCL_NAMESPACE_BEGIN

ccl_device void svm_node_hsv(KernelGlobals *kg, ShaderData *sd, float *stack, uint in_color_offset, uint fac_offset, uint out_color_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);

	float fac = stack_load_float(stack, fac_offset);
	float3 in_color = stack_load_float3(stack, in_color_offset);
	float3 color = in_color;

	float hue = stack_load_float(stack, node1.y);
	float sat = stack_load_float(stack, node1.z);
	float val = stack_load_float(stack, node1.w);

	color = rgb_to_hsv(color);

	/* remember: fmod doesn't work for negative numbers here */
	color.x += hue + 0.5f;
	color.x = fmodf(color.x, 1.0f);
	color.y *= sat;
	color.z *= val;

	color = hsv_to_rgb(color);

	color.x = fac*color.x + (1.0f - fac)*in_color.x;
	color.y = fac*color.y + (1.0f - fac)*in_color.y;
	color.z = fac*color.z + (1.0f - fac)*in_color.z;

	/* Clamp color to prevent negative values cauzed by oversaturation. */
	color.x = max(color.x, 0.0f);
	color.y = max(color.y, 0.0f);
	color.z = max(color.z, 0.0f);

	if (stack_valid(out_color_offset))
		stack_store_float3(stack, out_color_offset, color);
}

CCL_NAMESPACE_END

#endif /* __SVM_HSV_H__ */

