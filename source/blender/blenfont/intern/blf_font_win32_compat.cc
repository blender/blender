/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Workaround for win32 which needs to use BLI_fopen to access files.
 *
 * defines #FT_New_Face__win32_compat, a drop-in replacement for \a #FT_New_Face.
 */

#ifdef WIN32

#  include <stdio.h>

#  include <ft2build.h>
#  include FT_FREETYPE_H

#  include "MEM_guardedalloc.h"

#  include "BLI_fileops.h"
#  include "BLI_utildefines.h"

#  include "blf_internal.hh"

/* internal freetype defines */
#  define STREAM_FILE(stream) static_cast<FILE *>(stream->descriptor.pointer)
#  define FT_THROW(e) -1

static void ft_ansi_stream_close(FT_Stream stream)
{
  fclose(STREAM_FILE(stream));

  stream->descriptor.pointer = nullptr;
  stream->size = 0;
  stream->base = 0;

  /* WARNING: this works but be careful!
   * Checked freetype sources, there isn't any access after closing. */
  MEM_freeN(stream);
}

static ulong ft_ansi_stream_io(FT_Stream stream, ulong offset, uchar *buffer, ulong count)
{

  if (!count && offset > stream->size) {
    return 1;
  }

  FILE *file = STREAM_FILE(stream);

  if (stream->pos != offset) {
    BLI_fseek(file, offset, SEEK_SET);
  }

  return fread(buffer, 1, count, file);
}

static FT_Error FT_Stream_Open__win32_compat(FT_Stream stream, const char *filepathname)
{
  BLI_assert(stream);

  stream->descriptor.pointer = nullptr;
  stream->pathname.pointer = (char *)filepathname;
  stream->base = 0;
  stream->pos = 0;
  stream->read = nullptr;
  stream->close = nullptr;

  FILE *file = BLI_fopen(filepathname, "rb");
  if (!file) {
    fprintf(stderr,
            "FT_Stream_Open: "
            "could not open `%s'\n",
            filepathname);
    return FT_THROW(Cannot_Open_Resource);
  }

  BLI_fseek(file, 0LL, SEEK_END);
  stream->size = ftell(file);
  if (!stream->size) {
    fprintf(stderr,
            "FT_Stream_Open: "
            "opened `%s' but zero-sized\n",
            filepathname);
    fclose(file);
    return FT_THROW(Cannot_Open_Stream);
  }

  BLI_fseek(file, 0LL, SEEK_SET);

  stream->descriptor.pointer = file;
  stream->read = ft_ansi_stream_io;
  stream->close = ft_ansi_stream_close;

  return FT_Err_Ok;
}

FT_Error FT_New_Face__win32_compat(FT_Library library,
                                   const char *pathname,
                                   FT_Long face_index,
                                   FT_Face *aface)
{
  FT_Error err;
  FT_Open_Args open;
  FT_Stream stream = static_cast<FT_Stream>(MEM_callocN(sizeof(*stream), __func__));

  open.flags = FT_OPEN_STREAM;
  open.stream = stream;

  stream->pathname.pointer = (void *)pathname;

  err = FT_Stream_Open__win32_compat(stream, pathname);
  if (err) {
    MEM_freeN(stream);
    return err;
  }

  err = FT_Open_Face(library, &open, face_index, aface);
  /* no need to free 'stream', its handled by FT_Open_Face if an error occurs */

  return err;
}

#endif /* WIN32 */
