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

#include "render/colorspace.h"

#include "util/util_color.h"
#include "util/util_image.h"
#include "util/util_half.h"
#include "util/util_logging.h"
#include "util/util_math.h"
#include "util/util_thread.h"
#include "util/util_vector.h"

#ifdef WITH_OCIO
#  include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif

CCL_NAMESPACE_BEGIN

/* Builtin colorspaces. */
ustring u_colorspace_auto;
ustring u_colorspace_raw("__builtin_raw");
ustring u_colorspace_srgb("__builtin_srgb");

/* Cached data. */
#ifdef WITH_OCIO
static thread_mutex cache_mutex;
static unordered_map<ustring, ustring, ustringHash> cached_colorspaces;
static unordered_map<ustring, OCIO::ConstProcessorRcPtr, ustringHash> cached_processors;
#endif

ColorSpaceProcessor *ColorSpaceManager::get_processor(ustring colorspace)
{
#ifdef WITH_OCIO
  /* Only use this for OpenColorIO color spaces, not the builtin ones. */
  assert(colorspace != u_colorspace_srgb && colorspace != u_colorspace_auto);

  if (colorspace == u_colorspace_raw) {
    return NULL;
  }

  OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
  if (!config) {
    return NULL;
  }

  /* Cache processor until free_memory(), memory overhead is expected to be
   * small and the processor is likely to be reused. */
  thread_scoped_lock cache_lock(cache_mutex);
  if (cached_processors.find(colorspace) == cached_processors.end()) {
    try {
      cached_processors[colorspace] = config->getProcessor(colorspace.c_str(), "scene_linear");
    }
    catch (OCIO::Exception &exception) {
      cached_processors[colorspace] = OCIO::ConstProcessorRcPtr();
      VLOG(1) << "Colorspace " << colorspace.c_str()
              << " can't be converted to scene_linear: " << exception.what();
    }
  }

  const OCIO::Processor *processor = cached_processors[colorspace].get();
  return (ColorSpaceProcessor *)processor;
#else
  /* No OpenColorIO. */
  (void)colorspace;
  return NULL;
#endif
}

ustring ColorSpaceManager::detect_known_colorspace(ustring colorspace,
                                                   const char *file_format,
                                                   bool is_float)
{
  if (colorspace == u_colorspace_auto) {
    /* Auto detect sRGB or raw if none specified. */
    if (is_float) {
      bool srgb = (colorspace == "sRGB" || colorspace == "GammaCorrected" ||
                   (colorspace.empty() &&
                    (strcmp(file_format, "png") == 0 || strcmp(file_format, "tiff") == 0 ||
                     strcmp(file_format, "dpx") == 0 || strcmp(file_format, "jpeg2000") == 0)));
      return srgb ? u_colorspace_srgb : u_colorspace_raw;
    }
    else {
      return u_colorspace_srgb;
    }
  }
  else if (colorspace == u_colorspace_srgb || colorspace == u_colorspace_raw) {
    /* Builtin colorspaces. */
    return colorspace;
  }
  else {
    /* Use OpenColorIO. */
#ifdef WITH_OCIO
    {
      thread_scoped_lock cache_lock(cache_mutex);
      /* Cached lookup. */
      if (cached_colorspaces.find(colorspace) != cached_colorspaces.end()) {
        return cached_colorspaces[colorspace];
      }
    }

    /* Detect if it matches a simple builtin colorspace. */
    bool is_scene_linear, is_srgb;
    is_builtin_colorspace(colorspace, is_scene_linear, is_srgb);

    thread_scoped_lock cache_lock(cache_mutex);
    if (is_scene_linear) {
      VLOG(1) << "Colorspace " << colorspace.string() << " is no-op";
      cached_colorspaces[colorspace] = u_colorspace_raw;
      return u_colorspace_raw;
    }
    else if (is_srgb) {
      VLOG(1) << "Colorspace " << colorspace.string() << " is sRGB";
      cached_colorspaces[colorspace] = u_colorspace_srgb;
      return u_colorspace_srgb;
    }

    /* Verify if we can convert from the requested color space. */
    if (!get_processor(colorspace)) {
      OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
      if (!config || !config->getColorSpace(colorspace.c_str())) {
        VLOG(1) << "Colorspace " << colorspace.c_str() << " not found, using raw instead";
      }
      else {
        VLOG(1) << "Colorspace " << colorspace.c_str()
                << " can't be converted to scene_linear, using raw instead";
      }
      cached_colorspaces[colorspace] = u_colorspace_raw;
      return u_colorspace_raw;
    }

    /* Convert to/from colorspace with OpenColorIO. */
    VLOG(1) << "Colorspace " << colorspace.string() << " handled through OpenColorIO";
    cached_colorspaces[colorspace] = colorspace;
    return colorspace;
#else
    VLOG(1) << "Colorspace " << colorspace.c_str() << " not available, built without OpenColorIO";
    return u_colorspace_raw;
#endif
  }
}

