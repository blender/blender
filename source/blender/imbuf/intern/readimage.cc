/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#ifdef _WIN32
#  include <io.h>
#  include <stddef.h>
#  include <sys/types.h>
#endif

#include "BLI_fileops.h"
#include "BLI_mmap.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include <stdlib.h>

#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"
#include "IMB_thumbs.h"
#include "imbuf.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

static void imb_handle_alpha(ImBuf *ibuf,
                             int flags,
                             char colorspace[IM_MAX_SPACE],
                             const char effective_colorspace[IM_MAX_SPACE])
{
  if (colorspace) {
    if (ibuf->byte_buffer.data != nullptr && ibuf->float_buffer.data == nullptr) {
      /* byte buffer is never internally converted to some standard space,
       * store pointer to its color space descriptor instead
       */
      ibuf->byte_buffer.colorspace = colormanage_colorspace_get_named(effective_colorspace);
    }

    BLI_strncpy(colorspace, effective_colorspace, IM_MAX_SPACE);
  }

  bool is_data = (colorspace && IMB_colormanagement_space_name_is_data(colorspace));
  int alpha_flags = (flags & IB_alphamode_detect) ? ibuf->flags : flags;

  if (is_data || (flags & IB_alphamode_channel_packed)) {
    /* Don't touch alpha. */
    ibuf->flags |= IB_alphamode_channel_packed;
  }
  else if (flags & IB_alphamode_ignore) {
    /* Make opaque. */
    IMB_rectfill_alpha(ibuf, 1.0f);
    ibuf->flags |= IB_alphamode_ignore;
  }
  else {
    if (alpha_flags & IB_alphamode_premul) {
      if (ibuf->byte_buffer.data) {
        IMB_unpremultiply_alpha(ibuf);
      }
      else {
        /* pass, floats are expected to be premul */
      }
    }
    else {
      if (ibuf->float_buffer.data) {
        IMB_premultiply_alpha(ibuf);
      }
      else {
        /* pass, bytes are expected to be straight */
      }
    }
  }

  /* OCIO_TODO: in some cases it's faster to do threaded conversion,
   *            but how to distinguish such cases */
  colormanage_imbuf_make_linear(ibuf, effective_colorspace);
}

ImBuf *IMB_ibImageFromMemory(
    const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE], const char *descr)
{
  ImBuf *ibuf;
  const ImFileType *type;
  char effective_colorspace[IM_MAX_SPACE] = "";

  if (mem == nullptr) {
    fprintf(stderr, "%s: nullptr pointer\n", __func__);
    return nullptr;
  }

  if (colorspace) {
    STRNCPY(effective_colorspace, colorspace);
  }

  for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (type->load) {
      ibuf = type->load(mem, size, flags, effective_colorspace);
      if (ibuf) {
        imb_handle_alpha(ibuf, flags, colorspace, effective_colorspace);
        return ibuf;
      }
    }
  }

  if ((flags & IB_test) == 0) {
    fprintf(stderr, "%s: unknown file-format (%s)\n", __func__, descr);
  }

  return nullptr;
}

ImBuf *IMB_loadifffile(int file, int flags, char colorspace[IM_MAX_SPACE], const char *descr)
{
  ImBuf *ibuf;
  uchar *mem;
  size_t size;

  if (file == -1) {
    return nullptr;
  }

  size = BLI_file_descriptor_size(file);

  imb_mmap_lock();
  BLI_mmap_file *mmap_file = BLI_mmap_open(file);
  imb_mmap_unlock();
  if (mmap_file == nullptr) {
    fprintf(stderr, "%s: couldn't get mapping %s\n", __func__, descr);
    return nullptr;
  }

  mem = static_cast<uchar *>(BLI_mmap_get_pointer(mmap_file));

  ibuf = IMB_ibImageFromMemory(mem, size, flags, colorspace, descr);

  imb_mmap_lock();
  BLI_mmap_free(mmap_file);
  imb_mmap_unlock();

  return ibuf;
}

ImBuf *IMB_loadiffname(const char *filepath, int flags, char colorspace[IM_MAX_SPACE])
{
  ImBuf *ibuf;
  int file;

  BLI_assert(!BLI_path_is_rel(filepath));

  file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }

  ibuf = IMB_loadifffile(file, flags, colorspace, filepath);

  if (ibuf) {
    STRNCPY(ibuf->filepath, filepath);
  }

  close(file);

  return ibuf;
}

ImBuf *IMB_thumb_load_image(const char *filepath,
                            size_t max_thumb_size,
                            char colorspace[IM_MAX_SPACE])
{
  const ImFileType *type = IMB_file_type_from_ftype(IMB_ispic_type(filepath));
  if (type == nullptr) {
    return nullptr;
  }

  ImBuf *ibuf = nullptr;
  int flags = IB_rect | IB_metadata;
  /* Size of the original image. */
  size_t width = 0;
  size_t height = 0;

  char effective_colorspace[IM_MAX_SPACE] = "";
  if (colorspace) {
    STRNCPY(effective_colorspace, colorspace);
  }

  if (type->load_filepath_thumbnail) {
    ibuf = type->load_filepath_thumbnail(
        filepath, flags, max_thumb_size, colorspace, &width, &height);
  }
  else {
    /* Skip images of other types if over 100MB. */
    const size_t file_size = BLI_file_size(filepath);
    if (file_size != size_t(-1) && file_size > THUMB_SIZE_MAX) {
      return nullptr;
    }
    ibuf = IMB_loadiffname(filepath, flags, colorspace);
    if (ibuf) {
      width = ibuf->x;
      height = ibuf->y;
    }
  }

  if (ibuf) {
    imb_handle_alpha(ibuf, flags, colorspace, effective_colorspace);

    if (width > 0 && height > 0) {
      /* Save dimensions of original image into the thumbnail metadata. */
      char cwidth[40];
      char cheight[40];
      SNPRINTF(cwidth, "%zu", width);
      SNPRINTF(cheight, "%zu", height);
      IMB_metadata_ensure(&ibuf->metadata);
      IMB_metadata_set_field(ibuf->metadata, "Thumb::Image::Width", cwidth);
      IMB_metadata_set_field(ibuf->metadata, "Thumb::Image::Height", cheight);
    }
  }

  return ibuf;
}

ImBuf *IMB_testiffname(const char *filepath, int flags)
{
  ImBuf *ibuf;
  int file;
  char colorspace[IM_MAX_SPACE] = "\0";

  BLI_assert(!BLI_path_is_rel(filepath));

  file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }

  ibuf = IMB_loadifffile(file, flags | IB_test | IB_multilayer, colorspace, filepath);

  if (ibuf) {
    STRNCPY(ibuf->filepath, filepath);
  }

  close(file);

  return ibuf;
}
