/*
 * Copyright 2011, Blender Foundation.
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

__device float4 svm_image_texture(KernelGlobals *kg, int id, float x, float y)
{
	float4 r;

	/* not particularly proud of this massive switch, what are the
	   alternatives?
	   - use a single big 1D texture, and do our own lookup/filtering
	   - group by size and use a 3d texture, performance impact
	   - group into larger texture with some padding for correct lerp

	   also note that cuda has 128 textures limit, we use 100 now, since
	   we still need some for other storage */

#ifdef __KERNEL_OPENCL__
	r = make_float4(0.0f, 0.0f, 0.0f, 0.0f); /* todo */
#else
	switch(id) {
		case 0: r = kernel_tex_image_interp(__tex_image_000, x, y); break;
		case 1: r = kernel_tex_image_interp(__tex_image_001, x, y); break;
		case 2: r = kernel_tex_image_interp(__tex_image_002, x, y); break;
		case 3: r = kernel_tex_image_interp(__tex_image_003, x, y); break;
		case 4: r = kernel_tex_image_interp(__tex_image_004, x, y); break;
		case 5: r = kernel_tex_image_interp(__tex_image_005, x, y); break;
		case 6: r = kernel_tex_image_interp(__tex_image_006, x, y); break;
		case 7: r = kernel_tex_image_interp(__tex_image_007, x, y); break;
		case 8: r = kernel_tex_image_interp(__tex_image_008, x, y); break;
		case 9: r = kernel_tex_image_interp(__tex_image_009, x, y); break;
		case 10: r = kernel_tex_image_interp(__tex_image_010, x, y); break;
		case 11: r = kernel_tex_image_interp(__tex_image_011, x, y); break;
		case 12: r = kernel_tex_image_interp(__tex_image_012, x, y); break;
		case 13: r = kernel_tex_image_interp(__tex_image_013, x, y); break;
		case 14: r = kernel_tex_image_interp(__tex_image_014, x, y); break;
		case 15: r = kernel_tex_image_interp(__tex_image_015, x, y); break;
		case 16: r = kernel_tex_image_interp(__tex_image_016, x, y); break;
		case 17: r = kernel_tex_image_interp(__tex_image_017, x, y); break;
		case 18: r = kernel_tex_image_interp(__tex_image_018, x, y); break;
		case 19: r = kernel_tex_image_interp(__tex_image_019, x, y); break;
		case 20: r = kernel_tex_image_interp(__tex_image_020, x, y); break;
		case 21: r = kernel_tex_image_interp(__tex_image_021, x, y); break;
		case 22: r = kernel_tex_image_interp(__tex_image_022, x, y); break;
		case 23: r = kernel_tex_image_interp(__tex_image_023, x, y); break;
		case 24: r = kernel_tex_image_interp(__tex_image_024, x, y); break;
		case 25: r = kernel_tex_image_interp(__tex_image_025, x, y); break;
		case 26: r = kernel_tex_image_interp(__tex_image_026, x, y); break;
		case 27: r = kernel_tex_image_interp(__tex_image_027, x, y); break;
		case 28: r = kernel_tex_image_interp(__tex_image_028, x, y); break;
		case 29: r = kernel_tex_image_interp(__tex_image_029, x, y); break;
		case 30: r = kernel_tex_image_interp(__tex_image_030, x, y); break;
		case 31: r = kernel_tex_image_interp(__tex_image_031, x, y); break;
		case 32: r = kernel_tex_image_interp(__tex_image_032, x, y); break;
		case 33: r = kernel_tex_image_interp(__tex_image_033, x, y); break;
		case 34: r = kernel_tex_image_interp(__tex_image_034, x, y); break;
		case 35: r = kernel_tex_image_interp(__tex_image_035, x, y); break;
		case 36: r = kernel_tex_image_interp(__tex_image_036, x, y); break;
		case 37: r = kernel_tex_image_interp(__tex_image_037, x, y); break;
		case 38: r = kernel_tex_image_interp(__tex_image_038, x, y); break;
		case 39: r = kernel_tex_image_interp(__tex_image_039, x, y); break;
		case 40: r = kernel_tex_image_interp(__tex_image_040, x, y); break;
		case 41: r = kernel_tex_image_interp(__tex_image_041, x, y); break;
		case 42: r = kernel_tex_image_interp(__tex_image_042, x, y); break;
		case 43: r = kernel_tex_image_interp(__tex_image_043, x, y); break;
		case 44: r = kernel_tex_image_interp(__tex_image_044, x, y); break;
		case 45: r = kernel_tex_image_interp(__tex_image_045, x, y); break;
		case 46: r = kernel_tex_image_interp(__tex_image_046, x, y); break;
		case 47: r = kernel_tex_image_interp(__tex_image_047, x, y); break;
		case 48: r = kernel_tex_image_interp(__tex_image_048, x, y); break;
		case 49: r = kernel_tex_image_interp(__tex_image_049, x, y); break;
		case 50: r = kernel_tex_image_interp(__tex_image_050, x, y); break;
		case 51: r = kernel_tex_image_interp(__tex_image_051, x, y); break;
		case 52: r = kernel_tex_image_interp(__tex_image_052, x, y); break;
		case 53: r = kernel_tex_image_interp(__tex_image_053, x, y); break;
		case 54: r = kernel_tex_image_interp(__tex_image_054, x, y); break;
		case 55: r = kernel_tex_image_interp(__tex_image_055, x, y); break;
		case 56: r = kernel_tex_image_interp(__tex_image_056, x, y); break;
		case 57: r = kernel_tex_image_interp(__tex_image_057, x, y); break;
		case 58: r = kernel_tex_image_interp(__tex_image_058, x, y); break;
		case 59: r = kernel_tex_image_interp(__tex_image_059, x, y); break;
		case 60: r = kernel_tex_image_interp(__tex_image_060, x, y); break;
		case 61: r = kernel_tex_image_interp(__tex_image_061, x, y); break;
		case 62: r = kernel_tex_image_interp(__tex_image_062, x, y); break;
		case 63: r = kernel_tex_image_interp(__tex_image_063, x, y); break;
		case 64: r = kernel_tex_image_interp(__tex_image_064, x, y); break;
		case 65: r = kernel_tex_image_interp(__tex_image_065, x, y); break;
		case 66: r = kernel_tex_image_interp(__tex_image_066, x, y); break;
		case 67: r = kernel_tex_image_interp(__tex_image_067, x, y); break;
		case 68: r = kernel_tex_image_interp(__tex_image_068, x, y); break;
		case 69: r = kernel_tex_image_interp(__tex_image_069, x, y); break;
		case 70: r = kernel_tex_image_interp(__tex_image_070, x, y); break;
		case 71: r = kernel_tex_image_interp(__tex_image_071, x, y); break;
		case 72: r = kernel_tex_image_interp(__tex_image_072, x, y); break;
		case 73: r = kernel_tex_image_interp(__tex_image_073, x, y); break;
		case 74: r = kernel_tex_image_interp(__tex_image_074, x, y); break;
		case 75: r = kernel_tex_image_interp(__tex_image_075, x, y); break;
		case 76: r = kernel_tex_image_interp(__tex_image_076, x, y); break;
		case 77: r = kernel_tex_image_interp(__tex_image_077, x, y); break;
		case 78: r = kernel_tex_image_interp(__tex_image_078, x, y); break;
		case 79: r = kernel_tex_image_interp(__tex_image_079, x, y); break;
		case 80: r = kernel_tex_image_interp(__tex_image_080, x, y); break;
		case 81: r = kernel_tex_image_interp(__tex_image_081, x, y); break;
		case 82: r = kernel_tex_image_interp(__tex_image_082, x, y); break;
		case 83: r = kernel_tex_image_interp(__tex_image_083, x, y); break;
		case 84: r = kernel_tex_image_interp(__tex_image_084, x, y); break;
		case 85: r = kernel_tex_image_interp(__tex_image_085, x, y); break;
		case 86: r = kernel_tex_image_interp(__tex_image_086, x, y); break;
		case 87: r = kernel_tex_image_interp(__tex_image_087, x, y); break;
		case 88: r = kernel_tex_image_interp(__tex_image_088, x, y); break;
		case 89: r = kernel_tex_image_interp(__tex_image_089, x, y); break;
		case 90: r = kernel_tex_image_interp(__tex_image_090, x, y); break;
		case 91: r = kernel_tex_image_interp(__tex_image_091, x, y); break;
		case 92: r = kernel_tex_image_interp(__tex_image_092, x, y); break;
		case 93: r = kernel_tex_image_interp(__tex_image_093, x, y); break;
		case 94: r = kernel_tex_image_interp(__tex_image_094, x, y); break;
		case 95: r = kernel_tex_image_interp(__tex_image_095, x, y); break;
		case 96: r = kernel_tex_image_interp(__tex_image_096, x, y); break;
		case 97: r = kernel_tex_image_interp(__tex_image_097, x, y); break;
		case 98: r = kernel_tex_image_interp(__tex_image_098, x, y); break;
		case 99: r = kernel_tex_image_interp(__tex_image_099, x, y); break;
		default: 
			kernel_assert(0);
			return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
#endif

	return r;
}

