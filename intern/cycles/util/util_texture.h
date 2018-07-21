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
#define TEX_NUM_MAX (INT_MAX >> 4)

/* Color to use when textures are not found. */
#define TEX_IMAGE_MISSING_R 1
#define TEX_IMAGE_MISSING_G 0
#define TEX_IMAGE_MISSING_B 1
#define TEX_IMAGE_MISSING_A 1

/* Texture type. */
#define kernel_tex_type(tex) (tex & IMAGE_DATA_TYPE_MASK)

/* Interpolation types for textures
 * cuda also use texture space to store other objects */
typedef enum InterpolationType {
	INTERPOLATION_NONE = -1,
	INTERPOLATION_LINEAR = 0,
	INTERPOLATION_CLOSEST = 1,
	INTERPOLATION_CUBIC = 2,
	INTERPOLATION_SMART = 3,

	INTERPOLATION_NUM_TYPES,
} InterpolationType;

/* Texture types
 * Since we store the type in the lower bits of a flat index,
 * the shift and bit mask constant below need to be kept in sync. */
typedef enum ImageDataType {
	IMAGE_DATA_TYPE_FLOAT4 = 0,
	IMAGE_DATA_TYPE_BYTE4 = 1,
	IMAGE_DATA_TYPE_HALF4 = 2,
	IMAGE_DATA_TYPE_FLOAT = 3,
	IMAGE_DATA_TYPE_BYTE = 4,
	IMAGE_DATA_TYPE_HALF = 5,
	IMAGE_DATA_TYPE_USHORT4 = 6,
	IMAGE_DATA_TYPE_USHORT = 7,

	IMAGE_DATA_NUM_TYPES
} ImageDataType;

#define IMAGE_DATA_TYPE_SHIFT 3
#define IMAGE_DATA_TYPE_MASK 0x7

/* Extension types for textures.
 *
 * Defines how the image is extrapolated past its original bounds. */
typedef enum ExtensionType {
	/* Cause the image to repeat horizontally and vertically. */
	EXTENSION_REPEAT = 0,
	/* Extend by repeating edge pixels of the image. */
	EXTENSION_EXTEND = 1,
	/* Clip to image size and set exterior pixels as transparent. */
	EXTENSION_CLIP = 2,

	EXTENSION_NUM_TYPES,
} ExtensionType;

typedef struct TextureInfo {
	/* Pointer, offset or texture depending on device. */
	uint64_t data;
	/* Buffer number for OpenCL. */
	uint cl_buffer;
	/* Interpolation and extension type. */
	uint interpolation, extension;
	/* Dimensions. */
	uint width, height, depth;
} TextureInfo;

CCL_NAMESPACE_END

#endif /* __UTIL_TEXTURE_H__ */
