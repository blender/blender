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

#ifndef __UTIL_TEXTURE_H__
#define __UTIL_TEXTURE_H__

CCL_NAMESPACE_BEGIN

/* Texture limits on devices. */

/* CUDA (Geforce 4xx and 5xx) */
#define TEX_NUM_FLOAT4_CUDA      5
#define TEX_NUM_BYTE4_CUDA       84
#define TEX_NUM_HALF4_CUDA       0
#define TEX_NUM_FLOAT_CUDA       0
#define TEX_NUM_BYTE_CUDA        0
#define TEX_NUM_HALF_CUDA        0
#define TEX_START_FLOAT4_CUDA    0
#define TEX_START_BYTE4_CUDA     TEX_NUM_FLOAT4_CUDA
#define TEX_START_HALF4_CUDA     (TEX_NUM_FLOAT4_CUDA + TEX_NUM_BYTE4_CUDA)
#define TEX_START_FLOAT_CUDA     (TEX_NUM_FLOAT4_CUDA + TEX_NUM_BYTE4_CUDA + TEX_NUM_HALF4_CUDA)
#define TEX_START_BYTE_CUDA      (TEX_NUM_FLOAT4_CUDA + TEX_NUM_BYTE4_CUDA + TEX_NUM_HALF4_CUDA + TEX_NUM_FLOAT_CUDA)
#define TEX_START_HALF_CUDA      (TEX_NUM_FLOAT4_CUDA + TEX_NUM_BYTE4_CUDA + TEX_NUM_HALF4_CUDA + TEX_NUM_FLOAT_CUDA + TEX_NUM_BYTE_CUDA)

/* Any architecture other than old CUDA cards */
#define TEX_NUM_MAX (INT_MAX >> 4)

/* Color to use when textures are not found. */
#define TEX_IMAGE_MISSING_R 1
#define TEX_IMAGE_MISSING_G 0
#define TEX_IMAGE_MISSING_B 1
#define TEX_IMAGE_MISSING_A 1

#if defined (__KERNEL_CUDA__) && (__CUDA_ARCH__ < 300)
#  define kernel_tex_type(tex) (tex < TEX_START_BYTE4_CUDA ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_BYTE4)
#  define kernel_tex_index(tex) (tex)
#else
#  define kernel_tex_type(tex) (tex & IMAGE_DATA_TYPE_MASK)
#  define kernel_tex_index(tex) (tex >> IMAGE_DATA_TYPE_SHIFT)
#endif

CCL_NAMESPACE_END

#endif /* __UTIL_TEXTURE_H__ */
