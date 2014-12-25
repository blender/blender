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

ccl_device float invert(float color, float factor)
{
	return factor*(1.0f - color) + (1.0f - factor) * color;
}

ccl_device void svm_node_invert(ShaderData *sd, float *stack, uint in_fac, uint in_color, uint out_color)
{
	float factor = stack_load_float(stack, in_fac);
	float3 color = stack_load_float3(stack, in_color);

	color.x = invert(color.x, factor);
	color.y = invert(color.y, factor);
	color.z = invert(color.z, factor);

	if (stack_valid(out_color))
		stack_store_float3(stack, out_color, color);
}

CCL_NAMESPACE_END

