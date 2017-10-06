/*
 * Copyright 2017 Blender Foundation
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

#if __CUDA_ARCH__ >= 300

/* Kepler */

ccl_device float4 kernel_tex_image_interp(void *kg, int id, float x, float y)
{
	const TextureInfo& info = kernel_tex_fetch(__texture_info, id);
	CUtexObject tex = (CUtexObject)info.data;

	/* float4, byte4 and half4 */
	const int texture_type = kernel_tex_type(id);
	if(texture_type == IMAGE_DATA_TYPE_FLOAT4 ||
	   texture_type == IMAGE_DATA_TYPE_BYTE4 ||
	   texture_type == IMAGE_DATA_TYPE_HALF4)
	{
		return tex2D<float4>(tex, x, y);
	}
	/* float, byte and half */
	else {
		float f = tex2D<float>(tex, x, y);
		return make_float4(f, f, f, 1.0f);
	}
}

ccl_device float4 kernel_tex_image_interp_3d(void *kg, int id, float x, float y, float z)
{
	const TextureInfo& info = kernel_tex_fetch(__texture_info, id);
	CUtexObject tex = (CUtexObject)info.data;

	const int texture_type = kernel_tex_type(id);
	if(texture_type == IMAGE_DATA_TYPE_FLOAT4 ||
	   texture_type == IMAGE_DATA_TYPE_BYTE4 ||
	   texture_type == IMAGE_DATA_TYPE_HALF4)
	{
		return tex3D<float4>(tex, x, y, z);
	}
	else {
		float f = tex3D<float>(tex, x, y, z);
		return make_float4(f, f, f, 1.0f);
	}
}

#else

/* Fermi */

