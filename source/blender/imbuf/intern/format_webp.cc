/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/mux.h>

#include "BLI_fileops.h"
#include "BLI_mmap.h"

#include "IMB_allocimbuf.hh"
#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"image.webp"};

bool imb_is_a_webp(const uchar *mem, size_t size)
{
  if (WebPGetInfo(mem, size, nullptr, nullptr)) {
    return true;
  }
  return false;
}

ImBuf *imb_loadwebp(const uchar *mem, size_t size, int flags, ImFileColorSpace & /*r_colorspace*/)
{
  if (!imb_is_a_webp(mem, size)) {
    return nullptr;
  }

  WebPBitstreamFeatures features;
  if (WebPGetFeatures(mem, size, &features) != VP8_STATUS_OK) {
    CLOG_ERROR(&LOG, "Failed to parse features");
    return nullptr;
  }

  const int planes = features.has_alpha ? 32 : 24;
  ImBuf *ibuf = IMB_allocImBuf(features.width, features.height, planes, 0);

  if (ibuf == nullptr) {
    CLOG_ERROR(&LOG, "Failed to allocate image memory");
    return nullptr;
  }

  if ((flags & IB_test) == 0) {
    ibuf->ftype = IMB_FTYPE_WEBP;
    IMB_alloc_byte_pixels(ibuf);
    /* Flip the image during decoding to match Blender. */
    uchar *last_row = ibuf->byte_buffer.data + (4 * size_t(ibuf->y - 1) * size_t(ibuf->x));
    if (WebPDecodeRGBAInto(mem, size, last_row, size_t(ibuf->x) * ibuf->y * 4, -4 * ibuf->x) ==
        nullptr)
    {
      CLOG_ERROR(&LOG, "Failed to decode image");
    }
  }

  return ibuf;
}

