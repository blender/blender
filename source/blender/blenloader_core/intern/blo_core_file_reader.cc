/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>

/* For #close function. */
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "BLI_fileops.h"
#include "BLI_filereader.h"

#include "BLO_core_file_reader.hh"

namespace blender {

FileReader *BLO_file_reader_uncompressed_from_descriptor(int filedes)
{
  if (FileReader *mmap_reader = BLI_filereader_new_mmap(filedes)) {
    /* The mapped memory is still valid even when the file is closed. */
    close(filedes);
    return BLO_file_reader_uncompressed(mmap_reader);
  }
  return BLO_file_reader_uncompressed(BLI_filereader_new_file(filedes));
}

FileReader *BLO_file_reader_uncompressed_from_memory(const void *mem, const int memsize)
{
  return BLO_file_reader_uncompressed(BLI_filereader_new_memory(mem, memsize));
}

FileReader *BLO_file_reader_uncompressed(FileReader *rawfile)
{
  if (!rawfile) {
    return nullptr;
  }
  char first_bytes[7];
  if (rawfile->read(rawfile, first_bytes, sizeof(first_bytes)) != sizeof(first_bytes)) {
    /* The file is too small to possibly be a valid blend file. */
    rawfile->close(rawfile);
    return nullptr;
  }
  /* Rewind to the start of the file. */
  rawfile->seek(rawfile, 0, SEEK_SET);
  if (memcmp(first_bytes, "BLENDER", sizeof(first_bytes)) == 0) {
    /* The file is uncompressed. */
    return rawfile;
  }
  if (BLI_file_magic_is_gzip(first_bytes)) {
    /* The new reader takes ownership of the rawfile. */
    return BLI_filereader_new_gzip(rawfile);
  }
  if (BLI_file_magic_is_zstd(first_bytes)) {
    /* The new reader takes ownership of the rawfile. */
    return BLI_filereader_new_zstd(rawfile);
  }
  rawfile->close(rawfile);
  return nullptr;
}

}  // namespace blender
