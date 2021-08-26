/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004-2021 Blender Foundation
 * All rights reserved.
 */

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
