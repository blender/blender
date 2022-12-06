/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <cstdio>

#include "IO_ply.h"
#include "../intern/ply_data.hh"
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

void export_vertices(std::unique_ptr<FileBuffer> &buffer,
                     std::unique_ptr<PlyData> &plyData,
                     PLYExportParams export_params);

void export_faces(std::unique_ptr<FileBuffer> &buffer,
                  std::unique_ptr<PlyData> &plyData,
                  const PLYExportParams export_params);

}  // namespace blender::io::ply