void ColorSpaceManager::is_builtin_colorspace(ustring colorspace,
                                              bool &is_scene_linear,
                                              bool &is_srgb)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)get_processor(colorspace);
  if (!processor) {
    is_scene_linear = false;
    is_srgb = false;
    return;
  }

  is_scene_linear = true;
  is_srgb = true;
  for (int i = 0; i < 256; i++) {
    float v = i / 255.0f;

    float cR[3] = {v, 0, 0};
    float cG[3] = {0, v, 0};
    float cB[3] = {0, 0, v};
    float cW[3] = {v, v, v};
    processor->applyRGB(cR);
    processor->applyRGB(cG);
    processor->applyRGB(cB);
    processor->applyRGB(cW);

    /* Make sure that there is no channel crosstalk. */
    if (fabsf(cR[1]) > 1e-5f || fabsf(cR[2]) > 1e-5f || fabsf(cG[0]) > 1e-5f ||
        fabsf(cG[2]) > 1e-5f || fabsf(cB[0]) > 1e-5f || fabsf(cB[1]) > 1e-5f) {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }
    /* Make sure that the three primaries combine linearly. */
    if (!compare_floats(cR[0], cW[0], 1e-6f, 64) || !compare_floats(cG[1], cW[1], 1e-6f, 64) ||
        !compare_floats(cB[2], cW[2], 1e-6f, 64)) {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }
    /* Make sure that the three channels behave identically. */
    if (!compare_floats(cW[0], cW[1], 1e-6f, 64) || !compare_floats(cW[1], cW[2], 1e-6f, 64)) {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }

    float out_v = average(make_float3(cW[0], cW[1], cW[2]));
    if (!compare_floats(v, out_v, 1e-6f, 64)) {
      is_scene_linear = false;
    }
    if (!compare_floats(color_srgb_to_linear(v), out_v, 1e-6f, 64)) {
      is_srgb = false;
    }
  }
#else
  (void)colorspace;
  is_scene_linear = false;
  is_srgb = false;
#endif
}

#ifdef WITH_OCIO

template<typename T> inline float4 cast_to_float4(T *data)
{
  return make_float4(util_image_cast_to_float(data[0]),
                     util_image_cast_to_float(data[1]),
                     util_image_cast_to_float(data[2]),
                     util_image_cast_to_float(data[3]));
}

template<typename T> inline void cast_from_float4(T *data, float4 value)
{
  data[0] = util_image_cast_from_float<T>(value.x);
  data[1] = util_image_cast_from_float<T>(value.y);
  data[2] = util_image_cast_from_float<T>(value.z);
  data[3] = util_image_cast_from_float<T>(value.w);
}

