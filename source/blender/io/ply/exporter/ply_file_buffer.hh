/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <type_traits>

#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#include <fmt/format.h>

namespace blender::io::ply {

/**
 * File buffer writer.
 * All writes are done into an internal chunked memory buffer
 * (list of default 64 kilobyte blocks).
 * Call write_to_file once in a while to write the memory buffer(s)
 * into the given file.
 */
class FileBuffer : private NonMovable {
  using VectorChar = Vector<char>;
  Vector<VectorChar> blocks_;
  size_t buffer_chunk_size_;
  const char *filepath_;
  FILE *outfile_;

 public:
  FileBuffer(const char *filepath, size_t buffer_chunk_size = 64 * 1024);

  virtual ~FileBuffer() = default;

  /* Write contents to the buffer(s) into a file, and clear the buffers. */
  void write_to_file();

  void close_file();

  virtual void write_vertex(float x, float y, float z) = 0;

  virtual void write_UV(float u, float v) = 0;

  virtual void write_vertex_normal(float nx, float ny, float nz) = 0;

  virtual void write_vertex_color(uchar r, uchar g, uchar b, uchar a) = 0;

  virtual void write_vertex_end() = 0;

  virtual void write_face(char count, Span<uint32_t> const &vertex_indices) = 0;

  virtual void write_edge(int first, int second) = 0;

  void write_header_element(StringRef name, int count);

  void write_header_scalar_property(StringRef dataType, StringRef name);

  void write_header_list_property(StringRef countType, StringRef dataType, StringRef name);

  void write_string(StringRef s);

  void write_newline();

 protected:
  /* Ensure the last block contains at least this amount of free space.
   * If not, add a new block with max of block size & the amount of space needed. */
  void ensure_space(size_t at_least)
  {
    if (blocks_.is_empty() || (blocks_.last().capacity() - blocks_.last().size() < at_least)) {
      blocks_.append(VectorChar());
      blocks_.last().reserve(std::max(at_least, buffer_chunk_size_));
    }
  }

  template<typename... T> void write_fstring(const char *fmt, T &&...args)
  {
    /* Format into a local buffer. */
    fmt::memory_buffer buf;
    fmt::format_to(fmt::appender(buf), fmt, std::forward<T>(args)...);
    size_t len = buf.size();
    ensure_space(len);
    VectorChar &bb = blocks_.last();
    bb.insert(bb.end(), buf.begin(), buf.end());
  }

  void write_bytes(Span<char> bytes);
};

}  // namespace blender::io::ply
