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

/* Brick */

ccl_device_noinline float brick_noise(uint n) /* fast integer noise */
{
	uint nn;
	n = (n + 1013) & 0x7fffffff;
	n = (n >> 13) ^ n;
	nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 0.5f * ((float)nn / 1073741824.0f);
}

ccl_device_noinline float2 svm_brick(float3 p, float mortar_size, float mortar_smooth, float bias,
	float brick_width, float row_height, float offset_amount, int offset_frequency,
	float squash_amount, int squash_frequency)
{
	int bricknum, rownum;
	float offset = 0.0f;
	float x, y;

	rownum = floor_to_int(p.y / row_height);

	if(offset_frequency && squash_frequency) {
		brick_width *= (rownum % squash_frequency) ? 1.0f : squash_amount; /* squash */
		offset = (rownum % offset_frequency) ? 0.0f : (brick_width*offset_amount); /* offset */
	}

	bricknum = floor_to_int((p.x+offset) / brick_width);

	x = (p.x+offset) - brick_width*bricknum;
	y = p.y - row_height*rownum;

	float tint = saturate((brick_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias));
	float min_dist = min(min(x, y), min(brick_width - x, row_height - y));

	float mortar;
	if(min_dist >= mortar_size) {
		mortar = 0.0f;
	}
	else if(mortar_smooth == 0.0f) {
		mortar = 1.0f;
	}
	else {
		min_dist = 1.0f - min_dist/mortar_size;
		mortar = (min_dist < mortar_smooth)? smoothstepf(min_dist / mortar_smooth) : 1.0f;
	}

	return make_float2(tint, mortar);
}

ccl_device void svm_node_tex_brick(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);
	uint4 node3 = read_node(kg, offset);
	uint4 node4 = read_node(kg, offset);

	/* Input and Output Sockets */
	uint co_offset, color1_offset, color2_offset, mortar_offset, scale_offset;
	uint mortar_size_offset, bias_offset, brick_width_offset, row_height_offset;
	uint color_offset, fac_offset, mortar_smooth_offset;

	/* RNA properties */
	uint offset_frequency, squash_frequency;

	decode_node_uchar4(node.y, &co_offset, &color1_offset, &color2_offset, &mortar_offset);
	decode_node_uchar4(node.z, &scale_offset, &mortar_size_offset, &bias_offset, &brick_width_offset);
	decode_node_uchar4(node.w, &row_height_offset, &color_offset, &fac_offset, &mortar_smooth_offset);

	decode_node_uchar4(node2.x, &offset_frequency, &squash_frequency, NULL, NULL);

	float3 co = stack_load_float3(stack, co_offset);

	float3 color1 = stack_load_float3(stack, color1_offset);
	float3 color2 = stack_load_float3(stack, color2_offset);
	float3 mortar = stack_load_float3(stack, mortar_offset);

	float scale = stack_load_float_default(stack, scale_offset, node2.y);
	float mortar_size = stack_load_float_default(stack, mortar_size_offset, node2.z);
	float mortar_smooth = stack_load_float_default(stack, mortar_smooth_offset, node4.x);
	float bias = stack_load_float_default(stack, bias_offset, node2.w);
	float brick_width = stack_load_float_default(stack, brick_width_offset, node3.x);
	float row_height = stack_load_float_default(stack, row_height_offset, node3.y);
	float offset_amount = __int_as_float(node3.z);
	float squash_amount = __int_as_float(node3.w);

	float2 f2 = svm_brick(co*scale, mortar_size, mortar_smooth, bias, brick_width, row_height,
		offset_amount, offset_frequency, squash_amount, squash_frequency);

	float tint = f2.x;
	float f = f2.y;

	if(f != 1.0f) {
		float facm = 1.0f - tint;
		color1 = facm * color1 + tint * color2;
	}

	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, color1*(1.0f-f) + mortar*f);
	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END
