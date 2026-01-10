/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/colorspace.h"
#include "util/color.h"
#include "util/image.h"
#include "util/log.h"
#include "util/map.h"
#include "util/math.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/transform.h"
#include "util/vector.h"

#ifdef WITH_OCIO
#  include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif

CCL_NAMESPACE_BEGIN

/* Builtin colorspaces. */
ustring u_colorspace_auto;
ustring u_colorspace_data("data");
ustring u_colorspace_scene_linear("scene_linear");
ustring u_colorspace_scene_linear_srgb("scene_linear_srgb");
ustring u_colorspace_srgb("__builtin_srgb");

/* Cached data. */
#ifdef WITH_OCIO
static thread_mutex cache_processors_mutex;
static unordered_map<ustring, OCIO::ConstProcessorRcPtr> cache_processors;

static thread_mutex cache_colorspaces_mutex;
static unordered_map<ustring, ustring> cached_colorspaces;

static thread_mutex cache_scene_linear_interop_id_mutex;
static bool cache_scene_linear_interop_id_done = false;
static const char *cache_scene_linear_interop_id = "";
static const char *cache_scene_linear_srgb_interop_id = "";
#endif

static thread_mutex cache_xyz_to_scene_linear_mutex;
static string cache_xyz_to_scene_linear_hash;

ColorSpaceProcessor *ColorSpaceManager::get_processor(ustring colorspace)
{
#ifdef WITH_OCIO
  /* Only use this for OpenColorIO color spaces, not the builtin ones. */
  assert(colorspace != u_colorspace_scene_linear_srgb && colorspace != u_colorspace_auto);

  if (colorspace == u_colorspace_data || colorspace == u_colorspace_scene_linear) {
    return nullptr;
  }

  OCIO::ConstConfigRcPtr config = nullptr;
  try {
    config = OCIO::GetCurrentConfig();
  }
  catch (const OCIO::Exception &exception) {
    LOG_WARNING << "OCIO config error: " << exception.what();
    return nullptr;
  }

  if (!config) {
    return nullptr;
  }

  /* Cache processor until free_memory(), memory overhead is expected to be
   * small and the processor is likely to be reused. */
  const thread_scoped_lock cache_processors_lock(cache_processors_mutex);
  if (cache_processors.find(colorspace) == cache_processors.end()) {
    try {
      if (colorspace == u_colorspace_srgb) {
        /* Linear Rec.709 to sRGB is handled separately in to_scene_linear, here
         * we only need the matrix transform from scene_linear to Linear Rec.709. */
        if (strcmp(get_scene_linear_interop_id(), "lin_rec709_scene") == 0) {
          cache_processors[colorspace] = nullptr;
        }
        else {
          const Transform rgb_to_rec709 = transform_inverse(get_xyz_to_scene_linear_rgb()) *
                                          get_xyz_to_rec709();
          const double data[16] = {rgb_to_rec709.x.x,
                                   rgb_to_rec709.x.y,
                                   rgb_to_rec709.x.z,
                                   rgb_to_rec709.x.w,
                                   rgb_to_rec709.y.x,
                                   rgb_to_rec709.y.y,
                                   rgb_to_rec709.y.z,
                                   rgb_to_rec709.y.w,
                                   rgb_to_rec709.z.x,
                                   rgb_to_rec709.z.y,
                                   rgb_to_rec709.z.z,
                                   rgb_to_rec709.z.w,
                                   0.0f,
                                   0.0f,
                                   0.0f,
                                   1.0f};
          OCIO::MatrixTransformRcPtr tfm = OCIO::MatrixTransform::Create();
          tfm->setMatrix(data);
          cache_processors[colorspace] = config->getProcessor(tfm, OCIO::TRANSFORM_DIR_FORWARD);
        }
      }
      else {
        cache_processors[colorspace] = config->getProcessor(colorspace.c_str(), "scene_linear");
      }
    }
    catch (const OCIO::Exception &exception) {
      cache_processors[colorspace] = OCIO::ConstProcessorRcPtr();
      LOG_WARNING << "Colorspace " << colorspace.c_str()
                  << " can't be converted to scene_linear: " << exception.what();
    }
  }

  const OCIO::Processor *processor = cache_processors[colorspace].get();
  return (ColorSpaceProcessor *)processor;
#else
  /* No OpenColorIO. */
  (void)colorspace;
  return nullptr;
#endif
}

