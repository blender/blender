/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_oiio.h"

#include "util/image.h"
#include "util/image_maketx.h"
#include "util/image_metadata.h"
#include "util/path.h"
#include "util/progress.h"
#include "util/string.h"
#include "util/types_base.h"
#include "util/types_image.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

/* File handle cache limiter. This is shared across multiple Cycles instances as
 * the limit we risk running into is per process.
 *
 * Increasing this limit to be closer to system_max_open_files() would improve
 * performance, and on Windows the system limit is adjustable. However this would
 * need careful testing as other parts of the software uses file handles too. */
static CacheLimiter<ImageInput> cache_limiter_image_input(128);

OIIOImageLoader::OIIOImageLoader(const string &filepath) : original_filepath(filepath) {}

OIIOImageLoader::~OIIOImageLoader() = default;

bool OIIOImageLoader::load_metadata(ImageMetaData &metadata,
                                    const ImageLoaderParams &params,
                                    Progress &progress)
{
  if (params.use_texture_cache) {
    const std::string &filepath = get_filepath();
    const bool found = resolve_tx(filepath,
                                  params.texture_cache_path,
                                  params.colorspace,
                                  params.alpha_type,
                                  IMAGE_FORMAT_PLAIN,
                                  texture_cache_filepath,
                                  metadata);

    if (found) {
      return true;
    }

    if (params.auto_texture_cache) {
      progress.set_status("Generating tx cache | " + path_filename(filepath), "");

      if (!make_tx(filepath,
                   texture_cache_filepath,
                   params.colorspace,
                   params.alpha_type,
                   IMAGE_FORMAT_PLAIN))
      {
        texture_cache_filepath.clear();
      }
    }
    else {
      texture_cache_filepath.clear();
    }
  }

  return metadata.oiio_load_metadata(get_filepath());
}

bool OIIOImageLoader::load_pixels(const ImageMetaData &metadata, void *pixels)
{
  if (!metadata.oiio_load_pixels(get_filepath(), pixels)) {
    return false;
  }

  metadata.conform_pixels(pixels);
  return true;
}

template<typename StorageType>
static bool oiio_load_pixels_tile(const unique_ptr<ImageInput> &in,
                                  const ImageMetaData &metadata,
                                  const int miplevel,
                                  const int64_t x,
                                  const int64_t y,
                                  const int64_t w,
                                  const int64_t h,
                                  const OIIO::TypeDesc typedesc,
                                  const int64_t x_stride,
                                  const int64_t y_stride,
                                  StorageType *pixels)

{
  const int channels = metadata.channels;
  int64_t read_y_stride = y_stride;

  /* Read pixels through OpenImageIO. */
  StorageType *readpixels = pixels;
  vector<StorageType> tmppixels;
  if (channels > 4) {
    tmppixels.resize(w * h * channels);
    readpixels = &tmppixels[0];
    read_y_stride = w * channels * sizeof(StorageType);
  }

  if (!in->read_tiles(0,
                      miplevel,
                      x,
                      x + w,
                      y,
                      y + h,
                      0,
                      1,
                      0,
                      channels,
                      typedesc,
                      readpixels,
                      x_stride,
                      read_y_stride))
  {
    return false;
  }

  if (channels > 4) {
    for (int64_t j = 0; j < h; j++) {
      const StorageType *in_pixels = tmppixels.data() + j * w * channels;
      StorageType *out_pixels = pixels + j * (y_stride / sizeof(StorageType));
      for (int64_t i = 0; i < w; i++) {
        out_pixels[i * 4 + 3] = in_pixels[i * channels + 3];
        out_pixels[i * 4 + 2] = in_pixels[i * channels + 2];
        out_pixels[i * 4 + 1] = in_pixels[i * channels + 1];
        out_pixels[i * 4 + 0] = in_pixels[i * channels + 0];
      }
    }
    tmppixels.clear();
  }

  /* Can skip conform for speed if it's a Blender native tx file. */
  if (metadata.tile_need_conform) {
    metadata.conform_pixels(pixels,
                            w,
                            h,
                            x_stride / sizeof(StorageType),
                            y_stride / sizeof(StorageType),
                            y_stride / sizeof(StorageType));
  }

  return true;
}

