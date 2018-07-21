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

#ifndef __UTIL_IMAGE_IMPL_H__
#define __UTIL_IMAGE_IMPL_H__

#include "util/util_algorithm.h"
#include "util/util_half.h"
#include "util/util_image.h"

CCL_NAMESPACE_BEGIN

namespace {

template<typename T>
const T *util_image_read(const vector<T>& pixels,
                         const size_t width,
                         const size_t height,
                         const size_t /*depth*/,
                         const size_t components,
                         const size_t x, const size_t y, const size_t z) {
	const size_t index = ((size_t)z * (width * height) +
	                      (size_t)y * width +
	                      (size_t)x) * components;
	return &pixels[index];
}

template<typename T>
void util_image_downscale_sample(const vector<T>& pixels,
                                 const size_t width,
                                 const size_t height,
                                 const size_t depth,
                                 const size_t components,
                                 const size_t kernel_size,
                                 const float x,
                                 const float y,
                                 const float z,
                                 T *result)
{
	assert(components <= 4);
	const size_t ix = (size_t)x,
	             iy = (size_t)y,
	             iz = (size_t)z;
	/* TODO(sergey): Support something smarter than box filer. */
	float accum[4] = {0};
	size_t count = 0;
	for(size_t dz = 0; dz < kernel_size; ++dz) {
		for(size_t dy = 0; dy < kernel_size; ++dy) {
			for(size_t dx = 0; dx < kernel_size; ++dx) {
				const size_t nx = ix + dx,
				             ny = iy + dy,
				             nz = iz + dz;
				if(nx >= width || ny >= height || nz >= depth) {
					continue;
				}
				const T *pixel = util_image_read(pixels,
				                                 width, height, depth,
				                                 components,
				                                 nx, ny, nz);
				for(size_t k = 0; k < components; ++k) {
					accum[k] += util_image_cast_to_float(pixel[k]);
				}
				++count;
			}
		}
	}
	if(count != 0) {
		const float inv_count = 1.0f / (float)count;
		for(size_t k = 0; k < components; ++k) {
			result[k] = util_image_cast_from_float<T>(accum[k] * inv_count);
		}
	}
	else {
		for(size_t k = 0; k < components; ++k) {
			result[k] = T(0.0f);
		}
	}
}

template<typename T>
void util_image_downscale_pixels(const vector<T>& input_pixels,
                                 const size_t input_width,
                                 const size_t input_height,
                                 const size_t input_depth,
                                 const size_t components,
                                 const float inv_scale_factor,
                                 const size_t output_width,
                                 const size_t output_height,
                                 const size_t output_depth,
                                 vector<T> *output_pixels)
{
	const size_t kernel_size = (size_t)(inv_scale_factor + 0.5f);
	for(size_t z = 0; z < output_depth; ++z) {
		for(size_t y = 0; y < output_height; ++y) {
			for(size_t x = 0; x < output_width; ++x) {
				const float input_x = (float)x * inv_scale_factor,
				            input_y = (float)y * inv_scale_factor,
				            input_z = (float)z * inv_scale_factor;
				const size_t output_index =
				        (z * output_width * output_height +
				         y * output_width + x) * components;
				util_image_downscale_sample(input_pixels,
				                            input_width, input_height, input_depth,
				                            components,
				                            kernel_size,
				                            input_x, input_y, input_z,
				                            &output_pixels->at(output_index));
			}
		}
	}
}

}  /* namespace */

template<typename T>
void util_image_resize_pixels(const vector<T>& input_pixels,
                              const size_t input_width,
                              const size_t input_height,
                              const size_t input_depth,
                              const size_t components,
                              const float scale_factor,
                              vector<T> *output_pixels,
                              size_t *output_width,
                              size_t *output_height,
                              size_t *output_depth)
{
	/* Early output for case when no scaling is applied. */
	if(scale_factor == 1.0f) {
		*output_width = input_width;
		*output_height = input_height;
		*output_depth = input_depth;
		*output_pixels = input_pixels;
		return;
	}
	/* First of all, we calculate output image dimensions.
	 * We clamp them to be 1 pixel at least so we do not generate degenerate
	 * image.
	 */
	*output_width = max((size_t)((float)input_width * scale_factor), (size_t)1);
	*output_height = max((size_t)((float)input_height * scale_factor), (size_t)1);
	*output_depth = max((size_t)((float)input_depth * scale_factor), (size_t)1);
	/* Prepare pixel storage for the result. */
	const size_t num_output_pixels = ((*output_width) *
	                                  (*output_height) *
	                                  (*output_depth)) * components;
	output_pixels->resize(num_output_pixels);
	if(scale_factor < 1.0f) {
		const float inv_scale_factor = 1.0f / scale_factor;
		util_image_downscale_pixels(input_pixels,
		                            input_width, input_height, input_depth,
		                            components,
		                            inv_scale_factor,
		                            *output_width, *output_height, *output_depth,
		                            output_pixels);
	} else {
		/* TODO(sergey): Needs implementation. */
	}
}

CCL_NAMESPACE_END

#endif  /* __UTIL_IMAGE_IMPL_H__ */
