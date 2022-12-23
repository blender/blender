/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <cstdio>

#include "IO_ply.h"
#include "ply_data.hh"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

void write_vertices(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (int i = 0; i < plyData->vertices.size(); i++) {
    buffer->write_vertex(plyData->vertices[i].x, plyData->vertices[i].y, plyData->vertices[i].z);

    if (plyData->vertex_normals.size() > 0)
      buffer->write_vertex_normals(plyData->vertex_normals[i].x,
                                   plyData->vertex_normals[i].y,
                                   plyData->vertex_normals[i].z);

    buffer->write_vertex_end();
  }
  buffer->write_to_file();
}

void write_faces(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (const auto &face : plyData->faces) {
    buffer->write_face(int(face.size()), face);
  }
  buffer->write_to_file();
}
}  // namespace blender::io::ply
