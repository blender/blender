/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <cstdio>

#include "../intern/ply_data.hh"
#include "IO_ply.h"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

void write_vertices(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (const auto &vertex : plyData->vertices) {
    buffer->write_vertex(vertex.x, vertex.y, vertex.z);
  }
  buffer->write_to_file();
}

void write_faces(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (const auto &face : plyData->faces) {
    buffer->write_face(face.size(), face);
  }
  buffer->write_to_file();
}
}  // namespace blender::io::ply