ccl_device float4 kernel_tex_image_interp(void *kg, int id, float x, float y)
{
	float4 r;
	switch(id) {
		case 0: r = tex2D(__tex_image_float4_000, x, y); break;
		case 8: r = tex2D(__tex_image_float4_008, x, y); break;
		case 16: r = tex2D(__tex_image_float4_016, x, y); break;
		case 24: r = tex2D(__tex_image_float4_024, x, y); break;
		case 32: r = tex2D(__tex_image_float4_032, x, y); break;
		case 1: r = tex2D(__tex_image_byte4_001, x, y); break;
		case 9: r = tex2D(__tex_image_byte4_009, x, y); break;
		case 17: r = tex2D(__tex_image_byte4_017, x, y); break;
		case 25: r = tex2D(__tex_image_byte4_025, x, y); break;
		case 33: r = tex2D(__tex_image_byte4_033, x, y); break;
		case 41: r = tex2D(__tex_image_byte4_041, x, y); break;
		case 49: r = tex2D(__tex_image_byte4_049, x, y); break;
		case 57: r = tex2D(__tex_image_byte4_057, x, y); break;
		case 65: r = tex2D(__tex_image_byte4_065, x, y); break;
		case 73: r = tex2D(__tex_image_byte4_073, x, y); break;
		case 81: r = tex2D(__tex_image_byte4_081, x, y); break;
		case 89: r = tex2D(__tex_image_byte4_089, x, y); break;
		case 97: r = tex2D(__tex_image_byte4_097, x, y); break;
		case 105: r = tex2D(__tex_image_byte4_105, x, y); break;
		case 113: r = tex2D(__tex_image_byte4_113, x, y); break;
		case 121: r = tex2D(__tex_image_byte4_121, x, y); break;
		case 129: r = tex2D(__tex_image_byte4_129, x, y); break;
		case 137: r = tex2D(__tex_image_byte4_137, x, y); break;
		case 145: r = tex2D(__tex_image_byte4_145, x, y); break;
		case 153: r = tex2D(__tex_image_byte4_153, x, y); break;
		case 161: r = tex2D(__tex_image_byte4_161, x, y); break;
		case 169: r = tex2D(__tex_image_byte4_169, x, y); break;
		case 177: r = tex2D(__tex_image_byte4_177, x, y); break;
		case 185: r = tex2D(__tex_image_byte4_185, x, y); break;
		case 193: r = tex2D(__tex_image_byte4_193, x, y); break;
		case 201: r = tex2D(__tex_image_byte4_201, x, y); break;
		case 209: r = tex2D(__tex_image_byte4_209, x, y); break;
		case 217: r = tex2D(__tex_image_byte4_217, x, y); break;
		case 225: r = tex2D(__tex_image_byte4_225, x, y); break;
		case 233: r = tex2D(__tex_image_byte4_233, x, y); break;
		case 241: r = tex2D(__tex_image_byte4_241, x, y); break;
		case 249: r = tex2D(__tex_image_byte4_249, x, y); break;
		case 257: r = tex2D(__tex_image_byte4_257, x, y); break;
		case 265: r = tex2D(__tex_image_byte4_265, x, y); break;
		case 273: r = tex2D(__tex_image_byte4_273, x, y); break;
		case 281: r = tex2D(__tex_image_byte4_281, x, y); break;
		case 289: r = tex2D(__tex_image_byte4_289, x, y); break;
		case 297: r = tex2D(__tex_image_byte4_297, x, y); break;
		case 305: r = tex2D(__tex_image_byte4_305, x, y); break;
		case 313: r = tex2D(__tex_image_byte4_313, x, y); break;
		case 321: r = tex2D(__tex_image_byte4_321, x, y); break;
		case 329: r = tex2D(__tex_image_byte4_329, x, y); break;
		case 337: r = tex2D(__tex_image_byte4_337, x, y); break;
		case 345: r = tex2D(__tex_image_byte4_345, x, y); break;
		case 353: r = tex2D(__tex_image_byte4_353, x, y); break;
		case 361: r = tex2D(__tex_image_byte4_361, x, y); break;
		case 369: r = tex2D(__tex_image_byte4_369, x, y); break;
		case 377: r = tex2D(__tex_image_byte4_377, x, y); break;
		case 385: r = tex2D(__tex_image_byte4_385, x, y); break;
		case 393: r = tex2D(__tex_image_byte4_393, x, y); break;
		case 401: r = tex2D(__tex_image_byte4_401, x, y); break;
		case 409: r = tex2D(__tex_image_byte4_409, x, y); break;
		case 417: r = tex2D(__tex_image_byte4_417, x, y); break;
		case 425: r = tex2D(__tex_image_byte4_425, x, y); break;
		case 433: r = tex2D(__tex_image_byte4_433, x, y); break;
		case 441: r = tex2D(__tex_image_byte4_441, x, y); break;
		case 449: r = tex2D(__tex_image_byte4_449, x, y); break;
		case 457: r = tex2D(__tex_image_byte4_457, x, y); break;
		case 465: r = tex2D(__tex_image_byte4_465, x, y); break;
		case 473: r = tex2D(__tex_image_byte4_473, x, y); break;
		case 481: r = tex2D(__tex_image_byte4_481, x, y); break;
		case 489: r = tex2D(__tex_image_byte4_489, x, y); break;
		case 497: r = tex2D(__tex_image_byte4_497, x, y); break;
		case 505: r = tex2D(__tex_image_byte4_505, x, y); break;
		case 513: r = tex2D(__tex_image_byte4_513, x, y); break;
		case 521: r = tex2D(__tex_image_byte4_521, x, y); break;
		case 529: r = tex2D(__tex_image_byte4_529, x, y); break;
		case 537: r = tex2D(__tex_image_byte4_537, x, y); break;
		case 545: r = tex2D(__tex_image_byte4_545, x, y); break;
		case 553: r = tex2D(__tex_image_byte4_553, x, y); break;
		case 561: r = tex2D(__tex_image_byte4_561, x, y); break;
		case 569: r = tex2D(__tex_image_byte4_569, x, y); break;
		case 577: r = tex2D(__tex_image_byte4_577, x, y); break;
		case 585: r = tex2D(__tex_image_byte4_585, x, y); break;
		case 593: r = tex2D(__tex_image_byte4_593, x, y); break;
		case 601: r = tex2D(__tex_image_byte4_601, x, y); break;
		case 609: r = tex2D(__tex_image_byte4_609, x, y); break;
		case 617: r = tex2D(__tex_image_byte4_617, x, y); break;
		case 625: r = tex2D(__tex_image_byte4_625, x, y); break;
		case 633: r = tex2D(__tex_image_byte4_633, x, y); break;
		case 641: r = tex2D(__tex_image_byte4_641, x, y); break;
		case 649: r = tex2D(__tex_image_byte4_649, x, y); break;
		case 657: r = tex2D(__tex_image_byte4_657, x, y); break;
		case 665: r = tex2D(__tex_image_byte4_665, x, y); break;
		default: r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	return r;
}

ccl_device float4 kernel_tex_image_interp_3d(void *kg, int id, float x, float y, float z)
{
	float4 r;
	switch(id) {
		case 0: r = tex3D(__tex_image_float4_3d_000, x, y, z); break;
		case 8: r = tex3D(__tex_image_float4_3d_008, x, y, z); break;
		case 16: r = tex3D(__tex_image_float4_3d_016, x, y, z); break;
		case 24: r = tex3D(__tex_image_float4_3d_024, x, y, z); break;
		case 32: r = tex3D(__tex_image_float4_3d_032, x, y, z); break;
	}
	return r;
}

#endif

