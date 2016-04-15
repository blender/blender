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

/* Texture limits on various devices. */

#define TEX_NUM_FLOAT_IMAGES	5

/* generic */
#define TEX_NUM_IMAGES			88
#define TEX_IMAGE_BYTE_START	TEX_NUM_FLOAT_IMAGES

/* extended gpu */
#define TEX_EXTENDED_NUM_IMAGES_GPU		145

/* extended cpu */
#define TEX_EXTENDED_NUM_FLOAT_IMAGES	1024
#define TEX_EXTENDED_NUM_IMAGES_CPU		1024
#define TEX_EXTENDED_IMAGE_BYTE_START	TEX_EXTENDED_NUM_FLOAT_IMAGES

/* Limitations for packed images.
 *
 * Technically number of textures is unlimited, but it should in
 * fact be in sync with CPU limitations.
 */
#define TEX_PACKED_NUM_IMAGES			1024

/* Color to use when textures are not found. */
#define TEX_IMAGE_MISSING_R 1
#define TEX_IMAGE_MISSING_G 0
#define TEX_IMAGE_MISSING_B 1
#define TEX_IMAGE_MISSING_A 1

CCL_NAMESPACE_END

#endif /* __UTIL_TEXTURE_H__ */
