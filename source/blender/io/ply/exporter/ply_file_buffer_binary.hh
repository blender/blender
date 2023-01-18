/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <cstdio>
#include <string>
#include <type_traits>

#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

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

  void write_vertex(float x, float y, float z) override
  {
    char *xbits = reinterpret_cast<char *>(&x);
    char *ybits = reinterpret_cast<char *>(&y);
    char *zbits = reinterpret_cast<char *>(&z);

    Vector<char> data{};
    data.reserve(12); /* resize vector for 3 floats */
    data.insert(data.end(), xbits, xbits + sizeof(float));
    data.insert(data.end(), ybits, ybits + sizeof(float));
    data.insert(data.end(), zbits, zbits + sizeof(float));

    write_bytes(data);
  }

  void write_UV(float u, float v) override
  {
    char *ubits = reinterpret_cast<char *>(&u);
    char *vbits = reinterpret_cast<char *>(&v);

    Vector<char> data{};
    data.reserve(8); /* resize vector for 2 floats */
    data.insert(data.end(), ubits, ubits + sizeof(float));
    data.insert(data.end(), vbits, vbits + sizeof(float));

    write_bytes(data);
  }

  void write_vertex_normal(float nx, float ny, float nz) override
  {
    char *xbits = reinterpret_cast<char *>(&nx);
    char *ybits = reinterpret_cast<char *>(&ny);
    char *zbits = reinterpret_cast<char *>(&nz);

    Vector<char> data{};
    data.reserve(12); /* resize vector for 3 floats */
    data.insert(data.end(), xbits, xbits + sizeof(float));
    data.insert(data.end(), ybits, ybits + sizeof(float));
    data.insert(data.end(), zbits, zbits + sizeof(float));

    write_bytes(data);
  }

  void write_vertex_color(uchar r, uchar g, uchar b, uchar a) override
  {
    char *rbits = reinterpret_cast<char *>(&r);
    char *gbits = reinterpret_cast<char *>(&g);
    char *bbits = reinterpret_cast<char *>(&b);
    char *abits = reinterpret_cast<char *>(&a);

    Vector<char> data(rbits, rbits + sizeof(char));
    data.reserve(4); /* resize vector for 4 bytes */
    data.insert(data.end(), gbits, gbits + sizeof(char));
    data.insert(data.end(), bbits, bbits + sizeof(char));
    data.insert(data.end(), abits, abits + sizeof(char));

    write_bytes(data);
  }

  void write_vertex_end() override
  {
    /* In binary, there is no end to a vertex. */
  }

  void write_face(int size, Vector<uint32_t> const &vertex_indices) override
  {
    /* Pre allocate memory so no further allocation has to be done for typical faces. */
    Vector<char, 128> data;
    data.append((char)size);
    for (auto &&vertexIndex : vertex_indices) {
      uint32_t x = vertexIndex;
      auto *vtxbits = static_cast<char *>(static_cast<void *>(&x));
      data.insert(data.end(), vtxbits, vtxbits + sizeof(uint32_t));
    }

    write_bytes(data);
  }

  void write_edge(int first, int second) override
  {

    auto *fbits = reinterpret_cast<char *>(&first);
    auto *sbits = reinterpret_cast<char *>(&second);

    Vector<char> data(fbits, fbits + sizeof(int));
    data.insert(data.end(), sbits, sbits + sizeof(int));

    write_bytes(data);
  }
};
}  // namespace blender::io::ply
