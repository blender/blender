/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#include "IO_spz.hh"

#include "importer/spz_import.hh"

namespace blender {

void SPZ_import(bContext *C, const SPZImportParams *import_params)
{
  io::spz::importer_main(C, *import_params);
}

PointCloud *SPZ_import_point_cloud(const SPZImportParams *import_params)
{
  return io::spz::import_point_cloud(*import_params);
}

}  // namespace blender
