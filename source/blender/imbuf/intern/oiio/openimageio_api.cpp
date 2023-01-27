/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup openimageio
 */

#include <set>

#if defined(WIN32)
#  include "utfconv.h"
#  define _USE_MATH_DEFINES
#endif

/* NOTE: Keep first, #BLI_path_util conflicts with OIIO's format. */
#include "openimageio_api.h"
#include <OpenImageIO/imageio.h>
#include <memory>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "IMB_allocimbuf.h"
#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

OIIO_NAMESPACE_USING

using std::string;
using std::unique_ptr;

using uchar = uchar;

template<class T, class Q>
static void fill_all_channels(T *pixels, int width, int height, int components, Q alpha)
{
  if (components == 2) {
    for (int i = width * height - 1; i >= 0; i--) {
      pixels[i * 4 + 3] = pixels[i * 2 + 1];
      pixels[i * 4 + 2] = pixels[i * 2 + 0];
      pixels[i * 4 + 1] = pixels[i * 2 + 0];
      pixels[i * 4 + 0] = pixels[i * 2 + 0];
    }
  }
  else if (components == 3) {
    for (int i = width * height - 1; i >= 0; i--) {
      pixels[i * 4 + 3] = alpha;
      pixels[i * 4 + 2] = pixels[i * 3 + 2];
      pixels[i * 4 + 1] = pixels[i * 3 + 1];
      pixels[i * 4 + 0] = pixels[i * 3 + 0];
    }
  }
  else if (components == 1) {
    for (int i = width * height - 1; i >= 0; i--) {
      pixels[i * 4 + 3] = alpha;
      pixels[i * 4 + 2] = pixels[i];
      pixels[i * 4 + 1] = pixels[i];
      pixels[i * 4 + 0] = pixels[i];
    }
  }
}

static ImBuf *imb_oiio_load_image(
    ImageInput *in, int width, int height, int components, int flags, bool is_alpha)
{
  ImBuf *ibuf;
  int scanlinesize = width * components * sizeof(uchar);

  /* allocate the memory for the image */
  ibuf = IMB_allocImBuf(width, height, is_alpha ? 32 : 24, flags | IB_rect);

  try {
    if (!in->read_image(0,
                        0,
                        0,
                        components,
                        TypeDesc::UINT8,
                        (uchar *)ibuf->rect + (height - 1) * scanlinesize,
                        AutoStride,
                        -scanlinesize,
                        AutoStride)) {
      std::cerr << __func__ << ": ImageInput::read_image() failed:" << std::endl
                << in->geterror() << std::endl;

      if (ibuf) {
        IMB_freeImBuf(ibuf);
      }

      return nullptr;
    }
  }
  catch (const std::exception &exc) {
    std::cerr << exc.what() << std::endl;
    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }

    return nullptr;
  }

  /* ImBuf always needs 4 channels */
  fill_all_channels((uchar *)ibuf->rect, width, height, components, 0xFF);

  return ibuf;
}

static ImBuf *imb_oiio_load_image_float(
    ImageInput *in, int width, int height, int components, int flags, bool is_alpha)
{
  ImBuf *ibuf;
  int scanlinesize = width * components * sizeof(float);

  /* allocate the memory for the image */
  ibuf = IMB_allocImBuf(width, height, is_alpha ? 32 : 24, flags | IB_rectfloat);

  try {
    if (!in->read_image(0,
                        0,
                        0,
                        components,
                        TypeDesc::FLOAT,
                        (uchar *)ibuf->rect_float + (height - 1) * scanlinesize,
                        AutoStride,
                        -scanlinesize,
                        AutoStride)) {
      std::cerr << __func__ << ": ImageInput::read_image() failed:" << std::endl
                << in->geterror() << std::endl;

      if (ibuf) {
        IMB_freeImBuf(ibuf);
      }

      return nullptr;
    }
  }
  catch (const std::exception &exc) {
    std::cerr << exc.what() << std::endl;
    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }

    return nullptr;
  }

  /* ImBuf always needs 4 channels */
  fill_all_channels((float *)ibuf->rect_float, width, height, components, 1.0f);

  /* NOTE: Photoshop 16 bit files never has alpha with it,
   * so no need to handle associated/unassociated alpha. */
  return ibuf;
}

