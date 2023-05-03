/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbdds
 */

#include "BLI_utildefines.h"

#include <DirectDrawSurface.h>
#include <FlipDXT.h>
#include <Stream.h>
#include <cstddef>
#include <cstdio> /* printf */
#include <dds_api.h>
#include <fstream>

#if defined(WIN32)
#  include "utfconv.h"
#endif

#include "IMB_allocimbuf.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "imbuf.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

extern "C" {

bool imb_save_dds(struct ImBuf *ibuf, const char *filepath, int /*flags*/)
{
  return false; /* TODO: finish this function. */

  /* check image buffer */
  if (ibuf == nullptr) {
    return false;
  }
  if (ibuf->rect == nullptr) {
    return false;
  }

  /* open file for writing */
  std::ofstream fildes;

#if defined(WIN32)
  wchar_t *wname = alloc_utf16_from_8(filepath, 0);
  fildes.open(wname);
  free(wname);
#else
  fildes.open(filepath);
#endif

  /* write header */
  fildes << "DDS ";
  fildes.close();

  return true;
}

bool imb_is_a_dds(const uchar *mem, const size_t size)
{
  if (size < 8) {
    return false;
  }
  /* heuristic check to see if mem contains a DDS file */
  /* header.fourcc == FOURCC_DDS */
  if ((mem[0] != 'D') || (mem[1] != 'D') || (mem[2] != 'S') || (mem[3] != ' ')) {
    return false;
  }
  /* header.size == 124 */
  if ((mem[4] != 124) || mem[5] || mem[6] || mem[7]) {
    return false;
  }
  return true;
}

struct ImBuf *imb_load_dds(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  struct ImBuf *ibuf = nullptr;
  DirectDrawSurface dds((uchar *)mem, size); /* reads header */
  uchar bits_per_pixel;
  uint *rect;
  Image img;
  uint numpixels = 0;
  int col;
  uchar *cp = (uchar *)&col;
  Color32 pixel;
  Color32 *pixels = nullptr;

  /* OCIO_TODO: never was able to save DDS, so can't test loading
   *            but profile used to be set to sRGB and can't see rect_float here, so
   *            default byte space should work fine
   */
  colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

  if (!imb_is_a_dds(mem, size)) {
    return nullptr;
  }

  /* check if DDS is valid and supported */
  if (!dds.isValid()) {
    /* no need to print error here, just testing if it is a DDS */
    if (flags & IB_test) {
      return nullptr;
    }

    printf("DDS: not valid; header follows\n");
    dds.printInfo();
    return nullptr;
  }
  if (!dds.isSupported()) {
    printf("DDS: format not supported\n");
    return nullptr;
  }
  if ((dds.width() > 65535) || (dds.height() > 65535)) {
    printf("DDS: dimensions too large\n");
    return nullptr;
  }

  /* convert DDS into ImBuf */
  dds.mipmap(&img, 0, 0); /* load first face, first mipmap */
  pixels = img.pixels();
  numpixels = dds.width() * dds.height();
  bits_per_pixel = 24;
  if (img.format() == Image::Format_ARGB) {
    /* check that there is effectively an alpha channel */
    for (uint i = 0; i < numpixels; i++) {
      pixel = pixels[i];
      if (pixel.a != 255) {
        bits_per_pixel = 32;
        break;
      }
    }
  }
  ibuf = IMB_allocImBuf(dds.width(), dds.height(), bits_per_pixel, 0);
  if (ibuf == nullptr) {
    return nullptr; /* memory allocation failed */
  }

  ibuf->ftype = IMB_FTYPE_DDS;
  ibuf->dds_data.fourcc = dds.fourCC();
  ibuf->dds_data.nummipmaps = dds.mipmapCount();

  if ((flags & IB_test) == 0) {
    if (!imb_addrectImBuf(ibuf)) {
      return ibuf;
    }
    if (ibuf->rect == nullptr) {
      return ibuf;
    }

    rect = ibuf->rect;
    cp[3] = 0xff; /* default alpha if alpha channel is not present */

    for (uint i = 0; i < numpixels; i++) {
      pixel = pixels[i];
      cp[0] = pixel.r; /* set R component of col */
      cp[1] = pixel.g; /* set G component of col */
      cp[2] = pixel.b; /* set B component of col */
      if (dds.hasAlpha()) {
        cp[3] = pixel.a; /* set A component of col */
      }
      rect[i] = col;
    }

    if (ibuf->dds_data.fourcc != FOURCC_DDS) {
      ibuf->dds_data.data = (uchar *)dds.readData(ibuf->dds_data.size);

      /* flip compressed texture */
      if (ibuf->dds_data.data) {
        FlipDXTCImage(dds.width(),
                      dds.height(),
                      ibuf->dds_data.nummipmaps,
                      dds.fourCC(),
                      ibuf->dds_data.data,
                      ibuf->dds_data.size,
                      &ibuf->dds_data.nummipmaps);
      }
    }
    else {
      ibuf->dds_data.data = nullptr;
      ibuf->dds_data.size = 0;
    }

    /* flip uncompressed texture */
    IMB_flipy(ibuf);
  }

  return ibuf;
}

} /* extern "C" */
