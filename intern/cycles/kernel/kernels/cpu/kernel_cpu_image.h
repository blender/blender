/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __KERNEL_CPU_IMAGE_H__
#define __KERNEL_CPU_IMAGE_H__

#ifdef __KERNEL_CPU__

CCL_NAMESPACE_BEGIN

ccl_device float4 kernel_tex_image_interp_impl(KernelGlobals *kg, int tex, float x, float y)
{
	if(tex >= TEX_START_HALF_CPU)
		return kg->texture_half_images[tex - TEX_START_HALF_CPU].interp(x, y);
	else if(tex >= TEX_START_HALF4_CPU)
		return kg->texture_half4_images[tex - TEX_START_HALF4_CPU].interp(x, y);
	else if(tex >= TEX_START_BYTE_CPU)
		return kg->texture_byte_images[tex - TEX_START_BYTE_CPU].interp(x, y);
	else if(tex >= TEX_START_FLOAT_CPU)
		return kg->texture_float_images[tex - TEX_START_FLOAT_CPU].interp(x, y);
	else if(tex >= TEX_START_BYTE4_CPU)
		return kg->texture_byte4_images[tex - TEX_START_BYTE4_CPU].interp(x, y);
	else
		return kg->texture_float4_images[tex].interp(x, y);
}

ccl_device float4 kernel_tex_image_interp_3d_impl(KernelGlobals *kg, int tex, float x, float y, float z)
{
	if(tex >= TEX_START_HALF_CPU)
		return kg->texture_half_images[tex - TEX_START_HALF_CPU].interp_3d(x, y, z);
	else if(tex >= TEX_START_HALF4_CPU)
		return kg->texture_half4_images[tex - TEX_START_HALF4_CPU].interp_3d(x, y, z);
	else if(tex >= TEX_START_BYTE_CPU)
		return kg->texture_byte_images[tex - TEX_START_BYTE_CPU].interp_3d(x, y, z);
	else if(tex >= TEX_START_FLOAT_CPU)
		return kg->texture_float_images[tex - TEX_START_FLOAT_CPU].interp_3d(x, y, z);
	else if(tex >= TEX_START_BYTE4_CPU)
		return kg->texture_byte4_images[tex - TEX_START_BYTE4_CPU].interp_3d(x, y, z);
	else
		return kg->texture_float4_images[tex].interp_3d(x, y, z);

}

ccl_device float4 kernel_tex_image_interp_3d_ex_impl(KernelGlobals *kg, int tex, float x, float y, float z, int interpolation)
{
	if(tex >= TEX_START_HALF_CPU)
		return kg->texture_half4_images[tex - TEX_START_HALF_CPU].interp_3d_ex(x, y, z, interpolation);
	else if(tex >= TEX_START_HALF4_CPU)
		return kg->texture_half_images[tex - TEX_START_HALF4_CPU].interp_3d_ex(x, y, z, interpolation);
	else if(tex >= TEX_START_BYTE_CPU)
		return kg->texture_byte_images[tex - TEX_START_BYTE_CPU].interp_3d_ex(x, y, z, interpolation);
	else if(tex >= TEX_START_FLOAT_CPU)
		return kg->texture_float_images[tex - TEX_START_FLOAT_CPU].interp_3d_ex(x, y, z, interpolation);
	else if(tex >= TEX_START_BYTE4_CPU)
		return kg->texture_byte4_images[tex - TEX_START_BYTE4_CPU].interp_3d_ex(x, y, z, interpolation);
	else
		return kg->texture_float4_images[tex].interp_3d_ex(x, y, z, interpolation);
}

CCL_NAMESPACE_END

#endif  // __KERNEL_CPU__


#endif // __KERNEL_CPU_IMAGE_H__
