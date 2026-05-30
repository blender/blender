/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#pragma once

#include <cstdio>

#include "BLI_string_ref.hh"

namespace blender {

struct PointCloud;
struct ReportList;

namespace io::spz {

/* Read the SPZ file pointed by the filepath.
 * Supports multiple SPZ file formats. */
PointCloud *read_spz_file(StringRefNull filepath, ReportList *reports);

/* Read SZP file that uses a single GZip stream to compress the whole file.
 * This applies to SPZ format versions 2 and 3. */
PointCloud *read_spz_gzip_compressed_file(FILE *file, ReportList *reports);

/* Read SZP file that uses NGSP format (introduced in version 4, uncompressed header, multiple
 * zstd compressed streams). */
PointCloud *read_spz_ngsp_file(FILE *file, ReportList *reports);

}  // namespace io::spz
}  // namespace blender
