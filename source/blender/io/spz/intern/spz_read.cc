/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#include "spz_read.hh"

#include <bit>

#include "BLI_fileops.hh"

#include "BKE_report.hh"

#include "CLG_log.h"

#include "spz_types.hh"

namespace blender::io::spz {

static CLG_LogRef LOG = {"io.spz"};

template<class T> static bool read_spz_scalar(FILE *file, T &data)
{
  if (fread(&data, sizeof(T), 1, file) != 1) {
    return false;
  }
  return true;
}

PointCloud *read_spz_file(const StringRefNull filepath, ReportList *reports)
{
  if constexpr (std::endian::native != std::endian::little) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Big endian machines are not supported");
    return nullptr;
  }

  FILE *file = BLI_fopen(filepath.c_str(), "rb");
  if (!file) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Error opening file for read");
    return nullptr;
  }

  uint32_t magic = 0;
  if (!read_spz_scalar(file, magic)) {
    BKE_report(reports, RPT_ERROR, "SPZ Read: Error reading magic value from file");
    fclose(file);
    return nullptr;
  }
  CLOG_DEBUG(&LOG, "Got file header magic: 0x%X", magic);

  /* Rewind the file stream so that specialized version reading can start from its beginning. */
  if (fseek(file, 0, SEEK_SET) == -1) {
    fclose(file);
    return nullptr;
  }

  PointCloud *point_cloud = nullptr;

  if (magic == SPZ_HEADER_MAGIC) {
    CLOG_DEBUG(&LOG, "Detected uncompressed NGSP magic");
    point_cloud = read_spz_ngsp_file(file, reports);
  }
  else if ((magic & 0xffff) == 0x8b1f) {
    /* Format prior to v4: single GZip stream. */
    CLOG_DEBUG(&LOG, "Detected Gzip compressed stream");
    point_cloud = read_spz_gzip_compressed_file(file, reports);
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "SPZ Read: Unrecognized file format");
  }

  fclose(file);

  return point_cloud;
}

}  // namespace blender::io::spz
