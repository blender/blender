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
  exporter_main(bmain, scene, view_layer, export_params);
}

void exporter_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYExportParams &export_params)
{
  // Load bmesh data into PlyData struct
  PlyData plyData;
  plyData.vertices = {{1, 1, -1},
                      {1, -1, -1},
                      {-1, -1, -1},
                      {-1, 1, -1},
                      {1, 0.999999, 1},
                      {-1, 1, 1},
                      {-1, -1, 1},
                      {0.999999, -1.000001, 1},
                      {1, 1, -1},
                      {1, 0.999999, 1},
                      {0.999999, -1.000001, 1},
                      {1, -1, -1},
                      {1, -1, -1},
                      {0.999999, -1.000001, 1},
                      {-1, -1, 1},
                      {-1, -1, -1},
                      {-1, -1, -1},
                      {-1, -1, 1},
                      {-1, 1, 1},
                      {-1, 1, -1},
                      {1, 0.999999, 1},
                      {1, 1, -1},
                      {-1, 1, -1},
                      {-1, 1, 1}};

  plyData.vertex_normals = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, 1},  {0, 0, 1},
                            {0, 0, 1},  {0, 0, 1},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},
                            {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {-1, 0, 0}, {-1, 0, 0},
                            {-1, 0, 0}, {-1, 0, 0}, {0, 1, 0},  {0, 1, 0},  {0, 1, 0},  {0, 1, 0}};

  plyData.vertex_colors = {{1, 0.8470588235294118, 0},
                           {0, 0.011764705882352941, 1},
                           {0, 0.011764705882352941, 1},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8509803921568627, 0.08627450980392157},
                           {1, 0.8470588235294118, 0},
                           {0, 0.00392156862745098, 1},
                           {0.00392156862745098, 0.00392156862745098, 1},
                           {1, 0.8470588235294118, 0.01568627450980392},
                           {1, 0.8509803921568627, 0.08627450980392157},
                           {0.00392156862745098, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0.00392156862745098, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0, 0.011764705882352941, 1},
                           {0, 0.00392156862745098, 1},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8509803921568627, 0.08627450980392157},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8470588235294118, 0}};

  // Create file, get writer
  FileBuffer *buffer;

  FileBufferAscii derivedBuffer(export_params.filepath);
  buffer = &derivedBuffer;
  // if (export_params.ascii_format) {
  // } else {
  //   FileBufferBinary derivedBuffer(export_params.filepath);
  //   buffer = &derivedBuffer;
  // }

  // Generate and write header
  generate_header(*buffer, plyData, export_params);
  buffer->write_to_file();

  // Generate and write vertices
  for (auto &&vertex : plyData.vertices)
  {
    buffer->write_vertex(vertex.x, vertex.y, vertex.z);
  }
  buffer->write_to_file();

  buffer->close_file();
}
}  // namespace blender::io::ply
