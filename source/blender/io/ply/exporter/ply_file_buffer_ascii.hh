/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <cstdio>
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
  FileBufferAscii(const char *filepath, size_t buffer_chunk_size = 64 * 1024)
      : FileBuffer(filepath, buffer_chunk_size)
  {
  }

  void write_vertex(float x, float y, float z)
  {
    write_fstring("{} {} {}\n", x, y, z);
  }

  void write_face(int count, Vector<int> vertices)
  {
    // fmt::memory_buffer buf;
    // for (auto &&vtx : vertices)
    // {
    //   fmt::format_to(fmt::appender(buf), "{} ", vtx);
    // }

    // write_fstring("{} {}", count, vertices);
  }
};
}  // namespace blender::io::ply
