/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ply_import_buffer.hh"

#include "BLI_fileops.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>

static inline bool is_newline(char ch)
{
  return ch == '\n';
}

namespace blender::io::ply {

PlyReadBuffer::PlyReadBuffer(const char *file_path, size_t read_buffer_size)
    : buffer_(read_buffer_size), read_buffer_size_(read_buffer_size)
{
  file_ = BLI_fopen(file_path, "rb");
}

PlyReadBuffer::~PlyReadBuffer()
{
  if (file_ != nullptr) {
    fclose(file_);
  }
}

void PlyReadBuffer::after_header(bool is_binary)
{
  is_binary_ = is_binary;
}

Span<char> PlyReadBuffer::read_line()
{
  if (is_binary_) {
    throw std::runtime_error("PLY read_line should not be used in binary mode");
  }
  if (pos_ >= last_newline_) {
    refill_buffer();
  }
  BLI_assert(last_newline_ <= buffer_.size());
  int res_begin = pos_;
  while (pos_ < last_newline_ && !is_newline(buffer_[pos_])) {
    pos_++;
  }
  int res_end = pos_;
  /* Remove possible trailing CR from the result. */
  if (res_end > res_begin && buffer_[res_end - 1] == '\r') {
    --res_end;
  }
  /* Move cursor past newline. */
  if (pos_ < buf_used_ && is_newline(buffer_[pos_])) {
    pos_++;
  }
  return Span<char>(buffer_.data() + res_begin, res_end - res_begin);
}

bool PlyReadBuffer::read_bytes(void *dst, size_t size)
{
  while (size > 0) {
    if (pos_ + size > buf_used_) {
      if (!refill_buffer()) {
        return false;
      }
    }
    int to_copy = int(size);
    if (to_copy > buf_used_) {
      to_copy = buf_used_;
    }
    memcpy(dst, buffer_.data() + pos_, to_copy);
    pos_ += to_copy;
    dst = (char *)dst + to_copy;
    size -= to_copy;
  }
  return true;
}

bool PlyReadBuffer::refill_buffer()
{
  BLI_assert(pos_ <= buf_used_);
  BLI_assert(pos_ <= buffer_.size());
  BLI_assert(buf_used_ <= buffer_.size());

  if (file_ == nullptr || at_eof_) {
    return false; /* File is fully read. */
  }

  /* Move any leftover to start of buffer. */
  int keep = buf_used_ - pos_;
  if (keep > 0) {
    memmove(buffer_.data(), buffer_.data() + pos_, keep);
  }
  /* Read in data from the file. */
  size_t read = fread(buffer_.data() + keep, 1, read_buffer_size_ - keep, file_) + keep;
  at_eof_ = read < read_buffer_size_;
  pos_ = 0;
  buf_used_ = int(read);

  /* Skip past newlines at the front of the buffer and find last newline. */
  if (!is_binary_) {
    while (pos_ < buf_used_ && is_newline(buffer_[pos_])) {
      pos_++;
    }

    int last_nl = buf_used_;
    if (!at_eof_) {
      while (last_nl > 0) {
        --last_nl;
        if (is_newline(buffer_[last_nl])) {
          break;
        }
      }
      if (!is_newline(buffer_[last_nl])) {
        /* Whole line did not fit into our read buffer. */
        throw std::runtime_error("PLY text line did not fit into the read buffer");
      }
    }
    last_newline_ = last_nl;
  }

  return true;
}

}  // namespace blender::io::ply