/* Slower versions for other all data types, which needs to convert to float and back. */
template<typename T, bool compress_as_srgb = false>
inline void processor_apply_pixels(const OCIO::Processor *processor,
                                   T *pixels,
                                   size_t width,
                                   size_t height)
{
  /* Process large images in chunks to keep temporary memory requirement down. */
  size_t y_chunk_size = max(1, 16 * 1024 * 1024 / (sizeof(float4) * width));
  vector<float4> float_pixels(y_chunk_size * width);

  for (size_t y0 = 0; y0 < height; y0 += y_chunk_size) {
    size_t y1 = std::min(y0 + y_chunk_size, height);
    size_t i = 0;

    for (size_t y = y0; y < y1; y++) {
      for (size_t x = 0; x < width; x++, i++) {
        float_pixels[i] = cast_to_float4(pixels + 4 * (y * width + x));
      }
    }

    OCIO::PackedImageDesc desc((float *)float_pixels.data(), width, y_chunk_size, 4);
    processor->apply(desc);

    i = 0;
    for (size_t y = y0; y < y1; y++) {
      for (size_t x = 0; x < width; x++, i++) {
        float4 value = float_pixels[i];
        if (compress_as_srgb) {
          value = color_linear_to_srgb_v4(value);
        }
        cast_from_float4(pixels + 4 * (y * width + x), value);
      }
    }
  }
}

/* Fast version for float images, which OpenColorIO can handle natively. */
template<>
inline void processor_apply_pixels(const OCIO::Processor *processor,
                                   float *pixels,
                                   size_t width,
                                   size_t height)
{
  OCIO::PackedImageDesc desc(pixels, width, height, 4);
  processor->apply(desc);
}
#endif

template<typename T>
void ColorSpaceManager::to_scene_linear(ustring colorspace,
                                        T *pixels,
                                        size_t width,
                                        size_t height,
                                        size_t depth,
                                        bool compress_as_srgb)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)get_processor(colorspace);

  if (processor) {
    if (compress_as_srgb) {
      /* Compress output as sRGB. */
      for (size_t z = 0; z < depth; z++) {
        processor_apply_pixels<T, true>(processor, &pixels[z * width * height], width, height);
      }
    }
    else {
      /* Write output as scene linear directly. */
      for (size_t z = 0; z < depth; z++) {
        processor_apply_pixels<T>(processor, &pixels[z * width * height], width, height);
      }
    }
  }
#else
  (void)colorspace;
  (void)pixels;
  (void)width;
  (void)height;
  (void)depth;
  (void)compress_as_srgb;
#endif
}

void ColorSpaceManager::to_scene_linear(ColorSpaceProcessor *processor_,
                                        float *pixel,
                                        int channels)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)processor_;

  if (processor) {
    if (channels == 3) {
      processor->applyRGB(pixel);
    }
    else if (channels == 4) {
      if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
        /* Fast path for RGBA. */
        processor->applyRGB(pixel);
      }
      else {
        /* Unassociate and associate alpha since color management should not
         * be affected by transparency. */
        float alpha = pixel[3];
        float inv_alpha = 1.0f / alpha;

        pixel[0] *= inv_alpha;
        pixel[1] *= inv_alpha;
        pixel[2] *= inv_alpha;

        processor->applyRGB(pixel);

        pixel[0] *= alpha;
        pixel[1] *= alpha;
        pixel[2] *= alpha;
      }
    }
  }
#else
  (void)processor_;
  (void)pixel;
  (void)channels;
#endif
}

void ColorSpaceManager::free_memory()
{
#ifdef WITH_OCIO
  map_free_memory(cached_colorspaces);
  map_free_memory(cached_colorspaces);
#endif
}

/* Template instanstations so we don't have to inline functions. */
template void ColorSpaceManager::to_scene_linear(ustring, uchar *, size_t, size_t, size_t, bool);
template void ColorSpaceManager::to_scene_linear(ustring, ushort *, size_t, size_t, size_t, bool);
template void ColorSpaceManager::to_scene_linear(ustring, half *, size_t, size_t, size_t, bool);
template void ColorSpaceManager::to_scene_linear(ustring, float *, size_t, size_t, size_t, bool);

CCL_NAMESPACE_END
