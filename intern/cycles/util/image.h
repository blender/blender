/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* OpenImageIO is used for all image file reading and writing. */

#include <OpenImageIO/imageio.h>

#include "util/half.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

using OIIO::AutoStride;
using OIIO::ImageInput;
using OIIO::ImageOutput;
using OIIO::ImageSpec;

template<typename T>
void util_image_resize_pixels(const vector<T> &input_pixels,
                              const size_t input_width,
                              const size_t input_height,
                              const size_t input_depth,
                              const size_t components,
                              vector<T> *output_pixels,
                              size_t *output_width,
                              size_t *output_height,
                              size_t *output_depth);

/* Cast input pixel from unknown storage to float. */
template<typename T> inline float util_image_cast_to_float(T value);

template<> inline float util_image_cast_to_float(const float value)
{
  return value;
}
template<> inline float util_image_cast_to_float(const uchar value)
{
  return (float)value / 255.0f;
}
template<> inline float util_image_cast_to_float(const uint16_t value)
{
  return (float)value / 65535.0f;
}
template<> inline float util_image_cast_to_float(half value)
{
  return half_to_float_image(value);
}

/* Cast float value to output pixel type. */
template<typename T> inline T util_image_cast_from_float(const float value);

template<> inline float util_image_cast_from_float(const float value)
{
  return value;
}
template<> inline uchar util_image_cast_from_float(const float value)
{
  if (value < 0.0f) {
    return 0;
  }
  if (value > (1.0f - 0.5f / 255.0f)) {
    return 255;
  }
  return (uchar)((255.0f * value) + 0.5f);  // NOLINT
}
template<> inline uint16_t util_image_cast_from_float(const float value)
{
  if (value < 0.0f) {
    return 0;
  }
  if (value > (1.0f - 0.5f / 65535.0f)) {
    return 65535;
  }
  return (uint16_t)((65535.0f * value) + 0.5f);  // NOLINT
}
template<> inline half util_image_cast_from_float(const float value)
{
  return float_to_half_image(value);
}

/* Multiply image pixels in native data format. */
template<typename T> inline T util_image_multiply_native(T a, T b);

template<> inline float util_image_multiply_native(const float a, const float b)
{
  return a * b;
}
template<> inline uchar util_image_multiply_native(const uchar a, const uchar b)
{
  return ((uint32_t)a * (uint32_t)b) / 255;
}
template<> inline uint16_t util_image_multiply_native(const uint16_t a, const uint16_t b)
{
  return ((uint32_t)a * (uint32_t)b) / 65535;
}
template<> inline half util_image_multiply_native(half a, half b)
{
  return float_to_half_image(half_to_float_image(a) * half_to_float_image(b));
}

CCL_NAMESPACE_END

#include "util/image_impl.h"  // IWYU pragma: export