bool ColorSpaceManager::colorspace_is_data(ustring colorspace)
{
  if (colorspace == u_colorspace_data) {
    return true;
  }
  if (colorspace == u_colorspace_auto || colorspace == u_colorspace_scene_linear ||
      colorspace == u_colorspace_scene_linear_srgb || colorspace == u_colorspace_srgb)
  {
    return false;
  }

#ifdef WITH_OCIO
  OCIO::ConstConfigRcPtr config = nullptr;
  try {
    config = OCIO::GetCurrentConfig();
  }
  catch (const OCIO::Exception &exception) {
    LOG_WARNING << "OCIO config error: " << exception.what();
    return false;
  }

  if (!config) {
    return false;
  }

  try {
    const OCIO::ConstColorSpaceRcPtr space = config->getColorSpace(colorspace.c_str());
    return space && space->isData();
  }
  catch (const OCIO::Exception &) {
    return false;
  }
#else
  return false;
#endif
}

const char *ColorSpaceManager::colorspace_interop_id(ustring colorspace)
{
  if (colorspace == u_colorspace_auto) {
    return nullptr;
  }
  if (colorspace == u_colorspace_data) {
    return "data";
  }
  if (colorspace == u_colorspace_srgb) {
    return "srgb_rec709_scene";
  }
  if (colorspace == u_colorspace_scene_linear) {
    const char *interop_id = get_scene_linear_interop_id(false);
    if (!strcmp(interop_id, "unknown")) {
      return interop_id;
    }
  }
  else if (colorspace == u_colorspace_scene_linear_srgb) {
    const char *interop_id = get_scene_linear_interop_id(true);
    if (!strcmp(interop_id, "unknown")) {
      return interop_id;
    }
  }

#ifdef WITH_OCIO
  OCIO::ConstConfigRcPtr config = nullptr;
  try {
    config = OCIO::GetCurrentConfig();
    if (!config) {
      return nullptr;
    }
  }
  catch (const OCIO::Exception &) {
    return nullptr;
  }

  try {
    const OCIO::ConstColorSpaceRcPtr space = config->getColorSpace(colorspace.c_str());
    if (space) {
      if (space->isData()) {
        return "data";
      }
      /* From https://github.com/AcademySoftwareFoundation/ColorInterop. */
      /* TODO: Use native interop ID support in OpenColorIO 2.5. */
      const char *interop_ids[] = {"lin_rec709_scene",
                                   "lin_p3d65_scene",
                                   "lin_rec2020_scene",
                                   "lin_adobergb_scene",
                                   "lin_ap1_scene",
                                   "lin_ap0_scene",
                                   "lin_ciexyzd65_scene",
                                   "srgb_rec709_scene",
                                   "g22_rec709_scene",
                                   "g18_rec709_scene",
                                   "srgb_ap1_scene",
                                   "g22_ap1_scene",
                                   "srgb_p3d65_scene",
                                   "g22_adobergb_scene"};
      for (const char *interop_id : interop_ids) {
        if (space->hasAlias(interop_id)) {
          return interop_id;
        }
      }
    }
  }
  catch (const OCIO::Exception &) {
    return nullptr;
  }
#endif

  return nullptr;
}