static bool oiio_load_pixels_tile(const unique_ptr<ImageInput> &in,
                                  const ImageMetaData &metadata,
                                  const int64_t /*height*/,
                                  const int miplevel,
                                  const int64_t x,
                                  const int64_t y,
                                  const int64_t w,
                                  const int64_t h,
                                  const int64_t x_stride,
                                  const int64_t y_stride,
                                  uint8_t *pixels)
{
  switch (metadata.type) {
    case IMAGE_DATA_TYPE_BYTE:
    case IMAGE_DATA_TYPE_BYTE4:
      return oiio_load_pixels_tile<uint8_t>(
          in, metadata, miplevel, x, y, w, h, TypeDesc::UINT8, x_stride, y_stride, pixels);
    case IMAGE_DATA_TYPE_USHORT:
    case IMAGE_DATA_TYPE_USHORT4:
      return oiio_load_pixels_tile<uint16_t>(in,
                                             metadata,
                                             miplevel,
                                             x,
                                             y,
                                             w,
                                             h,
                                             TypeDesc::USHORT,
                                             x_stride,
                                             y_stride,
                                             reinterpret_cast<uint16_t *>(pixels));
      break;
    case IMAGE_DATA_TYPE_HALF:
    case IMAGE_DATA_TYPE_HALF4:
      return oiio_load_pixels_tile<half>(in,
                                         metadata,
                                         miplevel,
                                         x,
                                         y,
                                         w,
                                         h,
                                         TypeDesc::HALF,
                                         x_stride,
                                         y_stride,
                                         reinterpret_cast<half *>(pixels));
    case IMAGE_DATA_TYPE_FLOAT:
    case IMAGE_DATA_TYPE_FLOAT4:
      return oiio_load_pixels_tile<float>(in,
                                          metadata,
                                          miplevel,
                                          x,
                                          y,
                                          w,
                                          h,
                                          TypeDesc::FLOAT,
                                          x_stride,
                                          y_stride,
                                          reinterpret_cast<float *>(pixels));
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
    case IMAGE_DATA_TYPE_NANOVDB_EMPTY:
    case IMAGE_DATA_NUM_TYPES:
      return false;
  }

  return false;
}

static bool oiio_load_pixels_tile_adjacent(const unique_ptr<ImageInput> &in,
                                           const ImageMetaData &metadata,
                                           const int64_t width,
                                           const int64_t height,
                                           const int miplevel,
                                           const int64_t x,
                                           const int64_t y,
                                           const int64_t w,
                                           const int64_t h,
                                           const int64_t x_stride,
                                           const int64_t y_stride,
                                           const int x_adjacent,
                                           const int y_adjacent,
                                           const int64_t padding,
                                           const ExtensionType extension,
                                           uint8_t *pixels)
{
  const int64_t tile_size = metadata.tile_size;

  int64_t x_new = x + x_adjacent * tile_size;
  int64_t y_new = y + y_adjacent * tile_size;

  const bool in_range = x_new >= 0 && x_new < width && y_new >= 0 && y_new < height;

  const int64_t pad_x = (x_adjacent < 0) ? 0 : (x_adjacent == 0) ? padding : padding + w;
  const int64_t pad_y = (y_adjacent < 0) ? 0 : (y_adjacent == 0) ? padding : padding + h;
  const int64_t pad_w = (x_adjacent == 0) ? w : padding;
  const int64_t pad_h = (y_adjacent == 0) ? h : padding;

  if (!in_range) {
    /* Adjacent tile does not exist, fill in padding depending on extension mode. */
    if (extension == EXTENSION_EXTEND) {
      /* Duplicate pixels from border of tile. */
      for (int64_t j = 0; j < pad_h; j++) {
        for (int64_t i = 0; i < pad_w; i++) {
          const int64_t source_x = (x_adjacent < 0) ? 0 : (x_adjacent == 0) ? i : w - 1;
          const int64_t source_y = (y_adjacent < 0) ? 0 : (y_adjacent == 0) ? j : h - 1;

          std::copy_n(pixels + (padding + source_x) * x_stride + (padding + source_y) * y_stride,
                      x_stride,
                      pixels + (pad_x + i) * x_stride + (pad_y + j) * y_stride);
        }
      }
      return true;
    }
    if (extension == EXTENSION_MIRROR) {
      /* Mirror pixels from border of tile. */
      for (int64_t j = 0; j < pad_h; j++) {
        for (int64_t i = 0; i < pad_w; i++) {
          const int64_t source_x = (x_adjacent < 0)  ? padding - 1 - i :
                                   (x_adjacent == 0) ? i :
                                                       w - 1 - i;
          const int64_t source_y = (y_adjacent < 0)  ? padding - 1 - j :
                                   (y_adjacent == 0) ? j :
                                                       h - 1 - j;

          std::copy_n(pixels + (padding + source_x) * x_stride + (padding + source_y) * y_stride,
                      x_stride,
                      pixels + (pad_x + i) * x_stride + (pad_y + j) * y_stride);
        }
      }
      return true;
    }
    if (extension == EXTENSION_CLIP) {
      /* Fill with zeros. */
      for (int64_t j = 0; j < pad_h; j++) {
        std::fill_n(pixels + pad_x * x_stride + (pad_y + j) * y_stride, x_stride * pad_w, 0);
      }
      return true;
    }
    if (extension == EXTENSION_REPEAT) {
      /* Wrap around for repeat mode. */
      if (x_new < 0) {
        x_new = (divide_up(width, tile_size) - 1) * tile_size;
      }
      else if (x_new >= width) {
        x_new = 0;
      }

      if (y_new < 0) {
        y_new = (divide_up(height, tile_size) - 1) * tile_size;
      }
      else if (y_new >= height) {
        y_new = 0;
      }
    }
  }

  /* Load adjacent tiles. */
  vector<uint8_t> tile_pixels(tile_size * tile_size * x_stride, 0);
  if (!oiio_load_pixels_tile(in,
                             metadata,
                             height,
                             miplevel,
                             x_new,
                             y_new,
                             tile_size,
                             tile_size,
                             x_stride,
                             tile_size * x_stride,
                             tile_pixels.data()))
  {
    return false;
  }

  /* Copy pixels from adjacent tiles. Use the actual size of the loaded tile
   * (clamped to image bounds) rather than full image width/height, since the
   * adjacent tile may be at the edge and smaller than tile_size. */
  const int64_t adj_tile_w = std::min(width - x_new, tile_size);
  const int64_t adj_tile_h = std::min(height - y_new, tile_size);
  const int64_t tile_x = (x_adjacent < 0) ? adj_tile_w - padding : 0;
  const int64_t tile_y = (y_adjacent < 0) ? adj_tile_h - padding : 0;

  for (int64_t j = 0; j < pad_h; j++) {
    std::copy_n(tile_pixels.data() + tile_x * x_stride + (tile_y + j) * (tile_size * x_stride),
                x_stride * pad_w,
                pixels + pad_x * x_stride + (pad_y + j) * y_stride);
  }

  return true;
}

