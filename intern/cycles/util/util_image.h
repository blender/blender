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

#ifndef __UTIL_IMAGE_H__
#define __UTIL_IMAGE_H__

/* OpenImageIO is used for all image file reading and writing. */

#include <OpenImageIO/imageio.h>

#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

OIIO_NAMESPACE_USING

template<typename T>
void util_image_resize_pixels(const vector<T>& input_pixels,
                              const size_t input_width,
                              const size_t input_height,
                              const size_t input_depth,
                              const size_t components,
                              vector<T> *output_pixels,
                              size_t *output_width,
                              size_t *output_height,
                              size_t *output_depth);

/* Cast input pixel from unknown storage to float. */
template<typename T>
inline float util_image_cast_to_float(T value);

template<>
inline float util_image_cast_to_float(float value)
{
	return value;
}
template<>
inline float util_image_cast_to_float(uchar value)
{
	return (float)value / 255.0f;
}
template<>
inline float util_image_cast_to_float(uint16_t value)
{
	return (float)value / 65535.0f;
}
template<>
inline float util_image_cast_to_float(half value)
{
	return half_to_float(value);
}

/* Cast float value to output pixel type. */
template<typename T>
inline T util_image_cast_from_float(float value);

template<>
inline float util_image_cast_from_float(float value)
{
	return value;
}
template<>
inline uchar util_image_cast_from_float(float value)
{
	if(value < 0.0f) {
		return 0;
	}
	else if(value > (1.0f - 0.5f / 255.0f)) {
		return 255;
	}
	return (uchar)((255.0f * value) + 0.5f);
}
template<>
inline uint16_t util_image_cast_from_float(float value)
{
	if(value < 0.0f) {
		return 0;
	}
	else if(value > (1.0f - 0.5f / 65535.0f)) {
		return 65535;
	}
	return (uint16_t)((65535.0f * value) + 0.5f);
}
template<>
inline half util_image_cast_from_float(float value)
{
	return float_to_half(value);
}

CCL_NAMESPACE_END

#endif /* __UTIL_IMAGE_H__ */

#include "util/util_image_impl.h"
