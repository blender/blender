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

#ifdef __KERNEL_OPENCL__

/* For OpenCL all images are packed in a single array, and we do manual lookup
 * and interpolation. */

ccl_device_inline float4 svm_image_texture_read(KernelGlobals *kg, int offset)
{
	uchar4 r = kernel_tex_fetch(__tex_image_packed, offset);
	float f = 1.0f/255.0f;
	return make_float4(r.x*f, r.y*f, r.z*f, r.w*f);
}

ccl_device_inline int svm_image_texture_wrap_periodic(int x, int width)
{
	x %= width;
	if(x < 0)
		x += width;
	return x;
}

ccl_device_inline int svm_image_texture_wrap_clamp(int x, int width)
{
	return clamp(x, 0, width-1);
}

ccl_device_inline float svm_image_texture_frac(float x, int *ix)
{
	int i = float_to_int(x) - ((x < 0.0f)? 1: 0);
	*ix = i;
	return x - (float)i;
}

ccl_device float4 svm_image_texture(KernelGlobals *kg, int id, float x, float y, uint srgb, uint use_alpha)
{
	/* first slots are used by float textures, which are not supported here */
	if(id < TEX_NUM_FLOAT_IMAGES)
		return make_float4(1.0f, 0.0f, 1.0f, 1.0f);

	id -= TEX_NUM_FLOAT_IMAGES;

	uint4 info = kernel_tex_fetch(__tex_image_packed_info, id);
	uint width = info.x;
	uint height = info.y;
	uint offset = info.z;
	uint periodic = (info.w & 0x1);
	uint interpolation = info.w >> 1;

	float4 r;
	int ix, iy, nix, niy;
	if (interpolation == INTERPOLATION_CLOSEST) {
		svm_image_texture_frac(x*width, &ix);
		svm_image_texture_frac(y*height, &iy);

		if(periodic) {
			ix = svm_image_texture_wrap_periodic(ix, width);
			iy = svm_image_texture_wrap_periodic(iy, height);
		}
		else {
			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);

		}
		r = svm_image_texture_read(kg, offset + ix + iy*width);
	}
	else { /* We default to linear interpolation if it is not closest */
		float tx = svm_image_texture_frac(x*width, &ix);
		float ty = svm_image_texture_frac(y*height, &iy);

		if(periodic) {
			ix = svm_image_texture_wrap_periodic(ix, width);
			iy = svm_image_texture_wrap_periodic(iy, height);

			nix = svm_image_texture_wrap_periodic(ix+1, width);
			niy = svm_image_texture_wrap_periodic(iy+1, height);
		}
		else {
			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);

			nix = svm_image_texture_wrap_clamp(ix+1, width);
			niy = svm_image_texture_wrap_clamp(iy+1, height);
		}


		r = (1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, offset + ix + iy*width);
		r += (1.0f - ty)*tx*svm_image_texture_read(kg, offset + nix + iy*width);
		r += ty*(1.0f - tx)*svm_image_texture_read(kg, offset + ix + niy*width);
		r += ty*tx*svm_image_texture_read(kg, offset + nix + niy*width);
	}

	if(use_alpha && r.w != 1.0f && r.w != 0.0f) {
		float invw = 1.0f/r.w;
		r.x *= invw;
		r.y *= invw;
		r.z *= invw;

		if(id >= TEX_NUM_FLOAT_IMAGES) {
			r.x = min(r.x, 1.0f);
			r.y = min(r.y, 1.0f);
			r.z = min(r.z, 1.0f);
		}
	}

	if(srgb) {
		r.x = color_srgb_to_scene_linear(r.x);
		r.y = color_srgb_to_scene_linear(r.y);
		r.z = color_srgb_to_scene_linear(r.z);
	}

	return r;
}

#else

ccl_device float4 svm_image_texture(KernelGlobals *kg, int id, float x, float y, uint srgb, uint use_alpha)
{
#ifdef __KERNEL_CPU__
#ifdef __KERNEL_SSE2__
	__m128 r_m128;
	float4 &r = (float4 &)r_m128;
	r = kernel_tex_image_interp(id, x, y);
#else
	float4 r = kernel_tex_image_interp(id, x, y);
#endif
#else
	float4 r;

	/* not particularly proud of this massive switch, what are the
	 * alternatives?
	 * - use a single big 1D texture, and do our own lookup/filtering
	 * - group by size and use a 3d texture, performance impact
	 * - group into larger texture with some padding for correct lerp
	 *
	 * also note that cuda has 128 textures limit, we use 100 now, since
	 * we still need some for other storage */

	switch(id) {
		case 0: r = kernel_tex_image_interp(__tex_image_float_000, x, y); break;
		case 1: r = kernel_tex_image_interp(__tex_image_float_001, x, y); break;
		case 2: r = kernel_tex_image_interp(__tex_image_float_002, x, y); break;
		case 3: r = kernel_tex_image_interp(__tex_image_float_003, x, y); break;
		case 4: r = kernel_tex_image_interp(__tex_image_float_004, x, y); break;
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

#ifdef __KERNEL_SSE2__
	float alpha = r.w;

	if(use_alpha && alpha != 1.0f && alpha != 0.0f) {
		r_m128 = _mm_div_ps(r_m128, _mm_set1_ps(alpha));
		if(id >= TEX_NUM_FLOAT_IMAGES)
			r_m128 = _mm_min_ps(r_m128, _mm_set1_ps(1.0f));
		r.w = alpha;
	}

	if(srgb) {
		r_m128 = color_srgb_to_scene_linear(r_m128);
		r.w = alpha;
	}
#else
	if(use_alpha && r.w != 1.0f && r.w != 0.0f) {
		float invw = 1.0f/r.w;
		r.x *= invw;
		r.y *= invw;
		r.z *= invw;

		if(id >= TEX_NUM_FLOAT_IMAGES) {
			r.x = min(r.x, 1.0f);
			r.y = min(r.y, 1.0f);
			r.z = min(r.z, 1.0f);
		}
	}

	if(srgb) {
		r.x = color_srgb_to_scene_linear(r.x);
		r.y = color_srgb_to_scene_linear(r.y);
		r.z = color_srgb_to_scene_linear(r.z);
	}
#endif

	return r;
}

#endif

ccl_device void svm_node_tex_image(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint id = node.y;
	uint co_offset, out_offset, alpha_offset, srgb;

	decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &srgb);

	float3 co = stack_load_float3(stack, co_offset);
	uint use_alpha = stack_valid(alpha_offset);
	float4 f = svm_image_texture(kg, id, co.x, co.y, srgb, use_alpha);

	if(stack_valid(out_offset))
		stack_store_float3(stack, out_offset, make_float3(f.x, f.y, f.z));
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, f.w);
}

