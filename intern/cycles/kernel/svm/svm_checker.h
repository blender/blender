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

/* Checker */

ccl_device_noinline float svm_checker(float3 p)
{
	/* avoid precision issues on unit coordinates */
	p.x = (p.x + 0.00001f)*0.9999f;
	p.y = (p.y + 0.00001f)*0.9999f;
	p.z = (p.z + 0.00001f)*0.9999f;

	int xi = float_to_int(fabsf(floorf(p.x)));
	int yi = float_to_int(fabsf(floorf(p.y)));
	int zi = float_to_int(fabsf(floorf(p.z)));

	return ((xi % 2 == yi % 2) == (zi % 2))? 1.0f: 0.0f;
}

ccl_device void svm_node_tex_checker(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{	
	uint co_offset, color1_offset, color2_offset, scale_offset;
	uint color_offset, fac_offset;

	decode_node_uchar4(node.y, &co_offset, &color1_offset, &color2_offset, &scale_offset);
	decode_node_uchar4(node.z, &color_offset, &fac_offset, NULL, NULL);

	float3 co = stack_load_float3(stack, co_offset);
	float3 color1 = stack_load_float3(stack, color1_offset);
	float3 color2 = stack_load_float3(stack, color2_offset);
	float scale = stack_load_float_default(stack, scale_offset, node.w);
	
	float f = svm_checker(co*scale);

	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, (f == 1.0f)? color1: color2);
	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

