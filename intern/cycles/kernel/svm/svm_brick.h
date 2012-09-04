/*
 * Copyright 2012, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

/* Brick */

__device_noinline float brick_noise(int n) /* fast integer noise */
{
	int nn;
	n = (n >> 13) ^ n;
	nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 0.5f * ((float)nn / 1073741824.0f);
}

__device_noinline float svm_brick(float3 p, float scale, float mortar_size, float bias,
	float brick_width, float row_height, float offset_amount, int offset_frequency,
	float squash_amount, int squash_frequency, float *tint)
{	
	p *= scale;

	int bricknum, rownum;
	float offset = 0.0f;
	float x, y;

	rownum = (int)floor(p.y / row_height);
	
	if(offset_frequency && squash_frequency) {
		brick_width *= ((int)(rownum) % squash_frequency ) ? 1.0f : squash_amount; /* squash */
		offset = ((int)(rownum) % offset_frequency ) ? 0 : (brick_width*offset_amount); /* offset */
	}

	bricknum = (int)floor((p.x+offset) / brick_width);

	x = (p.x+offset) - brick_width*bricknum;
	y = p.y - row_height*rownum;

	*tint = clamp((brick_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias), 0.0f, 1.0f);

	return (x < mortar_size || y < mortar_size ||
		x > (brick_width - mortar_size) ||
		y > (row_height - mortar_size)) ? 1.0f : 0.0f;
}

__device void svm_node_tex_brick(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{	
	uint4 node2 = read_node(kg, offset);
	uint4 node3 = read_node(kg, offset);
	
	/* Input and Output Sockets */
	uint co_offset, color1_offset, color2_offset, mortar_offset, scale_offset;
	uint mortar_size_offset, bias_offset, brick_width_offset, row_height_offset;
	uint color_offset, fac_offset;
	
	/* RNA properties */
	uint offset_frequency, squash_frequency;
	
	float tint = 0.0f;

	decode_node_uchar4(node.y, &co_offset, &color1_offset, &color2_offset, &mortar_offset);
	decode_node_uchar4(node.z, &scale_offset, &mortar_size_offset, &bias_offset, &brick_width_offset);
	decode_node_uchar4(node.w, &row_height_offset, &color_offset, &fac_offset, NULL);
	
	decode_node_uchar4(node2.x, &offset_frequency, &squash_frequency, NULL, NULL);

	float3 co = stack_load_float3(stack, co_offset);
	
	float3 color1 = stack_load_float3(stack, color1_offset);
	float3 color2 = stack_load_float3(stack, color2_offset);
	float3 mortar = stack_load_float3(stack, mortar_offset);
	
	float scale = stack_load_float_default(stack, scale_offset, node2.y);
	float mortar_size = stack_load_float_default(stack, mortar_size_offset, node2.z);
	float bias = stack_load_float_default(stack, bias_offset, node2.w);
	float brick_width = stack_load_float_default(stack, brick_width_offset, node3.x);
	float row_height = stack_load_float_default(stack, row_height_offset, node3.y);
	float offset_amount = __int_as_float(node3.z);
	float squash_amount = __int_as_float(node3.w);
	
	float f = svm_brick(co, scale, mortar_size, bias, brick_width, row_height,
		offset_amount, offset_frequency, squash_amount, squash_frequency,
		&tint);
	
	if(f != 1.0f) {
		float facm = 1.0f - tint;

		color1.x = facm * (color1.x) + tint * color2.x;
		color1.y = facm * (color1.y) + tint * color2.y;
		color1.z = facm * (color1.z) + tint * color2.z;
	}

	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, (f == 1.0f)? mortar: color1);
	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
}

CCL_NAMESPACE_END

