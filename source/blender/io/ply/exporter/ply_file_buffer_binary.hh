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
#include "ply_file_buffer.hh"

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#define FMT_HEADER_ONLY
#include <bitset>
#include <fmt/format.h>

namespace blender::io::ply {
class FileBufferBinary : public FileBuffer {
 public:
  using FileBuffer::FileBuffer;

  void write_float_3(float x, float y, float z) override
  {

    auto *xbits = reinterpret_cast<char *>(&x);
    auto *ybits = reinterpret_cast<char *>(&y);
    auto *zbits = reinterpret_cast<char *>(&z);

    std::vector<char> data(xbits, xbits + sizeof(float));
    data.insert(data.end(), ybits, ybits + sizeof(float));
    data.insert(data.end(), zbits, zbits + sizeof(float));

    write_bytes(data);
  }

  void write_uchar_4(uchar r, uchar g, uchar b, uchar a) override
  {

    auto *rbits = reinterpret_cast<char *>(&r);
    auto *gbits = reinterpret_cast<char *>(&g);
    auto *bbits = reinterpret_cast<char *>(&b);
    auto *abits = reinterpret_cast<char *>(&a);

    std::vector<char> data(rbits, rbits + sizeof(float));
    data.insert(data.end(), gbits, gbits + sizeof(float));
    data.insert(data.end(), bbits, bbits + sizeof(float));
    data.insert(data.end(), abits, abits + sizeof(float));

    write_bytes(data);

  }

  void write_face(int size, Vector<uint32_t> const &vertex_indices) override
  {
    std::vector<char> data;
    data.push_back((char)size);
    for (auto &&vertexIndex : vertex_indices) {
      uint32_t x = vertexIndex;
      auto *vtxbits = static_cast<char *>(static_cast<void *>(&x));
      data.insert(data.end(), vtxbits, vtxbits + sizeof(uint32_t));
    }

    write_bytes(data);
  }

  void write_edge(int first, int second) override
  {
    std::vector<char> first_char;
    std::vector<char> second_char;

    first_char.insert(first_char.end(), char(first));
    second_char.insert(second_char.end(), char(second));

    write_bytes(first_char);
    write_bytes(second_char);

  }

  void write_ASCII_new_line()
  {
    
  }
};
}  // namespace blender::io::ply