ustring ColorSpaceManager::detect_known_colorspace(ustring colorspace,
                                                   const char *file_colorspace,
                                                   const char *file_format,
                                                   bool is_float)
{
  if (colorspace == u_colorspace_auto) {
    /* Auto detect sRGB or raw if none specified. */
    if (is_float) {
      const bool srgb = (strcmp(file_colorspace, "sRGB") == 0 ||
                         strcmp(file_colorspace, "GammaCorrected") == 0 ||
                         (file_colorspace[0] == '\0' &&
                          (strcmp(file_format, "png") == 0 || strcmp(file_format, "jpeg") == 0 ||
                           strcmp(file_format, "tiff") == 0 || strcmp(file_format, "dpx") == 0 ||
                           strcmp(file_format, "jpeg2000") == 0)));
      return srgb ? u_colorspace_srgb : u_colorspace_scene_linear;
    }
    return u_colorspace_srgb;
  }

  /* Builtin colorspaces. */
  if (colorspace == u_colorspace_srgb || colorspace == u_colorspace_scene_linear ||
      colorspace == u_colorspace_scene_linear_srgb || colorspace == u_colorspace_data)
  {
    return colorspace;
  }

  if (colorspace_is_data(colorspace)) {
    return u_colorspace_data;
  }

  /* Use OpenColorIO. */
#ifdef WITH_OCIO
  {
    const thread_scoped_lock cache_lock(cache_colorspaces_mutex);
    /* Cached lookup. */
    if (cached_colorspaces.find(colorspace) != cached_colorspaces.end()) {
      return cached_colorspaces[colorspace];
    }
  }

  /* Detect if it matches a simple builtin colorspace. */
  bool is_scene_linear;
  bool is_scene_linear_srgb;
  is_builtin_colorspace(colorspace, is_scene_linear, is_scene_linear_srgb);

  const thread_scoped_lock cache_lock(cache_colorspaces_mutex);
  if (is_scene_linear) {
    LOG_INFO << "Colorspace " << colorspace.string() << " is no-op";
    cached_colorspaces[colorspace] = u_colorspace_scene_linear;
    return u_colorspace_scene_linear;
  }
  if (is_scene_linear_srgb) {
    LOG_INFO << "Colorspace " << colorspace.string() << " is scene linear sRGB";
    cached_colorspaces[colorspace] = u_colorspace_scene_linear_srgb;
    return u_colorspace_scene_linear_srgb;
  }

  /* Verify if we can convert from the requested color space. */
  if (!get_processor(colorspace)) {
    OCIO::ConstConfigRcPtr config = nullptr;
    try {
      config = OCIO::GetCurrentConfig();
    }
    catch (const OCIO::Exception &exception) {
      LOG_WARNING << "OCIO config error: " << exception.what();
      return u_colorspace_scene_linear;
    }

    if (!config || !config->getColorSpace(colorspace.c_str())) {
      LOG_WARNING << "Colorspace " << colorspace.c_str() << " not found, using raw instead";
    }
    else {
      LOG_WARNING << "Colorspace " << colorspace.c_str()
                  << " can't be converted to scene_linear, using raw instead";
    }
    cached_colorspaces[colorspace] = u_colorspace_scene_linear;
    return u_colorspace_scene_linear;
  }

  /* Convert to/from colorspace with OpenColorIO. */
  LOG_INFO << "Colorspace " << colorspace.string() << " handled through OpenColorIO";
  cached_colorspaces[colorspace] = colorspace;
  return colorspace;
#else
  LOG_WARNING << "Colorspace " << colorspace.c_str()
              << " not available, built without OpenColorIO";
  return u_colorspace_scene_linear;
#endif
}

