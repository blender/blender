/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_timeit.hh"

#include "IO_wavefront_obj.h"

#include "obj_exporter.hh"

/**
 * C-interface for the exporter.
 */
void OBJ_export(bContext *C, const OBJExportParams *export_params)
{
  SCOPED_TIMER("OBJ export");
  blender::io::obj::exporter_main(C, *export_params);
}
