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
  // not clean yet, but will do for now
  // hey blender, change your vertex color name to something logical <3
  for (int i = 0; i< plyData->vertices.size(); i++) { // change to for-loop for index
    auto &vertex = plyData->vertices[i];
    auto &vertex_color = plyData->vertex_colors[i];

    // same for normals later...
    buffer->write_float_3(vertex.x, vertex.y, vertex.z);
    buffer->write_uchar_4(uchar(vertex_color.x * 255), uchar(vertex_color.y * 255), uchar(vertex_color.z * 255), uchar(vertex_color.w * 255));
    buffer->write_ASCII_new_line();
  }
  buffer->write_to_file();
}

void write_edges(std::unique_ptr<FileBuffer> &buffer, std::unique_ptr<PlyData> &plyData)
{
  for (const auto &edge : plyData->edges) {
    buffer->write_edge(edge.first, edge.second);
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
