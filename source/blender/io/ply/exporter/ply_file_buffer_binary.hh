/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <string>
#include <type_traits>

#include "BLI_array.hh"
#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"

#include "ply_file_buffer.hh"

#include <bitset>

namespace blender::io::ply {
class FileBufferBinary : public FileBuffer {
  using FileBuffer::FileBuffer;

 public:
  void write_vertex(float x, float y, float z) override;

  void write_UV(float u, float v) override;

  void write_vertex_normal(float nx, float ny, float nz) override;

  void write_vertex_color(uchar r, uchar g, uchar b, uchar a) override;

  void write_vertex_end() override;

  void write_face(char size, Span<uint32_t> const &vertex_indices) override;

  void write_edge(int first, int second) override;
};
}  // namespace blender::io::ply
