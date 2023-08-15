/* SPDX-FileCopyrightText: 2004-2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#ifndef WIN32
#  include <unistd.h> /* for read close */
#else
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h> /* for open close read */
#endif

#include "BLI_blenlib.h"
#include "BLI_filereader.h"

#include "MEM_guardedalloc.h"

typedef struct {
  FileReader reader;

  int filedes;
} RawFileReader;

static ssize_t file_read(FileReader *reader, void *buffer, size_t size)
{
  RawFileReader *rawfile = (RawFileReader *)reader;
  ssize_t readsize = read(rawfile->filedes, buffer, size);

  if (readsize >= 0) {
    rawfile->reader.offset += readsize;
  }

  return readsize;
}

static off64_t file_seek(FileReader *reader, off64_t offset, int whence)
{
  RawFileReader *rawfile = (RawFileReader *)reader;
  rawfile->reader.offset = BLI_lseek(rawfile->filedes, offset, whence);
  return rawfile->reader.offset;
}

static void file_close(FileReader *reader)
{
  RawFileReader *rawfile = (RawFileReader *)reader;
  close(rawfile->filedes);
  MEM_freeN(rawfile);
}

FileReader *BLI_filereader_new_file(int filedes)
{
  RawFileReader *rawfile = MEM_callocN(sizeof(RawFileReader), __func__);

  rawfile->filedes = filedes;

  rawfile->reader.read = file_read;
  rawfile->reader.seek = file_seek;
  rawfile->reader.close = file_close;

  return (FileReader *)rawfile;
}