ccl_device void svm_node_tex_image_box(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	/* get object space normal */
	float3 N = sd->N;

	N = sd->N;
	if(sd->object != OBJECT_NONE)
		object_inverse_normal_transform(kg, sd, &N);

	/* project from direction vector to barycentric coordinates in triangles */
	N.x = fabsf(N.x);
	N.y = fabsf(N.y);
	N.z = fabsf(N.z);

	N /= (N.x + N.y + N.z);

	/* basic idea is to think of this as a triangle, each corner representing
	 * one of the 3 faces of the cube. in the corners we have single textures,
	 * in between we blend between two textures, and in the middle we a blend
	 * between three textures.
	 *
	 * the Nxyz values are the barycentric coordinates in an equilateral
	 * triangle, which in case of blending, in the middle has a smaller
	 * equilateral triangle where 3 textures blend. this divides things into
	 * 7 zones, with an if() test for each zone */

	float3 weight = make_float3(0.0f, 0.0f, 0.0f);
	float blend = __int_as_float(node.w);
	float limit = 0.5f*(1.0f + blend);

	/* first test for corners with single texture */
	if(N.x > limit*(N.x + N.y) && N.x > limit*(N.x + N.z)) {
		weight.x = 1.0f;
	}
	else if(N.y > limit*(N.x + N.y) && N.y > limit*(N.y + N.z)) {
		weight.y = 1.0f;
	}
	else if(N.z > limit*(N.x + N.z) && N.z > limit*(N.y + N.z)) {
		weight.z = 1.0f;
	}
	else if(blend > 0.0f) {
		/* in case of blending, test for mixes between two textures */
		if(N.z < (1.0f - limit)*(N.y + N.x)) {
			weight.x = N.x/(N.x + N.y);
			weight.x = clamp((weight.x - 0.5f*(1.0f - blend))/blend, 0.0f, 1.0f);
			weight.y = 1.0f - weight.x;
		}
		else if(N.x < (1.0f - limit)*(N.y + N.z)) {
			weight.y = N.y/(N.y + N.z);
			weight.y = clamp((weight.y - 0.5f*(1.0f - blend))/blend, 0.0f, 1.0f);
			weight.z = 1.0f - weight.y;
		}
		else if(N.y < (1.0f - limit)*(N.x + N.z)) {
			weight.x = N.x/(N.x + N.z);
			weight.x = clamp((weight.x - 0.5f*(1.0f - blend))/blend, 0.0f, 1.0f);
			weight.z = 1.0f - weight.x;
		}
		else {
			/* last case, we have a mix between three */
			weight.x = ((2.0f - limit)*N.x + (limit - 1.0f))/(2.0f*limit - 1.0f);
			weight.y = ((2.0f - limit)*N.y + (limit - 1.0f))/(2.0f*limit - 1.0f);
			weight.z = ((2.0f - limit)*N.z + (limit - 1.0f))/(2.0f*limit - 1.0f);
		}
	}

	/* now fetch textures */
	uint co_offset, out_offset, alpha_offset, srgb;
	decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &srgb);

	float3 co = stack_load_float3(stack, co_offset);
	uint id = node.y;

	float4 f = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	uint use_alpha = stack_valid(alpha_offset);

	if(weight.x > 0.0f)
		f += weight.x*svm_image_texture(kg, id, co.y, co.z, srgb, use_alpha);
	if(weight.y > 0.0f)
		f += weight.y*svm_image_texture(kg, id, co.x, co.z, srgb, use_alpha);
	if(weight.z > 0.0f)
		f += weight.z*svm_image_texture(kg, id, co.y, co.x, srgb, use_alpha);

	if(stack_valid(out_offset))
		stack_store_float3(stack, out_offset, make_float3(f.x, f.y, f.z));
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, f.w);
}


ccl_device void svm_node_tex_environment(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint id = node.y;
	uint co_offset, out_offset, alpha_offset, srgb;
	uint projection = node.w;

	decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &srgb);

	float3 co = stack_load_float3(stack, co_offset);
	float2 uv;

	co = normalize(co);
	
	if(projection == 0)
		uv = direction_to_equirectangular(co);
	else
		uv = direction_to_mirrorball(co);

	uint use_alpha = stack_valid(alpha_offset);
	float4 f = svm_image_texture(kg, id, uv.x, uv.y, srgb, use_alpha);

	if(stack_valid(out_offset))
		stack_store_float3(stack, out_offset, make_float3(f.x, f.y, f.z));
	if(stack_valid(alpha_offset))
		stack_store_float(stack, alpha_offset, f.w);
}

CCL_NAMESPACE_END

