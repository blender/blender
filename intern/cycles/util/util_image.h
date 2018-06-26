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

CCL_NAMESPACE_END

#endif /* __UTIL_IMAGE_H__ */

#include "util/util_image_impl.h"