ImBuf *imb_load_filepath_thumbnail_webp(const char *filepath,
                                        const int /*flags*/,
                                        const size_t max_thumb_size,
                                        ImFileColorSpace & /*r_colorspace*/,
                                        size_t *r_width,
                                        size_t *r_height)
{
  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }

  BLI_mmap_file *mmap_file = BLI_mmap_open(file);
  close(file);
  if (mmap_file == nullptr) {
    return nullptr;
  }

  const uchar *data = static_cast<const uchar *>(BLI_mmap_get_pointer(mmap_file));
  const size_t data_size = BLI_mmap_get_length(mmap_file);

  WebPDecoderConfig config;
  if (!data || !WebPInitDecoderConfig(&config) ||
      WebPGetFeatures(data, data_size, &config.input) != VP8_STATUS_OK ||
      BLI_mmap_any_io_error(mmap_file))
  {
    CLOG_ERROR(&LOG, "Invalid file");
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  /* Return full size of the image. */
  *r_width = size_t(config.input.width);
  *r_height = size_t(config.input.height);

  const float scale = float(max_thumb_size) / std::max(config.input.width, config.input.height);
  const int dest_w = std::max(int(config.input.width * scale), 1);
  const int dest_h = std::max(int(config.input.height * scale), 1);

  ImBuf *ibuf = IMB_allocImBuf(dest_w, dest_h, 32, IB_byte_data);
  if (ibuf == nullptr) {
    CLOG_ERROR(&LOG, "Failed to allocate image memory");
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  config.options.no_fancy_upsampling = 1;
  config.options.use_scaling = 1;
  config.options.scaled_width = dest_w;
  config.options.scaled_height = dest_h;
  config.options.bypass_filtering = 1;
  config.options.use_threads = 0;
  config.options.flip = 1;
  config.output.is_external_memory = 1;
  config.output.colorspace = MODE_RGBA;
  config.output.u.RGBA.rgba = ibuf->byte_buffer.data;
  config.output.u.RGBA.stride = 4 * ibuf->x;
  config.output.u.RGBA.size = size_t(config.output.u.RGBA.stride) * size_t(ibuf->y);

  if (WebPDecode(data, data_size, &config) != VP8_STATUS_OK || BLI_mmap_any_io_error(mmap_file)) {
    CLOG_ERROR(&LOG, "Failed to decode image");
    IMB_freeImBuf(ibuf);
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  /* Free the output buffer. */
  WebPFreeDecBuffer(&config.output);

  BLI_mmap_free(mmap_file);

  return ibuf;
}

bool imb_savewebp(ImBuf *ibuf, const char *filepath, int /*flags*/)
{
  const uint limit = 16383;
  if (ibuf->x > limit || ibuf->y > limit) {
    CLOG_ERROR(&LOG, "image x/y exceeds %u", limit);
    return false;
  }

  const int bytesperpixel = (ibuf->planes + 7) >> 3;
  uchar *encoded_data, *last_row;
  size_t encoded_data_size;

  if (bytesperpixel == 3) {
    /* We must convert the ImBuf RGBA buffer to RGB as WebP expects a RGB buffer. */
    const size_t num_pixels = IMB_get_pixel_count(ibuf);
    const uint8_t *rgba_rect = ibuf->byte_buffer.data;
    uint8_t *rgb_rect = MEM_malloc_arrayN<uint8_t>(num_pixels * 3, "webp rgb_rect");
    for (size_t i = 0; i < num_pixels; i++) {
      rgb_rect[i * 3 + 0] = rgba_rect[i * 4 + 0];
      rgb_rect[i * 3 + 1] = rgba_rect[i * 4 + 1];
      rgb_rect[i * 3 + 2] = rgba_rect[i * 4 + 2];
    }

    last_row = (uchar *)(rgb_rect + (size_t(ibuf->y - 1) * size_t(ibuf->x) * 3));

    if (ibuf->foptions.quality == 100.0f) {
      encoded_data_size = WebPEncodeLosslessRGB(
          last_row, ibuf->x, ibuf->y, -3 * ibuf->x, &encoded_data);
    }
    else {
      encoded_data_size = WebPEncodeRGB(
          last_row, ibuf->x, ibuf->y, -3 * ibuf->x, ibuf->foptions.quality, &encoded_data);
    }
    MEM_freeN(rgb_rect);
  }
  else if (bytesperpixel == 4) {
    last_row = ibuf->byte_buffer.data + 4 * size_t(ibuf->y - 1) * size_t(ibuf->x);

    if (ibuf->foptions.quality == 100.0f) {
      encoded_data_size = WebPEncodeLosslessRGBA(
          last_row, ibuf->x, ibuf->y, -4 * ibuf->x, &encoded_data);
    }
    else {
      encoded_data_size = WebPEncodeRGBA(
          last_row, ibuf->x, ibuf->y, -4 * ibuf->x, ibuf->foptions.quality, &encoded_data);
    }
  }
  else {
    CLOG_ERROR(&LOG, "Unsupported bytes per pixel: %d for file: '%s'", bytesperpixel, filepath);
    return false;
  }

  if (encoded_data == nullptr) {
    return false;
  }

  WebPMux *mux = WebPMuxNew();
  WebPData image_data = {encoded_data, encoded_data_size};
  WebPMuxSetImage(mux, &image_data, false /* Don't copy data */);

  /* Write ICC profile if there is one associated with the colorspace. */
  const ColorSpace *colorspace = ibuf->byte_buffer.colorspace;
  if (colorspace) {
    blender::Vector<char> icc_profile = IMB_colormanagement_space_to_icc_profile(colorspace);
    if (!icc_profile.is_empty()) {
      WebPData icc_chunk = {reinterpret_cast<const uint8_t *>(icc_profile.data()),
                            size_t(icc_profile.size())};
      WebPMuxSetChunk(mux, "ICCP", &icc_chunk, true /* copy data */);
    }
  }

  /* Assemble image and metadata. */
  WebPData output_data;
  if (WebPMuxAssemble(mux, &output_data) != WEBP_MUX_OK) {
    CLOG_ERROR(&LOG, "Error in mux assemble writing file: '%s'", filepath);
    WebPMuxDelete(mux);
    WebPFree(encoded_data);
    return false;
  }

  /* Write to file. */
  bool ok = true;
  FILE *fp = BLI_fopen(filepath, "wb");
  if (fp) {
    if (fwrite(output_data.bytes, output_data.size, 1, fp) != 1) {
      CLOG_ERROR(&LOG, "Unknown error writing file: '%s'", filepath);
      ok = false;
    }

    fclose(fp);
  }
  else {
    ok = false;
    CLOG_ERROR(&LOG, "Cannot open file for writing: '%s'", filepath);
  }

  WebPMuxDelete(mux);
  WebPFree(encoded_data);
  WebPDataClear(&output_data);

  return ok;
}
