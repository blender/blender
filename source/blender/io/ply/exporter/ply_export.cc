/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <cstdio>

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "IO_ply.hh"

#include "ply_data.hh"
#include "ply_export.hh"
#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_export_load_plydata.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"

namespace blender::io::ply {

void exporter_main(bContext *C, const PLYExportParams &export_params)
{
  std::unique_ptr<blender::io::ply::PlyData> plyData = std::make_unique<PlyData>();

  Depsgraph *depsgraph = nullptr;
  bool needs_free = false;

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  if (export_params.collection[0]) {
    Collection *collection = reinterpret_cast<Collection *>(
        BKE_libblock_find_name(bmain, ID_GR, export_params.collection));
    if (!collection) {
      BKE_reportf(export_params.reports,
                  RPT_ERROR,
                  "PLY Export: Unable to find collection '%s'",
                  export_params.collection);
      return;
    }

    ViewLayer *view_layer = CTX_data_view_layer(C);

    depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
    needs_free = true;
    DEG_graph_build_from_collection(depsgraph, collection);
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  }
  else {
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  }

  load_plydata(*plyData, depsgraph, export_params);

  if (needs_free) {
    DEG_graph_free(depsgraph);
  }

  std::unique_ptr<FileBuffer> buffer;

  try {
    if (export_params.ascii_format) {
      buffer = std::make_unique<FileBufferAscii>(export_params.filepath);
    }
    else {
      buffer = std::make_unique<FileBufferBinary>(export_params.filepath);
    }
  }
  catch (const std::system_error &ex) {
    fprintf(stderr, "%s\n", ex.what());
    BKE_reportf(export_params.reports,
                RPT_ERROR,
                "PLY Export: Cannot open file '%s'",
                export_params.filepath);
    return;
  }

  write_header(*buffer.get(), *plyData.get(), export_params);

  write_vertices(*buffer.get(), *plyData.get());

  write_faces(*buffer.get(), *plyData.get());

  write_edges(*buffer.get(), *plyData.get());

  buffer->close_file();
}
}  // namespace blender::io::ply