bool OIIOImageLoader::load_pixels_tile(const ImageMetaData &metadata,
                                       const int miplevel,
                                       const int64_t x,
                                       const int64_t y,
                                       const int64_t w,
                                       const int64_t h,
                                       const int64_t x_stride,
                                       const int64_t y_stride,
                                       const int64_t padding,
                                       const ExtensionType extension,
                                       uint8_t *pixels)
{
  assert(metadata.tile_size != 0);

  /* Don't keep retrying files that failed to open once. */
  if (texture_cache_file_handle_failed) {
    return false;
  }

  /* Create cached file handle if it was not created yet or it was deleted by the cache limiter. */
  CacheHandleGuard<ImageInput> file_handle_user = texture_cache_file_handle.acquire(
      cache_limiter_image_input, [&]() {
        const string &filepath = get_filepath();
        unique_ptr<ImageInput> in = unique_ptr<ImageInput>(ImageInput::create(filepath));
        if (!in) {
          texture_cache_file_handle_failed = true;
          return std::unique_ptr<ImageInput>();
        }

        ImageSpec spec = ImageSpec();
        ImageSpec config;
        config.attribute("oiio:UnassociatedAlpha", 1);

        if (!in->open(filepath, spec, config)) {
          texture_cache_file_handle_failed = true;
          return std::unique_ptr<ImageInput>();
        }

        return in;
      });

  if (texture_cache_file_handle_failed) {
    return false;
  }

  const int64_t width = metadata.width >> miplevel;
  const int64_t height = metadata.height >> miplevel;

  /* Load center pixels. */
  bool ok = oiio_load_pixels_tile(file_handle_user.get(),
                                  metadata,
                                  height,
                                  miplevel,
                                  x,
                                  y,
                                  w,
                                  h,
                                  x_stride,
                                  y_stride,
                                  pixels + padding * x_stride + padding * y_stride);

  /* Pad tile borders from adjacent tiles. */
  if (padding > 0) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0) {
          continue;
        }
        ok &= oiio_load_pixels_tile_adjacent(file_handle_user.get(),
                                             metadata,
                                             width,
                                             height,
                                             miplevel,
                                             x,
                                             y,
                                             w,
                                             h,
                                             x_stride,
                                             y_stride,
                                             i,
                                             j,
                                             padding,
                                             extension,
                                             pixels);
      }
    }
  }

  return ok;
}

string OIIOImageLoader::name() const
{
  return path_filename(get_filepath());
}

const string &OIIOImageLoader::get_filepath() const
{
  return (texture_cache_filepath.empty()) ? original_filepath : texture_cache_filepath;
}

bool OIIOImageLoader::equals(const ImageLoader &other) const
{
  const OIIOImageLoader &other_loader = (const OIIOImageLoader &)other;
  return original_filepath == other_loader.original_filepath;
}

CCL_NAMESPACE_END
