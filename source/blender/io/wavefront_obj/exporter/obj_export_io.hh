/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <cstdio>
#include <type_traits>

#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#include <fmt/format.h>

namespace blender::io::obj {

/**
 * File buffer writer.
 * All writes are done into an internal chunked memory buffer
 * (list of default 64 kilobyte blocks).
 * Call write_fo_file once in a while to write the memory buffer(s)
 * into the given file.
 */
class FormatHandler : NonCopyable, NonMovable {
 private:
  using VectorChar = Vector<char>;
  Vector<VectorChar> blocks_;
  size_t buffer_chunk_size_;

 public:
  FormatHandler(size_t buffer_chunk_size = 64 * 1024) : buffer_chunk_size_(buffer_chunk_size) {}

  /* Write contents to the buffer(s) into a file, and clear the buffers. */
  void write_to_file(FILE *f)
  {
    for (const auto &b : blocks_)
      fwrite(b.data(), 1, b.size(), f);
    blocks_.clear();
  }

  std::string get_as_string() const
  {
    std::string s;
    for (const auto &b : blocks_)
      s.append(b.data(), b.size());
    return s;
  }
  size_t get_block_count() const
  {
    return blocks_.size();
  }

  void append_from(FormatHandler &v)
  {
    blocks_.insert(blocks_.end(),
                   std::make_move_iterator(v.blocks_.begin()),
                   std::make_move_iterator(v.blocks_.end()));
    v.blocks_.clear();
  }

  void write_obj_vertex(float x, float y, float z)
  {
    write_impl("v {:.6f} {:.6f} {:.6f}\n", x, y, z);
  }
  void write_obj_vertex_color(float x, float y, float z, float r, float g, float b)
  {
    write_impl("v {:.6f} {:.6f} {:.6f} {:.4f} {:.4f} {:.4f}\n", x, y, z, r, g, b);
  }
  void write_obj_uv(float x, float y)
  {
    write_impl("vt {:.6f} {:.6f}\n", x, y);
  }
  void write_obj_normal(float x, float y, float z)
  {
    write_impl("vn {:.4f} {:.4f} {:.4f}\n", x, y, z);
  }
  void write_obj_poly_begin()
  {
    write_impl("f");
  }
  void write_obj_poly_end()
  {
    write_obj_newline();
  }
  void write_obj_poly_v_uv_normal(int v, int uv, int n)
  {
    write_impl(" {}/{}/{}", v, uv, n);
  }
  void write_obj_poly_v_normal(int v, int n)
  {
    write_impl(" {}//{}", v, n);
  }
  void write_obj_poly_v_uv(int v, int uv)
  {
    write_impl(" {}/{}", v, uv);
  }
  void write_obj_poly_v(int v)
  {
    write_impl(" {}", v);
  }
  void write_obj_usemtl(StringRef s)
  {
    write_impl("usemtl {}\n", (std::string_view)s);
  }
  void write_obj_mtllib(StringRef s)
  {
    write_impl("mtllib {}\n", (std::string_view)s);
  }
  void write_obj_smooth(int s)
  {
    write_impl("s {}\n", s);
  }
  void write_obj_group(StringRef s)
  {
    write_impl("g {}\n", (std::string_view)s);
  }
  void write_obj_object(StringRef s)
  {
    write_impl("o {}\n", (std::string_view)s);
  }
  void write_obj_edge(int a, int b)
  {
    write_impl("l {} {}\n", a, b);
  }
  void write_obj_cstype()
  {
    write_impl("cstype bspline\n");
  }
  void write_obj_nurbs_degree(int deg)
  {
    write_impl("deg {}\n", deg);
  }
  void write_obj_curve_begin()
  {
    write_impl("curv 0.0 1.0");
  }
  void write_obj_curve_end()
  {
    write_obj_newline();
  }
  void write_obj_nurbs_parm_begin()
  {
    write_impl("parm u 0.0");
  }
  void write_obj_nurbs_parm(float v)
  {
    write_impl(" {:.6f}", v);
  }
  void write_obj_nurbs_parm_end()
  {
    write_impl(" 1.0\n");
  }
  void write_obj_nurbs_group_end()
  {
    write_impl("end\n");
  }
  void write_obj_newline()
  {
    write_impl("\n");
  }

  void write_mtl_newmtl(StringRef s)
  {
    write_impl("newmtl {}\n", (std::string_view)s);
  }
  void write_mtl_float(const char *type, float v)
  {
    write_impl("{} {:.6f}\n", type, v);
  }
  void write_mtl_float3(const char *type, float r, float g, float b)
  {
    write_impl("{} {:.6f} {:.6f} {:.6f}\n", type, r, g, b);
  }
  void write_mtl_illum(int mode)
  {
    write_impl("illum {}\n", mode);
  }
  /* NOTE: options, if present, will have its own leading space. */
  void write_mtl_map(const char *type, StringRef options, StringRef value)
  {
    write_impl("{}{} {}\n", type, (std::string_view)options, (std::string_view)value);
  }

  void write_string(StringRef s)
  {
    write_impl("{}\n", (std::string_view)s);
  }

 private:
  /* Ensure the last block contains at least this amount of free space.
   * If not, add a new block with max of block size & the amount of space needed. */
  void ensure_space(size_t at_least)
  {
    if (blocks_.is_empty() || (blocks_.last().capacity() - blocks_.last().size() < at_least)) {
      blocks_.append(VectorChar());
      blocks_.last().reserve(std::max(at_least, buffer_chunk_size_));
    }
  }

  template<typename... T> void write_impl(const char *fmt, T &&...args)
  {
    /* Format into a local buffer. */
    fmt::memory_buffer buf;
    fmt::format_to(fmt::appender(buf), fmt, std::forward<T>(args)...);
    size_t len = buf.size();
    ensure_space(len);
    VectorChar &bb = blocks_.last();
    bb.insert(bb.end(), buf.begin(), buf.end());
  }
};

}  // namespace blender::io::obj
