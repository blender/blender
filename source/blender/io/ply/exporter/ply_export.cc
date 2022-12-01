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
#include "ply_export_header.hh"
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
  PlyData plyData;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;

  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    if (object->type != OB_MESH)
      continue;

    auto mesh = BKE_mesh_new_from_object(depsgraph, object, true, true);
    for (auto &&vertex : mesh->verts())
    {
      plyData.vertices.append(vertex.co);
    }

  }

  DEG_OBJECT_ITER_END;

  // Create file, get writer
  FileBuffer *buffer = nullptr;

  if (export_params.ascii_format) {
    buffer = new FileBufferAscii(export_params.filepath);
  }
  else {
    buffer = new FileBufferBinary(export_params.filepath);
  }

  // Generate and write header
  generate_header(*buffer, plyData, export_params);

  // Generate and write vertices
  for (auto &&vertex : plyData.vertices) {
    buffer->write_vertex(vertex.x, vertex.y, vertex.z);
  }
  buffer->write_to_file();

  buffer->close_file();
  delete buffer;
}
}  // namespace blender::io::ply
