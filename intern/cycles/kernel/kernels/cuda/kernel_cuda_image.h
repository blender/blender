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

/* w0, w1, w2, and w3 are the four cubic B-spline basis functions. */
ccl_device float cubic_w0(float a)
{
	return (1.0f/6.0f)*(a*(a*(-a + 3.0f) - 3.0f) + 1.0f);
}

ccl_device float cubic_w1(float a)
{
	return (1.0f/6.0f)*(a*a*(3.0f*a - 6.0f) + 4.0f);
}

ccl_device float cubic_w2(float a)
{
	return (1.0f/6.0f)*(a*(a*(-3.0f*a + 3.0f) + 3.0f) + 1.0f);
}

ccl_device float cubic_w3(float a)
{
	return (1.0f/6.0f)*(a*a*a);
}

/* g0 and g1 are the two amplitude functions. */
ccl_device float cubic_g0(float a)
{
	return cubic_w0(a) + cubic_w1(a);
}

ccl_device float cubic_g1(float a)
{
	return cubic_w2(a) + cubic_w3(a);
}

/* h0 and h1 are the two offset functions */
ccl_device float cubic_h0(float a)
{
	/* Note +0.5 offset to compensate for CUDA linear filtering convention. */
	return -1.0f + cubic_w1(a) / (cubic_w0(a) + cubic_w1(a)) + 0.5f;
}

ccl_device float cubic_h1(float a)
{
	return 1.0f + cubic_w3(a) / (cubic_w2(a) + cubic_w3(a)) + 0.5f;
}

/* Fast bicubic texture lookup using 4 bilinear lookups, adapted from CUDA samples. */
template<typename T>
ccl_device T kernel_tex_image_interp_bicubic(const TextureInfo& info, CUtexObject tex, float x, float y)
{
	x = (x * info.width) - 0.5f;
	y = (y * info.height) - 0.5f;

	float px = floor(x);
	float py = floor(y);
	float fx = x - px;
	float fy = y - py;

	float g0x = cubic_g0(fx);
	float g1x = cubic_g1(fx);
	float x0 = (px + cubic_h0(fx)) / info.width;
	float x1 = (px + cubic_h1(fx)) / info.width;
	float y0 = (py + cubic_h0(fy)) / info.height;
	float y1 = (py + cubic_h1(fy)) / info.height;

	return cubic_g0(fy) * (g0x * tex2D<T>(tex, x0, y0) +
	                       g1x * tex2D<T>(tex, x1, y0)) +
	       cubic_g1(fy) * (g0x * tex2D<T>(tex, x0, y1) +
	                       g1x * tex2D<T>(tex, x1, y1));
}

/* Fast tricubic texture lookup using 8 trilinear lookups. */
template<typename T>
ccl_device T kernel_tex_image_interp_bicubic_3d(const TextureInfo& info, CUtexObject tex, float x, float y, float z)
{
	x = (x * info.width) - 0.5f;
	y = (y * info.height) - 0.5f;
	z = (z * info.depth) - 0.5f;

	float px = floor(x);
	float py = floor(y);
	float pz = floor(z);
	float fx = x - px;
	float fy = y - py;
	float fz = z - pz;

	float g0x = cubic_g0(fx);
	float g1x = cubic_g1(fx);
	float g0y = cubic_g0(fy);
	float g1y = cubic_g1(fy);
	float g0z = cubic_g0(fz);
	float g1z = cubic_g1(fz);

	float x0 = (px + cubic_h0(fx)) / info.width;
	float x1 = (px + cubic_h1(fx)) / info.width;
	float y0 = (py + cubic_h0(fy)) / info.height;
	float y1 = (py + cubic_h1(fy)) / info.height;
	float z0 = (pz + cubic_h0(fz)) / info.depth;
	float z1 = (pz + cubic_h1(fz)) / info.depth;

	return g0z * (g0y * (g0x * tex3D<T>(tex, x0, y0, z0) +
	                     g1x * tex3D<T>(tex, x1, y0, z0)) +
	              g1y * (g0x * tex3D<T>(tex, x0, y1, z0) +
	                     g1x * tex3D<T>(tex, x1, y1, z0))) +
	       g1z * (g0y * (g0x * tex3D<T>(tex, x0, y0, z1) +
	                     g1x * tex3D<T>(tex, x1, y0, z1)) +
	              g1y * (g0x * tex3D<T>(tex, x0, y1, z1) +
	                     g1x * tex3D<T>(tex, x1, y1, z1)));
}

ccl_device float4 kernel_tex_image_interp(KernelGlobals *kg, int id, float x, float y)
{
	const TextureInfo& info = kernel_tex_fetch(__texture_info, id);
	CUtexObject tex = (CUtexObject)info.data;

	/* float4, byte4, ushort4 and half4 */
	const int texture_type = kernel_tex_type(id);
	if(texture_type == IMAGE_DATA_TYPE_FLOAT4 ||
	   texture_type == IMAGE_DATA_TYPE_BYTE4 ||
	   texture_type == IMAGE_DATA_TYPE_HALF4 ||
	   texture_type == IMAGE_DATA_TYPE_USHORT4)
	{
		if(info.interpolation == INTERPOLATION_CUBIC) {
			return kernel_tex_image_interp_bicubic<float4>(info, tex, x, y);
		}
		else {
			return tex2D<float4>(tex, x, y);
		}
	}
	/* float, byte and half */
	else {
		float f;

		if(info.interpolation == INTERPOLATION_CUBIC) {
			f = kernel_tex_image_interp_bicubic<float>(info, tex, x, y);
		}
		else {
			f = tex2D<float>(tex, x, y);
		}

		return make_float4(f, f, f, 1.0f);
	}
}

ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals *kg, int id, float x, float y, float z, InterpolationType interp)
{
	const TextureInfo& info = kernel_tex_fetch(__texture_info, id);
	CUtexObject tex = (CUtexObject)info.data;
	uint interpolation = (interp == INTERPOLATION_NONE)? info.interpolation: interp;

	const int texture_type = kernel_tex_type(id);
	if(texture_type == IMAGE_DATA_TYPE_FLOAT4 ||
	   texture_type == IMAGE_DATA_TYPE_BYTE4 ||
	   texture_type == IMAGE_DATA_TYPE_HALF4 ||
	   texture_type == IMAGE_DATA_TYPE_USHORT4)
	{
		if(interpolation == INTERPOLATION_CUBIC) {
			return kernel_tex_image_interp_bicubic_3d<float4>(info, tex, x, y, z);
		}
		else {
			return tex3D<float4>(tex, x, y, z);
		}
	}
	else {
		float f;

		if(interpolation == INTERPOLATION_CUBIC) {
			f = kernel_tex_image_interp_bicubic_3d<float>(info, tex, x, y, z);
		}
		else {
			f = tex3D<float>(tex, x, y, z);
		}

		return make_float4(f, f, f, 1.0f);
	}
}
