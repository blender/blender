/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BLI_timeit.hh"

#include "IO_ply.h"
#include "ply_export.hh"
#include "ply_import.hh"

void PLY_export(bContext *C, const struct PLYExportParams *export_params)
{
  SCOPED_TIMER("PLY Export");
  blender::io::ply::exporter_main(C, *export_params);
}

void PLY_import(bContext *C, const struct PLYImportParams *import_params)
{
  SCOPED_TIMER("PLY Import");
  blender::io::ply::importer_main(C, *import_params);
}
