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

    if (!plyData->vertex_normals.is_empty())
      buffer->write_vertex_normal(plyData->vertex_normals[i].x,
                                  plyData->vertex_normals[i].y,
                                  plyData->vertex_normals[i].z);

    if (!plyData->vertex_colors.is_empty())
      buffer->write_vertex_color(uchar(plyData->vertex_colors[i].x * 255),
                                 uchar(plyData->vertex_colors[i].y * 255),
                                 uchar(plyData->vertex_colors[i].z * 255),
                                 uchar(plyData->vertex_colors[i].w * 255));

    if (!plyData->UV_coordinates.is_empty())
      buffer->write_UV(plyData->UV_coordinates[i].x, plyData->UV_coordinates[i].y);

    buffer->write_vertex_end();
  }
  buffer->write_to_file();
}

void write_faces(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (const Vector<uint32_t> &face : plyData->faces) {
    buffer->write_face(char(face.size()), face);
  }
  buffer->write_to_file();
}
void write_edges(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (const std::pair<int, int> &edge : plyData->edges) {
    buffer->write_edge(edge.first, edge.second);
  }
  buffer->write_to_file();
}
}  // namespace blender::io::ply
