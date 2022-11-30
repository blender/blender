/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.h"

namespace blender::io::ply {

void ply_import_report_error(FILE *file);

/* Main import function used from within Blender. */
void importer_main(bContext *C, const PLYImportParams &import_params);

/* Used from tests, where full bContext does not exist. */
void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params);
enum PlyFormatType { ascii, binary_big_endian , binary_little_endian};

}  // namespace blender::io::ply
