/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blendthumb
 *
 * This file defines the thumbnail generation command (typically used on UNIX).
 *
 * To run automatically with a file manager such as Nautilus, save this file
 * in a directory that is listed in PATH environment variable, and create
 * `blender.thumbnailer` file in `${HOME}/.local/share/thumbnailers/` directory
 * with the following contents:
 *
 * \code{.txt}
 * [Thumbnailer Entry]
 * TryExec=blender-thumbnailer
 * Exec=blender-thumbnailer %u %o
 * MimeType=application/x-blender;
 * \endcode
 */

#include <fstream>
#include <optional>

#include <fcntl.h>
#ifndef WIN32
#  include <unistd.h> /* For read close. */
#else
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h> /* For open close read. */
#endif

#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_vector.hh"

#include "blendthumb.hh"

/**
 * This function opens .blend file from src_blend, extracts thumbnail from file if there is one,
 * and writes `.png` image into `dst_png`.
 * Returns exit code (0 if successful).
 */
static eThumbStatus extract_png_from_blend_file(const char *src_blend, const char *dst_png)
{
  eThumbStatus err;

  /* Open source file `src_blend`. */
  const int src_file = BLI_open(src_blend, O_BINARY | O_RDONLY, 0);
  if (src_file == -1) {
    return BT_FILE_ERR;
  }

  /* Thumbnail reading is responsible for freeing `file` and closing `src_file`. */
  FileReader *file = BLI_filereader_new_file(src_file);
  if (file == nullptr) {
    close(src_file);
    return BT_FILE_ERR;
  }

  /* Extract thumbnail from file. */
  Thumbnail thumb;
  err = blendthumb_create_thumb_from_file(file, &thumb);
  if (err != BT_OK) {
    return err;
  }

  /* Write thumbnail to `dst_png`. */
  const int dst_file = BLI_open(dst_png, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (dst_file == -1) {
    return BT_FILE_ERR;
  }

  std::optional<blender::Vector<uint8_t>> png_buf_opt = blendthumb_create_png_data_from_thumb(
      &thumb);
  if (!png_buf_opt) {
    err = BT_ERROR;
  }
  else {
    blender::Vector<uint8_t> png_buf = *png_buf_opt;
    err = (write(dst_file, png_buf.data(), png_buf.size()) == png_buf.size()) ? BT_OK :
                                                                                BT_FILE_ERR;
  }
  close(dst_file);

  return err;
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::cerr << "Usage: blender-thumbnailer <input.blend> <output.png>" << std::endl;
    return -1;
  }

  eThumbStatus ret = extract_png_from_blend_file(argv[1], argv[2]);

  return int(ret);
}
