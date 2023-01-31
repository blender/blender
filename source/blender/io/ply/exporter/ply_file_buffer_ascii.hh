/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "ply_file_buffer.hh"

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#define FMT_HEADER_ONLY
#include <fmt/format.h>

namespace blender::io::ply {
class FileBufferAscii : public FileBuffer {
 public:
  using FileBuffer::FileBuffer;

  void write_vertex(float x, float y, float z) override
  {
    write_fstring("{} {} {}", x, y, z);
  }

  void write_UV(float u, float v) override
  {
    write_fstring(" {} {}", u, v);
  }

  void write_vertex_normal(float nx, float ny, float nz) override
  {
    write_fstring(" {} {} {}", nx, ny, nz);
  }

  void write_vertex_color(uchar r, uchar g, uchar b, uchar a) override
  {
    write_fstring(" {} {} {} {}", r, g, b, a);
  }

  void write_vertex_end() override
  {
    write_fstring("\n");
  }

  void write_face(char count, Vector<uint32_t> const &vertex_indices) override
  {
    write_fstring("{}", count);

    for (auto &&v : vertex_indices) {
      write_fstring(" {}", v);
    }
    write_newline();
  }
  void write_edge(int first, int second) override
  {
    write_fstring("{} {}", first, second);
    write_newline();
  }
};
}  // namespace blender::io::ply
