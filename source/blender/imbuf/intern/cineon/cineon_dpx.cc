/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbcineon
 */

#include "logImageCore.h"
#include <cstdio>
#include <cstring>

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_global.hh"

#include "MEM_guardedalloc.h"

namespace blender {

static ImBuf *imb_load_dpx_cineon(const uchar *mem,
                                  size_t size,
                                  ImBufFlags flags,
                                  ImFileColorSpace &r_colorspace)
{
  ImBuf *ibuf;
  LogImageFile *image;
  int width, height, depth;

  logImageSetVerbose((G.debug & G_DEBUG) ? 1 : 0);

  image = logImageOpenFromMemory(mem, size);

  if (image == nullptr) {
    printf("Cineon: error opening image.\n");
    return nullptr;
  }

  logImageGetSize(image, &width, &height, &depth);

  ibuf = IMB_allocImBuf(width, height, ImBufFlags::FloatData | flags);
  if (ibuf == nullptr) {
    logImageClose(image);
    return nullptr;
  }

  if (!flag_is_set(flags, ImBufFlags::Test)) {
    if (logImageGetDataRGBA(image, ibuf->float_data_for_write(), 1) != 0) {
      logImageClose(image);
      IMB_freeImBuf(ibuf);
      return nullptr;
    }
    IMB_flipy(ibuf);
  }

  logImageClose(image);
  ibuf->ftype = IMB_FTYPE_CINEON;

  if (flag_is_set(flags, ImBufFlags::AlphaDetect)) {
    ibuf->flags |= ImBufFlags::AlphaPremul;
  }

  r_colorspace.is_hdr_float = true;

  return ibuf;
}

static int imb_save_dpx_cineon(ImBuf *ibuf, const char *filepath)
{
  LogImageFile *logImage;
  float *fbuf;
  float *fbuf_ptr;
  const uchar *rect_ptr;
  int x, y, rvalue;

  logImageSetVerbose((G.debug & G_DEBUG) ? 1 : 0);

  if (!ELEM(ibuf->color_mode, ImColorMode::RGB, ImColorMode::RGBA)) {
    printf("Cineon: only RGB/RGBA is supported, file: '%s'\n", filepath);
    return 0;
  }

  const bool has_alpha = ibuf->color_mode == ImColorMode::RGBA;
  logImage = logImageCreate(filepath, ibuf->x, ibuf->y, "Blender");

  if (logImage == nullptr) {
    printf("Cineon: error creating file.\n");
    return 0;
  }

  if (ibuf->float_data() != nullptr) {
    fbuf = MEM_new_array_uninitialized<float>(4 * size_t(ibuf->x) * size_t(ibuf->y),
                                              "fbuf in imb_save_dpx_cineon");

    for (y = 0; y < ibuf->y; y++) {
      float *dst_ptr = fbuf + (4 * (size_t(ibuf->y - y - 1) * size_t(ibuf->x)));
      const float *src_ptr = ibuf->float_data() + (4 * (size_t(y) * size_t(ibuf->x)));

      memcpy(dst_ptr, src_ptr, 4 * ibuf->x * sizeof(float));
    }

    rvalue = (logImageSetDataRGBA(logImage, fbuf, 1) == 0);

    MEM_delete(fbuf);
  }
  else {
    if (ibuf->byte_data()) {
      IMB_byte_from_float(ibuf);
    }

    fbuf = MEM_new_array_uninitialized<float>(4 * size_t(ibuf->x) * size_t(ibuf->y),
                                              "fbuf in imb_save_dpx_cineon");
    if (fbuf == nullptr) {
      printf("Cineon: error allocating memory.\n");
      logImageClose(logImage);
      return 0;
    }
    const uint8_t *byte_data = ibuf->byte_data();
    for (y = 0; y < ibuf->y; y++) {
      fbuf_ptr = fbuf + (4 * (size_t(ibuf->y - y - 1) * size_t(ibuf->x)));
      rect_ptr = byte_data + (4 * (size_t(y) * size_t(ibuf->x)));
      for (x = 0; x < ibuf->x; x++) {
        fbuf_ptr[0] = float(rect_ptr[0]) / 255.0f;
        fbuf_ptr[1] = float(rect_ptr[1]) / 255.0f;
        fbuf_ptr[2] = float(rect_ptr[2]) / 255.0f;
        fbuf_ptr[3] = has_alpha ? (float(rect_ptr[3]) / 255.0f) : 1.0f;
        fbuf_ptr += 4;
        rect_ptr += 4;
      }
    }
    rvalue = (logImageSetDataRGBA(logImage, fbuf, 0) == 0);
    MEM_delete(fbuf);
  }

  logImageClose(logImage);
  return rvalue;
}

bool imb_save_cineon(ImBuf *buf, const char *filepath, ImBufFlags /*flags*/)
{
  return imb_save_dpx_cineon(buf, filepath);
}

bool imb_is_a_cineon(const uchar *mem, size_t size)
{
  return logImageIsCineon(mem, size);
}

ImBuf *imb_load_cineon(const uchar *mem,
                       size_t size,
                       ImBufFlags flags,
                       ImFileColorSpace &r_colorspace)
{
  if (!imb_is_a_cineon(mem, size)) {
    return nullptr;
  }
  return imb_load_dpx_cineon(mem, size, flags, r_colorspace);
}

}  // namespace blender
