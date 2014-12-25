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

#ifndef __SVM_RAMP_H__
#define __SVM_RAMP_H__

CCL_NAMESPACE_BEGIN

ccl_device float4 rgb_ramp_lookup(KernelGlobals *kg, int offset, float f, bool interpolate)
{
	f = clamp(f, 0.0f, 1.0f)*(RAMP_TABLE_SIZE-1);

	/* clamp int as well in case of NaN */
	int i = clamp(float_to_int(f), 0, RAMP_TABLE_SIZE-1);
	float t = f - (float)i;

	float4 a = fetch_node_float(kg, offset+i);

	if(interpolate && t > 0.0f)
		a = (1.0f - t)*a + t*fetch_node_float(kg, offset+i+1);

	return a;
}

ccl_device void svm_node_rgb_ramp(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint fac_offset, color_offset, alpha_offset;
	uint interpolate = node.z;

	decode_node_uchar4(node.y, &fac_offset, &color_offset, &alpha_offset, NULL);

	float fac = stack_load_float(stack, fac_offset);
	float4 color = rgb_ramp_lookup(kg, *offset, fac, interpolate);

	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, float4_to_float3(color));
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, color.w);

	*offset += RAMP_TABLE_SIZE;
}

ccl_device void svm_node_rgb_curves(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint fac_offset = node.y;
	uint color_offset = node.z;
	uint out_offset = node.w;

	float fac = stack_load_float(stack, fac_offset);
	float3 color = stack_load_float3(stack, color_offset);

	float r = rgb_ramp_lookup(kg, *offset, color.x, true).x;
	float g = rgb_ramp_lookup(kg, *offset, color.y, true).y;
	float b = rgb_ramp_lookup(kg, *offset, color.z, true).z;

	color = (1.0f - fac)*color + fac*make_float3(r, g, b);
	stack_store_float3(stack, out_offset, color);

	*offset += RAMP_TABLE_SIZE;
}

ccl_device void svm_node_vector_curves(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint fac_offset = node.y;
	uint color_offset = node.z;
	uint out_offset = node.w;

	float fac = stack_load_float(stack, fac_offset);
	float3 color = stack_load_float3(stack, color_offset);

	float r = rgb_ramp_lookup(kg, *offset, (color.x + 1.0f)*0.5f, true).x;
	float g = rgb_ramp_lookup(kg, *offset, (color.y + 1.0f)*0.5f, true).y;
	float b = rgb_ramp_lookup(kg, *offset, (color.z + 1.0f)*0.5f, true).z;

	color = (1.0f - fac)*color + fac*make_float3(r*2.0f - 1.0f, g*2.0f - 1.0f, b*2.0f - 1.0f);
	stack_store_float3(stack, out_offset, color);

	*offset += RAMP_TABLE_SIZE;
}

CCL_NAMESPACE_END

#endif /* __SVM_RAMP_H__ */

