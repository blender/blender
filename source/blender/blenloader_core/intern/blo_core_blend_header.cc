/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "BLI_endian_defines.h"
#include "BLI_filereader.h"

#include "BLO_core_bhead.hh"
#include "BLO_core_blend_header.hh"

BHeadType BlenderHeader::bhead_type() const
{
  if (this->pointer_size == 4) {
    return BHeadType::BHead4;
  }
  if (this->file_format_version == BLEND_FILE_FORMAT_VERSION_0) {
    return BHeadType::SmallBHead8;
  }
  BLI_assert(this->file_format_version == BLEND_FILE_FORMAT_VERSION_1);
  return BHeadType::LargeBHead8;
}

BlenderHeaderVariant BLO_readfile_blender_header_decode(FileReader *file)
{
  char header_bytes[MAX_SIZEOFBLENDERHEADER];
  /* We read the minimal number of header bytes first. If necessary, the remaining bytes are read
   * below. */
  int64_t readsize = file->read(file, header_bytes, MIN_SIZEOFBLENDERHEADER);
  if (readsize != MIN_SIZEOFBLENDERHEADER) {
    return BlenderHeaderInvalid{};
  }
  if (!STREQLEN(header_bytes, "BLENDER", 7)) {
    return BlenderHeaderInvalid{};
  }
  /* If the first 7 bytes are BLENDER, it is very likely that this is a newer version of the
   * blend-file format. If the rest of the decode fails, we can still report that this was a
   * Blender file of a potentially future version. */

  BlenderHeader header;
  /* In the old header format, the next bytes indicate the pointer size. In the new format a
   * version number comes next. */
  const bool is_legacy_header = ELEM(header_bytes[7], '_', '-');

  if (is_legacy_header) {
    header.file_format_version = 0;
    switch (header_bytes[7]) {
      case '_':
        header.pointer_size = 4;
        break;
      case '-':
        header.pointer_size = 8;
        break;
      default:
        return BlenderHeaderUnknown{};
    }
    switch (header_bytes[8]) {
      case 'v':
        header.endian = L_ENDIAN;
        break;
      case 'V':
        header.endian = B_ENDIAN;
        break;
      default:
        return BlenderHeaderUnknown{};
    }
    if (!isdigit(header_bytes[9]) || !isdigit(header_bytes[10]) || !isdigit(header_bytes[11])) {
      return BlenderHeaderUnknown{};
    }
    char version_str[4];
    memcpy(version_str, header_bytes + 9, 3);
    version_str[3] = '\0';
    header.file_version = atoi(version_str);
    return header;
  }

  if (!isdigit(header_bytes[7]) || !isdigit(header_bytes[8])) {
    return BlenderHeaderUnknown{};
  }
  char header_size_str[3];
  memcpy(header_size_str, header_bytes + 7, 2);
  header_size_str[2] = '\0';
  const int header_size = atoi(header_size_str);
  if (header_size != MAX_SIZEOFBLENDERHEADER) {
    return BlenderHeaderUnknown{};
  }

  /* Read remaining header bytes. */
  const int64_t remaining_bytes_to_read = header_size - MIN_SIZEOFBLENDERHEADER;
  readsize = file->read(file, header_bytes + MIN_SIZEOFBLENDERHEADER, remaining_bytes_to_read);
  if (readsize != remaining_bytes_to_read) {
    return BlenderHeaderUnknown{};
  }
  if (header_bytes[9] != '-') {
    return BlenderHeaderUnknown{};
  }
  header.pointer_size = 8;
  if (!isdigit(header_bytes[10]) || !isdigit(header_bytes[11])) {
    return BlenderHeaderUnknown{};
  }
  char blend_file_version_format_str[3];
  memcpy(blend_file_version_format_str, header_bytes + 10, 2);
  blend_file_version_format_str[2] = '\0';
  header.file_format_version = atoi(blend_file_version_format_str);
  if (header.file_format_version != 1) {
    return BlenderHeaderUnknown{};
  }
  if (header_bytes[12] != 'v') {
    return BlenderHeaderUnknown{};
  }
  header.endian = L_ENDIAN;
  if (!isdigit(header_bytes[13]) || !isdigit(header_bytes[14]) || !isdigit(header_bytes[15]) ||
      !isdigit(header_bytes[16]))
  {
    return BlenderHeaderUnknown{};
  }
  char version_str[5];
  memcpy(version_str, header_bytes + 13, 4);
  version_str[4] = '\0';
  header.file_version = std::atoi(version_str);
  return header;
}
