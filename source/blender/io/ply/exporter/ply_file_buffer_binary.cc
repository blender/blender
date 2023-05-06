/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_file_buffer_binary.hh"

#include "BLI_math_vector_types.hh"

namespace blender::io::ply {
void FileBufferBinary::write_vertex(float x, float y, float z)
{
  float3 vector(x, y, z);
  char *bits = reinterpret_cast<char *>(&vector);
  Span<char> span(bits, sizeof(float3));

  write_bytes(span);
}

void FileBufferBinary::write_UV(float u, float v)
{
  float2 vector(u, v);
  char *bits = reinterpret_cast<char *>(&vector);
  Span<char> span(bits, sizeof(float2));

  write_bytes(span);
}

void FileBufferBinary::write_vertex_normal(float nx, float ny, float nz)
{
  float3 vector(nx, ny, nz);
  char *bits = reinterpret_cast<char *>(&vector);
  Span<char> span(bits, sizeof(float3));

  write_bytes(span);
}

void FileBufferBinary::write_vertex_color(uchar r, uchar g, uchar b, uchar a)
{
  uchar4 vector(r, g, b, a);
  char *bits = reinterpret_cast<char *>(&vector);
  Span<char> span(bits, sizeof(uchar4));

  write_bytes(span);
}

void FileBufferBinary::write_vertex_end()
{
  /* In binary, there is no end to a vertex. */
}

void FileBufferBinary::write_face(char size, Span<uint32_t> const &vertex_indices)
{
  write_bytes(Span<char>({size}));

  write_bytes(vertex_indices.cast<char>());
}

void FileBufferBinary::write_edge(int first, int second)
{
  int2 vector(first, second);
  char *bits = reinterpret_cast<char *>(&vector);
  Span<char> span(bits, sizeof(int2));

  write_bytes(span);
}
}  // namespace blender::io::ply
