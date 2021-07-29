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

ccl_device float4 svm_image_texture(KernelGlobals *kg, int id, float x, float y, uint srgb, uint use_alpha)
{
#ifdef __KERNEL_CPU__
	float4 r = kernel_tex_image_interp(id, x, y);
#elif defined(__KERNEL_OPENCL__)
	float4 r = kernel_tex_image_interp(kg, id, x, y);
#else
	float4 r;

#  if __CUDA_ARCH__ < 300
	/* not particularly proud of this massive switch, what are the
	 * alternatives?
	 * - use a single big 1D texture, and do our own lookup/filtering
	 * - group by size and use a 3d texture, performance impact
	 * - group into larger texture with some padding for correct lerp
	 *
	 * also note that cuda has a textures limit (128 for Fermi, 256 for Kepler),
	 * and we cannot use all since we still need some for other storage */

	switch(id) {
		case 0: r = kernel_tex_image_interp(__tex_image_float4_000, x, y); break;
		case 8: r = kernel_tex_image_interp(__tex_image_float4_008, x, y); break;
		case 16: r = kernel_tex_image_interp(__tex_image_float4_016, x, y); break;
		case 24: r = kernel_tex_image_interp(__tex_image_float4_024, x, y); break;
		case 32: r = kernel_tex_image_interp(__tex_image_float4_032, x, y); break;
		case 1: r = kernel_tex_image_interp(__tex_image_byte4_001, x, y); break;
		case 9: r = kernel_tex_image_interp(__tex_image_byte4_009, x, y); break;
		case 17: r = kernel_tex_image_interp(__tex_image_byte4_017, x, y); break;
		case 25: r = kernel_tex_image_interp(__tex_image_byte4_025, x, y); break;
		case 33: r = kernel_tex_image_interp(__tex_image_byte4_033, x, y); break;
		case 41: r = kernel_tex_image_interp(__tex_image_byte4_041, x, y); break;
		case 49: r = kernel_tex_image_interp(__tex_image_byte4_049, x, y); break;
		case 57: r = kernel_tex_image_interp(__tex_image_byte4_057, x, y); break;
		case 65: r = kernel_tex_image_interp(__tex_image_byte4_065, x, y); break;
		case 73: r = kernel_tex_image_interp(__tex_image_byte4_073, x, y); break;
		case 81: r = kernel_tex_image_interp(__tex_image_byte4_081, x, y); break;
		case 89: r = kernel_tex_image_interp(__tex_image_byte4_089, x, y); break;
		case 97: r = kernel_tex_image_interp(__tex_image_byte4_097, x, y); break;
		case 105: r = kernel_tex_image_interp(__tex_image_byte4_105, x, y); break;
		case 113: r = kernel_tex_image_interp(__tex_image_byte4_113, x, y); break;
		case 121: r = kernel_tex_image_interp(__tex_image_byte4_121, x, y); break;
		case 129: r = kernel_tex_image_interp(__tex_image_byte4_129, x, y); break;
		case 137: r = kernel_tex_image_interp(__tex_image_byte4_137, x, y); break;
		case 145: r = kernel_tex_image_interp(__tex_image_byte4_145, x, y); break;
		case 153: r = kernel_tex_image_interp(__tex_image_byte4_153, x, y); break;
		case 161: r = kernel_tex_image_interp(__tex_image_byte4_161, x, y); break;
		case 169: r = kernel_tex_image_interp(__tex_image_byte4_169, x, y); break;
		case 177: r = kernel_tex_image_interp(__tex_image_byte4_177, x, y); break;
		case 185: r = kernel_tex_image_interp(__tex_image_byte4_185, x, y); break;
		case 193: r = kernel_tex_image_interp(__tex_image_byte4_193, x, y); break;
		case 201: r = kernel_tex_image_interp(__tex_image_byte4_201, x, y); break;
		case 209: r = kernel_tex_image_interp(__tex_image_byte4_209, x, y); break;
		case 217: r = kernel_tex_image_interp(__tex_image_byte4_217, x, y); break;
		case 225: r = kernel_tex_image_interp(__tex_image_byte4_225, x, y); break;
		case 233: r = kernel_tex_image_interp(__tex_image_byte4_233, x, y); break;
		case 241: r = kernel_tex_image_interp(__tex_image_byte4_241, x, y); break;
		case 249: r = kernel_tex_image_interp(__tex_image_byte4_249, x, y); break;
		case 257: r = kernel_tex_image_interp(__tex_image_byte4_257, x, y); break;
		case 265: r = kernel_tex_image_interp(__tex_image_byte4_265, x, y); break;
		case 273: r = kernel_tex_image_interp(__tex_image_byte4_273, x, y); break;
		case 281: r = kernel_tex_image_interp(__tex_image_byte4_281, x, y); break;
		case 289: r = kernel_tex_image_interp(__tex_image_byte4_289, x, y); break;
		case 297: r = kernel_tex_image_interp(__tex_image_byte4_297, x, y); break;
		case 305: r = kernel_tex_image_interp(__tex_image_byte4_305, x, y); break;
		case 313: r = kernel_tex_image_interp(__tex_image_byte4_313, x, y); break;
		case 321: r = kernel_tex_image_interp(__tex_image_byte4_321, x, y); break;
		case 329: r = kernel_tex_image_interp(__tex_image_byte4_329, x, y); break;
		case 337: r = kernel_tex_image_interp(__tex_image_byte4_337, x, y); break;
		case 345: r = kernel_tex_image_interp(__tex_image_byte4_345, x, y); break;
		case 353: r = kernel_tex_image_interp(__tex_image_byte4_353, x, y); break;
		case 361: r = kernel_tex_image_interp(__tex_image_byte4_361, x, y); break;
		case 369: r = kernel_tex_image_interp(__tex_image_byte4_369, x, y); break;
		case 377: r = kernel_tex_image_interp(__tex_image_byte4_377, x, y); break;
		case 385: r = kernel_tex_image_interp(__tex_image_byte4_385, x, y); break;
		case 393: r = kernel_tex_image_interp(__tex_image_byte4_393, x, y); break;
		case 401: r = kernel_tex_image_interp(__tex_image_byte4_401, x, y); break;
		case 409: r = kernel_tex_image_interp(__tex_image_byte4_409, x, y); break;
		case 417: r = kernel_tex_image_interp(__tex_image_byte4_417, x, y); break;
		case 425: r = kernel_tex_image_interp(__tex_image_byte4_425, x, y); break;
		case 433: r = kernel_tex_image_interp(__tex_image_byte4_433, x, y); break;
		case 441: r = kernel_tex_image_interp(__tex_image_byte4_441, x, y); break;
		case 449: r = kernel_tex_image_interp(__tex_image_byte4_449, x, y); break;
		case 457: r = kernel_tex_image_interp(__tex_image_byte4_457, x, y); break;
		case 465: r = kernel_tex_image_interp(__tex_image_byte4_465, x, y); break;
		case 473: r = kernel_tex_image_interp(__tex_image_byte4_473, x, y); break;
		case 481: r = kernel_tex_image_interp(__tex_image_byte4_481, x, y); break;
		case 489: r = kernel_tex_image_interp(__tex_image_byte4_489, x, y); break;
		case 497: r = kernel_tex_image_interp(__tex_image_byte4_497, x, y); break;
		case 505: r = kernel_tex_image_interp(__tex_image_byte4_505, x, y); break;
		case 513: r = kernel_tex_image_interp(__tex_image_byte4_513, x, y); break;
		case 521: r = kernel_tex_image_interp(__tex_image_byte4_521, x, y); break;
		case 529: r = kernel_tex_image_interp(__tex_image_byte4_529, x, y); break;
		case 537: r = kernel_tex_image_interp(__tex_image_byte4_537, x, y); break;
		case 545: r = kernel_tex_image_interp(__tex_image_byte4_545, x, y); break;
		case 553: r = kernel_tex_image_interp(__tex_image_byte4_553, x, y); break;
		case 561: r = kernel_tex_image_interp(__tex_image_byte4_561, x, y); break;
		case 569: r = kernel_tex_image_interp(__tex_image_byte4_569, x, y); break;
		case 577: r = kernel_tex_image_interp(__tex_image_byte4_577, x, y); break;
		case 585: r = kernel_tex_image_interp(__tex_image_byte4_585, x, y); break;
		case 593: r = kernel_tex_image_interp(__tex_image_byte4_593, x, y); break;
		case 601: r = kernel_tex_image_interp(__tex_image_byte4_601, x, y); break;
		case 609: r = kernel_tex_image_interp(__tex_image_byte4_609, x, y); break;
		case 617: r = kernel_tex_image_interp(__tex_image_byte4_617, x, y); break;
		case 625: r = kernel_tex_image_interp(__tex_image_byte4_625, x, y); break;
		case 633: r = kernel_tex_image_interp(__tex_image_byte4_633, x, y); break;
		case 641: r = kernel_tex_image_interp(__tex_image_byte4_641, x, y); break;
		case 649: r = kernel_tex_image_interp(__tex_image_byte4_649, x, y); break;
		case 657: r = kernel_tex_image_interp(__tex_image_byte4_657, x, y); break;
		case 665: r = kernel_tex_image_interp(__tex_image_byte4_665, x, y); break;
		default:
			kernel_assert(0);
			return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
#  else
	CUtexObject tex = kernel_tex_fetch(__bindless_mapping, id);
	/* float4, byte4 and half4 */
	const int texture_type = kernel_tex_type(id);
	if(texture_type == IMAGE_DATA_TYPE_FLOAT4 ||
	   texture_type == IMAGE_DATA_TYPE_BYTE4 ||
	   texture_type == IMAGE_DATA_TYPE_HALF4)
	{
		r = kernel_tex_image_interp_float4(tex, x, y);
	}
	/* float, byte and half */
	else {
		float f = kernel_tex_image_interp_float(tex, x, y);
		r = make_float4(f, f, f, 1.0f);
	}
#  endif
#endif

	const float alpha = r.w;

	if(use_alpha && alpha != 1.0f && alpha != 0.0f) {
		r /= alpha;
		const int texture_type = kernel_tex_type(id);
		if(texture_type == IMAGE_DATA_TYPE_BYTE4 ||
		   texture_type == IMAGE_DATA_TYPE_BYTE)
		{
			r = min(r, make_float4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		r.w = alpha;
	}

	if(srgb) {
		r = color_srgb_to_scene_linear_v4(r);
	}

	return r;
}

/* Remap coordnate from 0..1 box to -1..-1 */
ccl_device_inline float3 texco_remap_square(float3 co)
{
	return (co - make_float3(0.5f, 0.5f, 0.5f)) * 2.0f;
}

ccl_device void svm_node_tex_image(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint id = node.y;
	uint co_offset, out_offset, alpha_offset, srgb;

	decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &srgb);

	float3 co = stack_load_float3(stack, co_offset);
	float2 tex_co;
	uint use_alpha = stack_valid(alpha_offset);
	if(node.w == NODE_IMAGE_PROJ_SPHERE) {
		co = texco_remap_square(co);
		tex_co = map_to_sphere(co);
	}
	else if(node.w == NODE_IMAGE_PROJ_TUBE) {
		co = texco_remap_square(co);
		tex_co = map_to_tube(co);
	}
	else {
		tex_co = make_float2(co.x, co.y);
	}
	float4 f = svm_image_texture(kg, id, tex_co.x, tex_co.y, srgb, use_alpha);

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
			weight.x = saturate((weight.x - 0.5f*(1.0f - blend))/blend);
			weight.y = 1.0f - weight.x;
		}
		else if(N.x < (1.0f - limit)*(N.y + N.z)) {
			weight.y = N.y/(N.y + N.z);
			weight.y = saturate((weight.y - 0.5f*(1.0f - blend))/blend);
			weight.z = 1.0f - weight.y;
		}
		else if(N.y < (1.0f - limit)*(N.x + N.z)) {
			weight.x = N.x/(N.x + N.z);
			weight.x = saturate((weight.x - 0.5f*(1.0f - blend))/blend);
			weight.z = 1.0f - weight.x;
		}
		else {
			/* last case, we have a mix between three */
			weight.x = ((2.0f - limit)*N.x + (limit - 1.0f))/(2.0f*limit - 1.0f);
			weight.y = ((2.0f - limit)*N.y + (limit - 1.0f))/(2.0f*limit - 1.0f);
			weight.z = ((2.0f - limit)*N.z + (limit - 1.0f))/(2.0f*limit - 1.0f);
		}
	}
	else {
		/* Desperate mode, no valid choice anyway, fallback to one side.*/
		weight.x = 1.0f;
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

	co = safe_normalize(co);

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

