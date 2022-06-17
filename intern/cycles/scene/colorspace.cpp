/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/colorspace.h"

#include "util/color.h"
#include "util/half.h"
#include "util/image.h"
#include "util/log.h"
#include "util/math.h"
#include "util/thread.h"
#include "util/vector.h"

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
static thread_mutex cache_colorspaces_mutex;
static thread_mutex cache_processors_mutex;
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
  thread_scoped_lock cache_processors_lock(cache_processors_mutex);
  if (cached_processors.find(colorspace) == cached_processors.end()) {
    try {
      cached_processors[colorspace] = config->getProcessor(colorspace.c_str(), "scene_linear");
    }
    catch (OCIO::Exception &exception) {
      cached_processors[colorspace] = OCIO::ConstProcessorRcPtr();
      VLOG_WARNING << "Colorspace " << colorspace.c_str()
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

bool ColorSpaceManager::colorspace_is_data(ustring colorspace)
{
  if (colorspace == u_colorspace_auto || colorspace == u_colorspace_raw ||
      colorspace == u_colorspace_srgb) {
    return false;
  }

#ifdef WITH_OCIO
  OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
  if (!config) {
    return false;
  }

  try {
    OCIO::ConstColorSpaceRcPtr space = config->getColorSpace(colorspace.c_str());
    return space && space->isData();
  }
  catch (OCIO::Exception &) {
    return false;
  }
#else
  return false;
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
      thread_scoped_lock cache_lock(cache_colorspaces_mutex);
      /* Cached lookup. */
      if (cached_colorspaces.find(colorspace) != cached_colorspaces.end()) {
        return cached_colorspaces[colorspace];
      }
    }

    /* Detect if it matches a simple builtin colorspace. */
    bool is_scene_linear, is_srgb;
    is_builtin_colorspace(colorspace, is_scene_linear, is_srgb);

    thread_scoped_lock cache_lock(cache_colorspaces_mutex);
    if (is_scene_linear) {
      VLOG_INFO << "Colorspace " << colorspace.string() << " is no-op";
      cached_colorspaces[colorspace] = u_colorspace_raw;
      return u_colorspace_raw;
    }
    else if (is_srgb) {
      VLOG_INFO << "Colorspace " << colorspace.string() << " is sRGB";
      cached_colorspaces[colorspace] = u_colorspace_srgb;
      return u_colorspace_srgb;
    }

    /* Verify if we can convert from the requested color space. */
    if (!get_processor(colorspace)) {
      OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
      if (!config || !config->getColorSpace(colorspace.c_str())) {
        VLOG_WARNING << "Colorspace " << colorspace.c_str() << " not found, using raw instead";
      }
      else {
        VLOG_WARNING << "Colorspace " << colorspace.c_str()
                     << " can't be converted to scene_linear, using raw instead";
      }
      cached_colorspaces[colorspace] = u_colorspace_raw;
      return u_colorspace_raw;
    }

    /* Convert to/from colorspace with OpenColorIO. */
    VLOG_INFO << "Colorspace " << colorspace.string() << " handled through OpenColorIO";
    cached_colorspaces[colorspace] = colorspace;
    return colorspace;
#else
    VLOG_WARNING << "Colorspace " << colorspace.c_str()
                 << " not available, built without OpenColorIO";
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

  OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();
  is_scene_linear = true;
  is_srgb = true;
  for (int i = 0; i < 256; i++) {
    float v = i / 255.0f;

    float cR[3] = {v, 0, 0};
    float cG[3] = {0, v, 0};
    float cB[3] = {0, 0, v};
    float cW[3] = {v, v, v};
    device_processor->applyRGB(cR);
    device_processor->applyRGB(cG);
    device_processor->applyRGB(cB);
    device_processor->applyRGB(cW);

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
inline void processor_apply_pixels_rgba(const OCIO::Processor *processor,
                                        T *pixels,
                                        size_t num_pixels)
{
  /* TODO: implement faster version for when we know the conversion
   * is a simple matrix transform between linear spaces. In that case
   * un-premultiply is not needed. */
  OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();

  /* Process large images in chunks to keep temporary memory requirement down. */
  const size_t chunk_size = std::min((size_t)(16 * 1024 * 1024), num_pixels);
  vector<float4> float_pixels(chunk_size);

  for (size_t j = 0; j < num_pixels; j += chunk_size) {
    size_t width = std::min(chunk_size, num_pixels - j);

    for (size_t i = 0; i < width; i++) {
      float4 value = cast_to_float4(pixels + 4 * (j + i));

      if (!(value.w <= 0.0f || value.w == 1.0f)) {
        float inv_alpha = 1.0f / value.w;
        value.x *= inv_alpha;
        value.y *= inv_alpha;
        value.z *= inv_alpha;
      }

      float_pixels[i] = value;
    }

    OCIO::PackedImageDesc desc((float *)float_pixels.data(), width, 1, 4);
    device_processor->apply(desc);

    for (size_t i = 0; i < width; i++) {
      float4 value = float_pixels[i];

      if (compress_as_srgb) {
        value = color_linear_to_srgb_v4(value);
      }

      if (!(value.w <= 0.0f || value.w == 1.0f)) {
        value.x *= value.w;
        value.y *= value.w;
        value.z *= value.w;
      }

      cast_from_float4(pixels + 4 * (j + i), value);
    }
  }
}

template<typename T, bool compress_as_srgb = false>
inline void processor_apply_pixels_grayscale(const OCIO::Processor *processor,
                                             T *pixels,
                                             size_t num_pixels)
{
  OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();

  /* Process large images in chunks to keep temporary memory requirement down. */
  const size_t chunk_size = std::min((size_t)(16 * 1024 * 1024), num_pixels);
  vector<float> float_pixels(chunk_size * 3);

  for (size_t j = 0; j < num_pixels; j += chunk_size) {
    size_t width = std::min(chunk_size, num_pixels - j);

    /* Convert to 3 channels, since that's the minimum required by OpenColorIO. */
    {
      const T *pixel = pixels + j;
      float *fpixel = float_pixels.data();
      for (size_t i = 0; i < width; i++, pixel++, fpixel += 3) {
        const float f = util_image_cast_to_float<T>(*pixel);
        fpixel[0] = f;
        fpixel[1] = f;
        fpixel[2] = f;
      }
    }

    OCIO::PackedImageDesc desc((float *)float_pixels.data(), width, 1, 3);
    device_processor->apply(desc);

    {
      T *pixel = pixels + j;
      const float *fpixel = float_pixels.data();
      for (size_t i = 0; i < width; i++, pixel++, fpixel += 3) {
        float f = average(make_float3(fpixel[0], fpixel[1], fpixel[2]));
        if (compress_as_srgb) {
          f = color_linear_to_srgb(f);
        }
        *pixel = util_image_cast_from_float<T>(f);
      }
    }
  }
}

#endif

template<typename T>
void ColorSpaceManager::to_scene_linear(
    ustring colorspace, T *pixels, size_t num_pixels, bool is_rgba, bool compress_as_srgb)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)get_processor(colorspace);

  if (processor) {
    if (is_rgba) {
      if (compress_as_srgb) {
        /* Compress output as sRGB. */
        processor_apply_pixels_rgba<T, true>(processor, pixels, num_pixels);
      }
      else {
        /* Write output as scene linear directly. */
        processor_apply_pixels_rgba<T>(processor, pixels, num_pixels);
      }
    }
    else {
      if (compress_as_srgb) {
        /* Compress output as sRGB. */
        processor_apply_pixels_grayscale<T, true>(processor, pixels, num_pixels);
      }
      else {
        /* Write output as scene linear directly. */
        processor_apply_pixels_grayscale<T>(processor, pixels, num_pixels);
      }
    }
  }
#else
  (void)colorspace;
  (void)pixels;
  (void)num_pixels;
  (void)is_rgba;
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
    OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();
    if (channels == 1) {
      float3 rgb = make_float3(pixel[0], pixel[0], pixel[0]);
      device_processor->applyRGB(&rgb.x);
      pixel[0] = average(rgb);
    }
    if (channels == 3) {
      device_processor->applyRGB(pixel);
    }
    else if (channels == 4) {
      if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
        /* Fast path for RGBA. */
        device_processor->applyRGB(pixel);
      }
      else {
        /* Un-associate and associate alpha since color management should not
         * be affected by transparency. */
        float alpha = pixel[3];
        float inv_alpha = 1.0f / alpha;

        pixel[0] *= inv_alpha;
        pixel[1] *= inv_alpha;
        pixel[2] *= inv_alpha;

        device_processor->applyRGB(pixel);

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
  map_free_memory(cached_processors);
#endif
}

/* Template instantiations so we don't have to inline functions. */
template void ColorSpaceManager::to_scene_linear(ustring, uchar *, size_t, bool, bool);
template void ColorSpaceManager::to_scene_linear(ustring, ushort *, size_t, bool, bool);
template void ColorSpaceManager::to_scene_linear(ustring, half *, size_t, bool, bool);
template void ColorSpaceManager::to_scene_linear(ustring, float *, size_t, bool, bool);

CCL_NAMESPACE_END