__device void svm_node_tex_image(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint id = node.y;
	uint co_offset, out_offset, alpha_offset, srgb;

	decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &srgb);

	float3 co = stack_load_float3(stack, co_offset);
	float4 f = svm_image_texture(kg, id, co.x, co.y);
	float3 r = make_float3(f.x, f.y, f.z);

	if(srgb) {
		r.x = color_srgb_to_scene_linear(r.x);
		r.y = color_srgb_to_scene_linear(r.y);
		r.z = color_srgb_to_scene_linear(r.z);
	}

	if(stack_valid(out_offset))
		stack_store_float3(stack, out_offset, r);
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, f.w);
}

__device void svm_node_tex_environment(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint id = node.y;
	uint co_offset, out_offset, alpha_offset, srgb;

	decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &srgb);

	float3 co = stack_load_float3(stack, co_offset);
	float2 uv = direction_to_equirectangular(co);
	float4 f = svm_image_texture(kg, id, uv.x, uv.y);
	float3 r = make_float3(f.x, f.y, f.z);

	if(srgb) {
		r.x = color_srgb_to_scene_linear(r.x);
		r.y = color_srgb_to_scene_linear(r.y);
		r.z = color_srgb_to_scene_linear(r.z);
	}

	if(stack_valid(out_offset))
		stack_store_float3(stack, out_offset, r);
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, f.w);
}

CCL_NAMESPACE_END