void ColorSpaceManager::is_builtin_colorspace(ustring colorspace,
                                              bool &is_scene_linear,
                                              bool &is_scene_linear_srgb)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)get_processor(colorspace);
  if (!processor) {
    is_scene_linear = false;
    is_scene_linear_srgb = false;
    return;
  }

  const OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();
  is_scene_linear = true;
  is_scene_linear_srgb = true;
  for (int i = 0; i < 256; i++) {
    const float v = i / 255.0f;

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
        fabsf(cG[2]) > 1e-5f || fabsf(cB[0]) > 1e-5f || fabsf(cB[1]) > 1e-5f)
    {
      is_scene_linear = false;
      is_scene_linear_srgb = false;
      break;
    }
    /* Make sure that the three primaries combine linearly. */
    if (!compare_floats(cR[0], cW[0], 1e-6f, 64) || !compare_floats(cG[1], cW[1], 1e-6f, 64) ||
        !compare_floats(cB[2], cW[2], 1e-6f, 64))
    {
      is_scene_linear = false;
      is_scene_linear_srgb = false;
      break;
    }
    /* Make sure that the three channels behave identically. */
    if (!compare_floats(cW[0], cW[1], 1e-6f, 64) || !compare_floats(cW[1], cW[2], 1e-6f, 64)) {
      is_scene_linear = false;
      is_scene_linear_srgb = false;
      break;
    }

    const float out_v = average(make_float3(cW[0], cW[1], cW[2]));
    if (!compare_floats(v, out_v, 1e-6f, 64)) {
      is_scene_linear = false;
    }
    if (!compare_floats(color_srgb_to_linear(v), out_v, 1e-4f, 64)) {
      is_scene_linear_srgb = false;
    }
  }
#else
  (void)colorspace;
  is_scene_linear = false;
  is_scene_linear_srgb = false;
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

template<typename T> inline void cast_from_float4(T *data, const float4 value)
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
                                        const int64_t width,
                                        const int64_t height,
                                        const int64_t y_stride,
                                        const bool ignore_alpha)
{
  /* TODO: implement faster version for when we know the conversion
   * is a simple matrix transform between linear spaces. In that case
   * un-premultiply is not needed. */
  const OCIO::ConstCPUProcessorRcPtr device_processor = (processor) ?
                                                            processor->getDefaultCPUProcessor() :
                                                            nullptr;

  /* Process large images in chunks to keep temporary memory requirement down. */
  const int64_t chunk_rows = divide_up(std::min((int64_t)(16 * 1024 * 1024), width * height),
                                       width);
  vector<float4> float_pixels(chunk_rows * width);

  for (int64_t row = 0; row < height; row += chunk_rows) {
    const int64_t num_rows = std::min(chunk_rows, height - row);

    for (int64_t j = 0; j < num_rows; j++) {
      T *pixel = pixels + (row + j) * y_stride;
      float4 *float_pixel = float_pixels.data() + j * width;
      for (int64_t i = 0; i < width; i++, pixel += 4, float_pixel++) {
        float4 value = cast_to_float4(pixel);

        if (!ignore_alpha && !(value.w <= 0.0f || value.w == 1.0f)) {
          const float inv_alpha = 1.0f / value.w;
          value.x *= inv_alpha;
          value.y *= inv_alpha;
          value.z *= inv_alpha;
        }

        *float_pixel = value;
      }
    }

    if (processor) {
      const OCIO::PackedImageDesc desc((float *)float_pixels.data(), num_rows * width, 1, 4);
      device_processor->apply(desc);
    }

    for (int64_t j = 0; j < num_rows; j++) {
      T *pixel = pixels + (row + j) * y_stride;
      float4 *float_pixel = float_pixels.data() + j * width;
      for (int64_t i = 0; i < width; i++, pixel += 4, float_pixel++) {
        float4 value = *float_pixel;

        if (compress_as_srgb) {
          value = color_linear_to_srgb_v4(value);
        }

        if (!ignore_alpha && !(value.w <= 0.0f || value.w == 1.0f)) {
          value.x *= value.w;
          value.y *= value.w;
          value.z *= value.w;
        }

        cast_from_float4(pixel, value);
      }
    }
  }
}

