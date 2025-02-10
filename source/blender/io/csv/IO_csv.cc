/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup csv
 */

#include "IO_csv.hh"

#include "csv_reader.hh"

namespace blender::io::csv {

PointCloud *import_csv_as_point_cloud(const CSVImportParams *import_params)
{
  return read_csv_file(*import_params);
}

}  // namespace blender::io::csv
