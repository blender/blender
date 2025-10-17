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

#include <cstdlib>

#include "BLI_fileops.h"
#include "BLI_mmap.h"
#include "BLI_path_utils.hh" /* For assertions. */
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "CLG_log.h"

#include "IMB_allocimbuf.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"
#include "imbuf.hh"

#include "IMB_colormanagement.hh"
#include "IMB_colormanagement_intern.hh"

static CLG_LogRef LOG = {"image.read"};

static void imb_handle_colorspace_and_alpha(ImBuf *ibuf,
                                            const int flags,
                                            const char *filepath,
                                            const ImFileColorSpace &file_colorspace,
                                            char r_colorspace[IM_MAX_SPACE])
{
  /* Determine file colorspace. */
  char new_colorspace[IM_MAX_SPACE];

  if (r_colorspace && r_colorspace[0]) {
    /* Existing configured colorspace has priority. */
    STRNCPY_UTF8(new_colorspace, r_colorspace);
  }
  else if (file_colorspace.metadata_colorspace[0] &&
           colormanage_colorspace_get_named(file_colorspace.metadata_colorspace))
  {
    /* Use colorspace from file metadata if provided. */
    STRNCPY_UTF8(new_colorspace, file_colorspace.metadata_colorspace);
  }
  else {
    /* The color-space from the file-path (not a file-path). */
    const char *filepath_colorspace = (filepath) ?
                                          IMB_colormanagement_space_from_filepath_rules(filepath) :
                                          nullptr;
    if (filepath_colorspace) {
      /* Use colorspace from OpenColorIO file rules. */
      STRNCPY_UTF8(new_colorspace, filepath_colorspace);
    }
    else {
      /* Use float colorspace if the image may contain HDR colors, byte otherwise. */
      const char *role_colorspace = IMB_colormanagement_role_colorspace_name_get(
          file_colorspace.is_hdr_float ? COLOR_ROLE_DEFAULT_FLOAT : COLOR_ROLE_DEFAULT_BYTE);
      STRNCPY_UTF8(new_colorspace, role_colorspace);
    }
  }

  if (r_colorspace) {
    BLI_strncpy_utf8(r_colorspace, new_colorspace, IM_MAX_SPACE);
  }

  if (r_colorspace) {
    if (ibuf->byte_buffer.data != nullptr && ibuf->float_buffer.data == nullptr) {
      /* byte buffer is never internally converted to some standard space,
       * store pointer to its color space descriptor instead
       */
      ibuf->byte_buffer.colorspace = colormanage_colorspace_get_named(new_colorspace);
    }
  }

  bool is_data = (r_colorspace && IMB_colormanagement_space_name_is_data(new_colorspace));
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

  colormanage_imbuf_make_linear(ibuf, new_colorspace, ColorManagedFileOutput::Image);
}

ImBuf *IMB_load_image_from_memory(const uchar *mem,
                                  const size_t size,
                                  const int flags,
                                  const char *descr,
                                  const char *filepath,
                                  char r_colorspace[IM_MAX_SPACE])
{
  ImBuf *ibuf;
  const ImFileType *type;

  if (mem == nullptr) {
    CLOG_ERROR(&LOG, "%s: nullptr pointer", __func__);
    return nullptr;
  }

  ImFileColorSpace file_colorspace;

  for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (type->load) {
      ibuf = type->load(mem, size, flags, file_colorspace);
      if (ibuf) {
        imb_handle_colorspace_and_alpha(ibuf, flags, filepath, file_colorspace, r_colorspace);
        return ibuf;
      }
    }
  }

  if ((flags & IB_test) == 0) {
    CLOG_ERROR(&LOG, "%s: unknown file-format (%s)", __func__, descr);
  }

  return nullptr;
}

ImBuf *IMB_load_image_from_file_descriptor(const int file,
                                           const int flags,
                                           const char *filepath,
                                           char r_colorspace[IM_MAX_SPACE])
{
  ImBuf *ibuf = nullptr;

  if (file == -1) {
    return nullptr;
  }

  BLI_mmap_file *mmap_file = BLI_mmap_open(file);
  if (mmap_file == nullptr) {
    CLOG_ERROR(&LOG, "%s: couldn't get mapping for \"%s\"", __func__, filepath);
    return nullptr;
  }

  const uchar *mem = static_cast<const uchar *>(BLI_mmap_get_pointer(mmap_file));
  const size_t size = BLI_mmap_get_length(mmap_file);

  ibuf = IMB_load_image_from_memory(mem, size, flags, filepath, filepath, r_colorspace);

  /* If we got an image but mmap encountered an error,
   * free the image and return nullptr as it could be corrupted. */
  if (ibuf != nullptr && BLI_mmap_any_io_error(mmap_file)) {
    IMB_freeImBuf(ibuf);
    ibuf = nullptr;
  }

  BLI_mmap_free(mmap_file);

  return ibuf;
}

ImBuf *IMB_load_image_from_filepath(const char *filepath,
                                    const int flags,
                                    char r_colorspace[IM_MAX_SPACE])
{
  ImBuf *ibuf;
  int file;

  BLI_assert(!BLI_path_is_rel(filepath));

  file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }

  ibuf = IMB_load_image_from_file_descriptor(file, flags, filepath, r_colorspace);

  if (ibuf) {
    STRNCPY(ibuf->filepath, filepath);
  }

  close(file);

  return ibuf;
}

ImBuf *IMB_thumb_load_image(const char *filepath,
                            const size_t max_thumb_size,
                            char r_colorspace[IM_MAX_SPACE],
                            const IMBThumbLoadFlags load_flags)
{
  const ImFileType *type = IMB_file_type_from_ftype(IMB_test_image_type(filepath));
  if (type == nullptr) {
    return nullptr;
  }

  ImBuf *ibuf = nullptr;
  int flags = IB_byte_data | IB_metadata;
  /* Size of the original image. */
  size_t width = 0;
  size_t height = 0;

  if (type->load_filepath_thumbnail) {
    ImFileColorSpace file_colorspace;
    ibuf = type->load_filepath_thumbnail(
        filepath, flags, max_thumb_size, file_colorspace, &width, &height);
    if (ibuf) {
      imb_handle_colorspace_and_alpha(ibuf, flags, filepath, file_colorspace, r_colorspace);
    }
  }
  else {
    /* Skip images of other types if over 100MB. */
    if (!flag_is_set(load_flags, IMBThumbLoadFlags::LoadLargeFiles)) {
      const size_t file_size = BLI_file_size(filepath);
      if (file_size != size_t(-1) && file_size > THUMB_SIZE_MAX) {
        return nullptr;
      }
    }
    ibuf = IMB_load_image_from_filepath(filepath, flags, r_colorspace);
    if (ibuf) {
      width = ibuf->x;
      height = ibuf->y;
    }
  }

  if (ibuf) {
    if (width > 0 && height > 0) {
      /* Save dimensions of original image into the thumbnail metadata. */
      char cwidth[40];
      char cheight[40];
      SNPRINTF_UTF8(cwidth, "%zu", width);
      SNPRINTF_UTF8(cheight, "%zu", height);
      IMB_metadata_ensure(&ibuf->metadata);
      IMB_metadata_set_field(ibuf->metadata, "Thumb::Image::Width", cwidth);
      IMB_metadata_set_field(ibuf->metadata, "Thumb::Image::Height", cheight);
    }
  }

  return ibuf;
}