template<typename T, bool compress_as_srgb = false>
inline void processor_apply_pixels_grayscale(const OCIO::Processor *processor,
                                             T *pixels,
                                             const int64_t width,
                                             const int64_t height,
                                             const int64_t y_stride)
{
  const OCIO::ConstCPUProcessorRcPtr device_processor = (processor) ?
                                                            processor->getDefaultCPUProcessor() :
                                                            nullptr;

  /* Process large images in chunks to keep temporary memory requirement down. */
  const int64_t chunk_rows = divide_up(std::min((int64_t)(16 * 1024 * 1024), width * height),
                                       width);
  vector<float> float_pixels(chunk_rows * width * 3);

  for (int64_t row = 0; row < height; row += chunk_rows) {
    const int64_t num_rows = std::min(chunk_rows, height - row);

    /* Convert to 3 channels, since that's the minimum required by OpenColorIO. */
    for (int64_t j = 0; j < num_rows; j++) {
      T *pixel = pixels + (row + j) * y_stride;
      float *float_pixel = float_pixels.data() + j * width * 3;
      for (int64_t i = 0; i < width; i++, pixel++, float_pixel += 3) {
        const float f = util_image_cast_to_float<T>(*pixel);
        float_pixel[0] = f;
        float_pixel[1] = f;
        float_pixel[2] = f;
      }
    }

    if (processor) {
      const OCIO::PackedImageDesc desc((float *)float_pixels.data(), num_rows * width, 1, 3);
      device_processor->apply(desc);
    }

    for (int64_t j = 0; j < num_rows; j++) {
      T *pixel = pixels + (row + j) * y_stride;
      float *float_pixel = float_pixels.data() + j * width * 3;
      for (int64_t i = 0; i < width; i++, pixel++, float_pixel += 3) {
        float f = average(make_float3(float_pixel[0], float_pixel[1], float_pixel[2]));
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
void ColorSpaceManager::to_scene_linear(ustring colorspace,
                                        T *pixels,
                                        const int64_t width,
                                        const int64_t height,
                                        const int64_t y_stride,
                                        bool is_rgba,
                                        bool compress_as_srgb,
                                        bool ignore_alpha)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)get_processor(colorspace);

  /* The processor for sRGB does not include the Linear Rec.709 to sRGB transform, that
   * is handled by compress_as_srgb for better performance when scene_linear is Rec.709 */
  if (colorspace == u_colorspace_srgb) {
    assert(!compress_as_srgb);
    compress_as_srgb = true;
  }

  if (is_rgba) {
    if (compress_as_srgb) {
      /* Compress output as sRGB. */
      processor_apply_pixels_rgba<T, true>(
          processor, pixels, width, height, y_stride, ignore_alpha);
    }
    else {
      /* Write output as scene linear directly. */
      processor_apply_pixels_rgba<T>(processor, pixels, width, height, y_stride, ignore_alpha);
    }
  }
  else {
    if (compress_as_srgb) {
      /* Compress output as sRGB. */
      processor_apply_pixels_grayscale<T, true>(processor, pixels, width, height, y_stride);
    }
    else {
      /* Write output as scene linear directly. */
      processor_apply_pixels_grayscale<T>(processor, pixels, width, height, y_stride);
    }
  }
#else
  (void)colorspace;
  (void)pixels;
  (void)width;
  (void)height;
  (void)y_stride;
  (void)is_rgba;
  (void)compress_as_srgb;
#endif
}

void ColorSpaceManager::to_scene_linear(ColorSpaceProcessor *processor_,
                                        float *pixel,
                                        const int channels)
{
#ifdef WITH_OCIO
  const OCIO::Processor *processor = (const OCIO::Processor *)processor_;

  if (processor) {
    const OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();
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
        const float alpha = pixel[3];
        const float inv_alpha = 1.0f / alpha;

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
  map_free_memory(cache_processors);
#endif
}

void ColorSpaceManager::init_fallback_config()
{
#ifdef WITH_OCIO
  OCIO::SetCurrentConfig(OCIO::Config::CreateRaw());
#endif
}

#ifdef WITH_OCIO
static bool to_scene_linear_transform(OCIO::ConstConfigRcPtr &config,
                                      const char *colorspace,
                                      Transform &to_scene_linear)
{
  OCIO::ConstProcessorRcPtr processor;
  try {
    processor = config->getProcessor("scene_linear", colorspace);
  }
  catch (OCIO::Exception &) {
    return false;
  }

  if (!processor) {
    return false;
  }

  const OCIO::ConstCPUProcessorRcPtr device_processor = processor->getDefaultCPUProcessor();
  if (!device_processor) {
    return false;
  }

  to_scene_linear = transform_identity();
  device_processor->applyRGB(&to_scene_linear.x.x);
  device_processor->applyRGB(&to_scene_linear.y.x);
  device_processor->applyRGB(&to_scene_linear.z.x);
  to_scene_linear = transform_transposed_inverse(to_scene_linear);
  return true;
}
#endif

Transform ColorSpaceManager::get_xyz_to_rec709()
{
  /* Default to ITU-BT.709 in case no appropriate transform found.
   * Note XYZ here is defined as having a D65 white point. */
  return make_transform(3.2404542f,
                        -1.5371385f,
                        -0.4985314f,
                        0.0f,
                        -0.9692660f,
                        1.8760108f,
                        0.0415560f,
                        0.0f,
                        0.0556434f,
                        -0.2040259f,
                        1.0572252f,
                        0.0f);
}

Transform ColorSpaceManager::get_xyz_to_rec2020()
{
  return make_transform(1.7166512f,
                        -0.3556708f,
                        -0.2533663f,
                        0.0f,
                        -0.6666844,
                        1.6164812f,
                        0.0157685f,
                        0.0f,
                        0.0176399f,
                        -0.0427706f,
                        0.9421031f,
                        0.0f);
}

Transform ColorSpaceManager::get_xyz_to_acescg()
{
  return transform_inverse(make_transform(0.652238f,
                                          0.128237f,
                                          0.169983f,
                                          0.0f,
                                          0.267672f,
                                          0.674340f,
                                          0.057988f,
                                          0.0f,
                                          -0.005382f,
                                          0.001369f,
                                          1.093071f,
                                          0.0f));
}

Transform ColorSpaceManager::get_xyz_to_scene_linear_rgb()
{
  Transform xyz_to_rgb = get_xyz_to_rec709();

#ifdef WITH_OCIO
  /* Get from OpenColorO config if it has the required roles. */
  OCIO::ConstConfigRcPtr config = nullptr;
  try {
    config = OCIO::GetCurrentConfig();
  }
  catch (OCIO::Exception &exception) {
    LOG_WARNING << "OCIO config error: " << exception.what();
    return xyz_to_rgb;
  }

  if (!(config && config->hasRole("scene_linear"))) {
    return xyz_to_rgb;
  }

  if (config->hasRole("aces_interchange")) {
    /* Standard OpenColorIO role, defined as ACES AP0 (ACES2065-1). */
    Transform aces_to_rgb;
    if (!to_scene_linear_transform(config, "aces_interchange", aces_to_rgb)) {
      return xyz_to_rgb;
    }

    /* This is the OpenColorIO builtin transform:
     * UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD. */
    const Transform ACES_AP0_to_xyz_D65 = make_transform(0.938280f,
                                                         -0.004451f,
                                                         0.016628f,
                                                         0.000000f,
                                                         0.337369f,
                                                         0.729522f,
                                                         -0.066890f,
                                                         0.000000f,
                                                         0.001174f,
                                                         -0.003711f,
                                                         1.091595f,
                                                         0.000000f);
    const Transform xyz_to_aces = transform_inverse(ACES_AP0_to_xyz_D65);
    xyz_to_rgb = aces_to_rgb * xyz_to_aces;
    return xyz_to_rgb;
  }

  if (config->hasRole("XYZ")) {
    /* Custom role used before the standard existed. */
    if (to_scene_linear_transform(config, "XYZ", xyz_to_rgb)) {
      return xyz_to_rgb;
    }
  }

  /* No reference role found to determine XYZ. */
#endif

  return xyz_to_rgb;
}

const std::string &ColorSpaceManager::get_xyz_to_scene_linear_rgb_string()
{
  /* NOTE: Be careful not to change existing hashes if at all possible, as this
   * will cause all texture files to be regenerated with significantly increased
   * disk usage. */
  const thread_scoped_lock cache_lock(cache_xyz_to_scene_linear_mutex);

  if (cache_xyz_to_scene_linear_hash.empty()) {
    /* TODO: Verify this results in the same hash across platforms. */
    const Transform xyz_to_rgb = get_xyz_to_scene_linear_rgb();
    cache_xyz_to_scene_linear_hash = string_printf(
        "%.4g_%.4g_%.4g_%.4g_%.4g_%.4g_%.4g_%.4g_%.4g_%.4g_%.4g_%.4g",
        xyz_to_rgb.x.x,
        xyz_to_rgb.x.y,
        xyz_to_rgb.x.z,
        xyz_to_rgb.x.w,
        xyz_to_rgb.y.x,
        xyz_to_rgb.y.y,
        xyz_to_rgb.y.z,
        xyz_to_rgb.y.w,
        xyz_to_rgb.z.x,
        xyz_to_rgb.z.y,
        xyz_to_rgb.z.z,
        xyz_to_rgb.z.w);
  }

  return cache_xyz_to_scene_linear_hash;
}

const char *ColorSpaceManager::get_scene_linear_interop_id(const bool srgb_encoded)
{
  const thread_scoped_lock cache_lock(cache_scene_linear_interop_id_mutex);

  if (!cache_scene_linear_interop_id_done) {
    const Transform xyz_to_rgb = get_xyz_to_scene_linear_rgb();

    if (transform_equal_threshold(xyz_to_rgb, get_xyz_to_rec709(), 0.0001f)) {
      cache_scene_linear_interop_id = "lin_rec709_scene";
      cache_scene_linear_srgb_interop_id = "srgb_rec709_scene";
    }
    else if (transform_equal_threshold(xyz_to_rgb, get_xyz_to_rec2020(), 0.0001f)) {
      cache_scene_linear_interop_id = "lin_rec2020_scene";
      cache_scene_linear_srgb_interop_id = "srgb_rec2020_scene";
    }
    else if (transform_equal_threshold(xyz_to_rgb, get_xyz_to_acescg(), 0.0001f)) {
      cache_scene_linear_interop_id = "lin_ap1_scene";
      cache_scene_linear_srgb_interop_id = "srgb_ap1_scene";
    }
    else {
      cache_scene_linear_interop_id = "unknown";
      cache_scene_linear_srgb_interop_id = "unknown";
    }

    cache_scene_linear_interop_id_done = true;
  }

  return (srgb_encoded) ? cache_scene_linear_srgb_interop_id : cache_scene_linear_interop_id;
}

/* Template instantiations so we don't have to inline functions. */
template void ColorSpaceManager::to_scene_linear(
    ustring, uchar *, int64_t, int64_t, int64_t, bool, bool, bool);
template void ColorSpaceManager::to_scene_linear(
    ustring, ushort *, int64_t, int64_t, int64_t, bool, bool, bool);
template void ColorSpaceManager::to_scene_linear(
    ustring, half *, int64_t, int64_t, int64_t, bool, bool, bool);
template void ColorSpaceManager::to_scene_linear(
    ustring, float *, int64_t, int64_t, int64_t, bool, bool, bool);

CCL_NAMESPACE_END
