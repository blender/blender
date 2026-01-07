/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include "BLI_timeit.hh"

#include "IO_stl.hh"
#include "stl_export.hh"
#include "stl_import.hh"

namespace blender {

void STL_import(bContext *C, const STLImportParams *import_params)
{
  SCOPED_TIMER("STL Import");
  io::stl::importer_main(C, *import_params);
}

void STL_export(bContext *C, const STLExportParams *export_params)
{
  SCOPED_TIMER("STL Export");
  io::stl::exporter_main(C, *export_params);
}

Mesh *STL_import_mesh(const STLImportParams *import_params)
{
  return io::stl::read_stl_file(*import_params);
}

}  // namespace blender