extern "C" {

bool imb_is_a_photoshop(const uchar *mem, size_t size)
{
  const uchar magic[4] = {'8', 'B', 'P', 'S'};
  if (size < sizeof(magic)) {
    return false;
  }
  return memcmp(magic, mem, sizeof(magic)) == 0;
}

int imb_save_photoshop(struct ImBuf *ibuf, const char * /*name*/, int flags)
{
  if (flags & IB_mem) {
    std::cerr << __func__ << ": Photoshop PSD-save: Create PSD in memory"
              << " currently not supported" << std::endl;
    imb_addencodedbufferImBuf(ibuf);
    ibuf->encodedsize = 0;
    return 0;
  }

  return 0;
}

struct ImBuf *imb_load_photoshop(const char *filename, int flags, char colorspace[IM_MAX_SPACE])
{
  struct ImBuf *ibuf = nullptr;
  int width, height, components;
  bool is_float, is_alpha, is_half;
  int basesize;
  char file_colorspace[IM_MAX_SPACE];
  const bool is_colorspace_manually_set = (colorspace[0] != '\0');

  /* load image from file through OIIO */
  if (IMB_ispic_type_matches(filename, IMB_FTYPE_PSD) == 0) {
    return nullptr;
  }

  colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

  unique_ptr<ImageInput> in(ImageInput::create(filename));
  if (!in) {
    std::cerr << __func__ << ": ImageInput::create() failed:" << std::endl
              << OIIO_NAMESPACE::geterror() << std::endl;
    return nullptr;
  }

  ImageSpec spec, config;
  config.attribute("oiio:UnassociatedAlpha", int(1));

  if (!in->open(filename, spec, config)) {
    std::cerr << __func__ << ": ImageInput::open() failed:" << std::endl
              << in->geterror() << std::endl;
    return nullptr;
  }

  if (!is_colorspace_manually_set) {
    string ics = spec.get_string_attribute("oiio:ColorSpace");
    BLI_strncpy(file_colorspace, ics.c_str(), IM_MAX_SPACE);

    /* Only use color-spaces exist. */
    if (colormanage_colorspace_get_named(file_colorspace)) {
      strcpy(colorspace, file_colorspace);
    }
    else {
      std::cerr << __func__ << ": The embed colorspace (\"" << file_colorspace
                << "\") not supported in existent OCIO configuration file. Fallback "
                << "to system default colorspace (\"" << colorspace << "\")." << std::endl;
    }
  }

  width = spec.width;
  height = spec.height;
  components = spec.nchannels;
  is_alpha = spec.alpha_channel != -1;
  basesize = spec.format.basesize();
  is_float = basesize > 1;
  is_half = spec.format == TypeDesc::HALF;

  /* we only handle certain number of components */
  if (!(components >= 1 && components <= 4)) {
    if (in) {
      in->close();
    }
    return nullptr;
  }

  if (is_float) {
    ibuf = imb_oiio_load_image_float(in.get(), width, height, components, flags, is_alpha);
  }
  else {
    ibuf = imb_oiio_load_image(in.get(), width, height, components, flags, is_alpha);
  }

  if (in) {
    in->close();
  }

  if (!ibuf) {
    return nullptr;
  }

  /* ImBuf always needs 4 channels */
  ibuf->ftype = IMB_FTYPE_PSD;
  ibuf->channels = 4;
  ibuf->planes = (3 + (is_alpha ? 1 : 0)) * 4 << basesize;
  ibuf->flags |= (is_float && is_half) ? IB_halffloat : 0;

  try {
    return ibuf;
  }
  catch (const std::exception &exc) {
    std::cerr << exc.what() << std::endl;
    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }

    return nullptr;
  }
}

int OIIO_getVersionHex(void)
{
  return openimageio_version();
}

} /* export "C" */
