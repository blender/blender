/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <stddef.h>
#include <stdio.h>

#include "BLI_array.hh"
#include "BLI_span.hh"

namespace blender::io::ply {

/**
 * Reads underlying PLY file in large chunks, and provides interface for ascii/header
 * parsing to read individual lines, and for binary parsing to read chunks of bytes.
 */
class PlyReadBuffer {
 public:
  PlyReadBuffer(const char *file_path, size_t read_buffer_size = 64 * 1024);
  ~PlyReadBuffer();

  /** After header is parsed, indicate whether the rest of reading will be ascii or binary. */
  void after_header(bool is_binary);

  /**
   * Gets the next line from the file as a Span. The line does not include any newline characters.
   */
  Span<char> read_line();

  /**
   * Reads a number of bytes into provided destination pointer. Returns false if this amount of
   * bytes can not be read.
   */
  bool read_bytes(void *dst, size_t size);

 private:
  bool refill_buffer();

 private:
  FILE *file_ = nullptr;
  Array<char> buffer_;
  int pos_ = 0;
  int buf_used_ = 0;
  int last_newline_ = 0;
  size_t read_buffer_size_ = 0;
  bool at_eof_ = false;
  bool is_binary_ = false;
};

}  // namespace blender::io::ply
