/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 *  \ingroup imbuf
 */

#include <stdio.h>
#include <stdlib.h>
#include <webp/decode.h>
#include <webp/encode.h>

#include "BLI_fileops.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"
#include "IMB_filetype.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"

bool imb_is_a_webp(const unsigned char *buf, size_t size)
{
  if (WebPGetInfo(buf, size, NULL, NULL)) {
    return true;
  }
  return false;
}

ImBuf *imb_loadwebp(const unsigned char *mem,
                    size_t size,
                    int flags,
                    char colorspace[IM_MAX_SPACE])
{
  if (!imb_is_a_webp(mem, size)) {
    return NULL;
  }

  colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

  WebPBitstreamFeatures features;
  if (WebPGetFeatures(mem, size, &features) != VP8_STATUS_OK) {
    fprintf(stderr, "WebP: Failed to parse features\n");
    return NULL;
  }

  const int planes = features.has_alpha ? 32 : 24;
  ImBuf *ibuf = IMB_allocImBuf(features.width, features.height, planes, 0);

  if (ibuf == NULL) {
    fprintf(stderr, "WebP: Failed to allocate image memory\n");
    return NULL;
  }

  if ((flags & IB_test) == 0) {
    ibuf->ftype = IMB_FTYPE_WEBP;
    imb_addrectImBuf(ibuf);
    /* Flip the image during decoding to match Blender. */
    unsigned char *last_row = (unsigned char *)(ibuf->rect + (ibuf->y - 1) * ibuf->x);
    if (WebPDecodeRGBAInto(mem, size, last_row, (size_t)(ibuf->x) * ibuf->y * 4, -4 * ibuf->x) ==
        NULL) {
      fprintf(stderr, "WebP: Failed to decode image\n");
    }
  }

  return ibuf;
}

bool imb_savewebp(struct ImBuf *ibuf, const char *name, int UNUSED(flags))
{
  const int bytesperpixel = (ibuf->planes + 7) >> 3;
  unsigned char *encoded_data, *last_row;
  size_t encoded_data_size;

  if (bytesperpixel == 3) {
    /* We must convert the ImBuf RGBA buffer to RGB as WebP expects a RGB buffer. */
    const size_t num_pixels = ibuf->x * ibuf->y;
    const uint8_t *rgba_rect = (uint8_t *)ibuf->rect;
    uint8_t *rgb_rect = MEM_mallocN(sizeof(uint8_t) * num_pixels * 3, "webp rgb_rect");
    for (int i = 0; i < num_pixels; i++) {
      rgb_rect[i * 3 + 0] = rgba_rect[i * 4 + 0];
      rgb_rect[i * 3 + 1] = rgba_rect[i * 4 + 1];
      rgb_rect[i * 3 + 2] = rgba_rect[i * 4 + 2];
    }

    last_row = (unsigned char *)(rgb_rect + (ibuf->y - 1) * ibuf->x * 3);

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
    last_row = (unsigned char *)(ibuf->rect + (ibuf->y - 1) * ibuf->x);

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
    fprintf(stderr, "WebP: Unsupported bytes per pixel: %d for file: '%s'\n", bytesperpixel, name);
    return false;
  }

  if (encoded_data != NULL) {
    FILE *fp = BLI_fopen(name, "wb");
    if (!fp) {
      free(encoded_data);
      fprintf(stderr, "WebP: Cannot open file for writing: '%s'\n", name);
      return false;
    }
    fwrite(encoded_data, encoded_data_size, 1, fp);
    free(encoded_data);
    fclose(fp);
  }

  return true;
}
