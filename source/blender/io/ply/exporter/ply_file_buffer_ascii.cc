/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_file_buffer_ascii.hh"

namespace blender::io::ply {

void FileBufferAscii::write_vertex(float x, float y, float z)
{
  write_fstring("{} {} {}", x, y, z);
}

void FileBufferAscii::write_UV(float u, float v)
{
  write_fstring(" {} {}", u, v);
}

void FileBufferAscii::write_vertex_normal(float nx, float ny, float nz)
{
  write_fstring(" {} {} {}", nx, ny, nz);
}

void FileBufferAscii::write_vertex_color(uchar r, uchar g, uchar b, uchar a)
{
  write_fstring(" {} {} {} {}", r, g, b, a);
}

void FileBufferAscii::write_vertex_end()
{
  write_fstring("\n");
}

void FileBufferAscii::write_face(char count, Span<uint32_t> const &vertex_indices)
{
  write_fstring("{}", int(count));

  for (const uint32_t v : vertex_indices) {
    write_fstring(" {}", v);
  }
  write_newline();
}

void FileBufferAscii::write_edge(int first, int second)
{
  write_fstring("{} {}", first, second);
  write_newline();
}
}  // namespace blender::io::ply
