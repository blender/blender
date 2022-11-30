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
#include <fmt/format.h>

namespace blender::io::ply {
class FileBufferAscii : public FileBuffer {
 public:
  FileBufferAscii(const char *filepath, size_t buffer_chunk_size = 64 * 1024)
      : FileBuffer(filepath, buffer_chunk_size)
  {
  }
};
}  // namespace blender::io::ply
