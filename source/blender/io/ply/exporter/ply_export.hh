/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.h"
#include "ply_data.hh"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

/* Main export function used from within Blender. */
void exporter_main(bContext *C, const PLYExportParams &export_params);

/* Used from tests, where full bContext does not exist. */
void exporter_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   bContext *C,
                   const PLYExportParams &export_params);

}  // namespace blender::io::ply
