/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_timeit.hh"

#include "IO_wavefront_obj.h"

#include "obj_exporter.hh"
#include "obj_importer.hh"

void OBJ_export(bContext *C, const OBJExportParams *export_params)
{
  SCOPED_TIMER("OBJ export");
  blender::io::obj::exporter_main(C, *export_params);
}

void OBJ_import(bContext *C, const OBJImportParams *import_params)
{
  SCOPED_TIMER(__func__);
  blender::io::obj::importer_main(C, *import_params);
}
