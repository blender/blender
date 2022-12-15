/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <cstdio>

#include "BKE_customdata.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "BLI_fileops.hh"
#include "BLI_math_vector.h"
#include "BLI_memory_utils.hh"

#include "DNA_object_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "../intern/ply_data.hh"
#include "ply_export.hh"
#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_export_load_plydata.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"

namespace blender::io::ply {

void exporter_main(bContext *C, const PLYExportParams &export_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  exporter_main(bmain, scene, view_layer, C, export_params);
}

void exporter_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   bContext *C,
                   const PLYExportParams &export_params)
{
  // Load bmesh data into PlyData struct
  std::unique_ptr<PlyData> plyData = std::make_unique<PlyData>();
  load_plydata(plyData, C);

  // Create file, get writer
  std::unique_ptr<FileBuffer> buffer;

  if (export_params.ascii_format) {
    buffer = std::make_unique<FileBufferAscii>(export_params.filepath);
  }
  else {
    buffer = std::make_unique<FileBufferBinary>(export_params.filepath);
  }

  // Generate and write header
  write_header(buffer, plyData, export_params);

  // Generate and write vertices
  write_vertices(buffer, plyData);

  // Generate and write faces
  write_faces(buffer, plyData);

  // Clean up
  buffer->close_file();
}
}  // namespace blender::io::ply
