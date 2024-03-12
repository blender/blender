/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include <cstdio>
#include <system_error>

#include "BLI_fileops.hh"
#include "BLI_memory_utils.hh"

#include "DNA_mesh_types.h"

/* NOTE: we could use C++17 <charconv> from_chars to parse
 * floats, but even if some compilers claim full support,
 * their standard libraries are not quite there yet.
 * LLVM/libc++ only has a float parser since LLVM 14,
 * and gcc/libstdc++ since 11.1. So until at least these are
 * the minimum spec, use an external library. */
#include "fast_float.h"

#include "stl_import_ascii_reader.hh"
#include "stl_import_mesh.hh"

namespace blender::io::stl {

class StringBuffer {
 private:
  char *start;
  const char *end;

 public:
  StringBuffer(char *buf, size_t len)
  {
    start = buf;
    end = start + len;
  }

  bool is_empty() const
  {
    return start == end;
  }

  void drop_leading_control_chars()
  {
    while ((start < end) && (*start) <= ' ') {
      start++;
    }
  }

  void drop_leading_non_control_chars()
  {
    while ((start < end) && (*start) > ' ') {
      start++;
    }
  }

  void drop_line()
  {
    while (start < end && *start != '\n') {
      start++;
    }
  }

  bool parse_token(const char *token, size_t token_length)
  {
    drop_leading_control_chars();
    if (end - start < token_length + 1) {
      return false;
    }
    if (memcmp(start, token, token_length) != 0) {
      return false;
    }
    if (start[token_length] > ' ') {
      return false;
    }
    start += token_length + 1;
    return true;
  }

  void drop_token()
  {
    drop_leading_non_control_chars();
    drop_leading_control_chars();
  }

  void parse_float(float &out)
  {
    drop_leading_control_chars();
    /* Skip '+' */
    if (start < end && *start == '+') {
      start++;
    }
    fast_float::from_chars_result res = fast_float::from_chars(start, end, out);
    if (ELEM(res.ec, std::errc::invalid_argument, std::errc::result_out_of_range)) {
      out = 0.0f;
    }
    start = const_cast<char *>(res.ptr);
  }
};

static inline void parse_float3(StringBuffer &buf, float out[3])
{
  for (int i = 0; i < 3; i++) {
    buf.parse_float(out[i]);
  }
}

Mesh *read_stl_ascii(const char *filepath, const bool use_custom_normals)
{
  size_t buffer_len;
  void *buffer = BLI_file_read_text_as_mem(filepath, 0, &buffer_len);
  if (buffer == nullptr) {
    fprintf(stderr, "STL Importer: cannot read from ASCII STL file: '%s'\n", filepath);
    return nullptr;
  }
  BLI_SCOPED_DEFER([&]() { MEM_freeN(buffer); });

  int num_reserved_tris = 1024;

  StringBuffer str_buf(static_cast<char *>(buffer), buffer_len);
  STLMeshHelper stl_mesh(num_reserved_tris, use_custom_normals);
  float triangle_buf[3][3];
  float custom_normal_buf[3];
  str_buf.drop_line(); /* Skip header line */
  while (!str_buf.is_empty()) {
    if (str_buf.parse_token("vertex", 6)) {
      parse_float3(str_buf, triangle_buf[0]);
      if (str_buf.parse_token("vertex", 6)) {
        parse_float3(str_buf, triangle_buf[1]);
      }
      if (str_buf.parse_token("vertex", 6)) {
        parse_float3(str_buf, triangle_buf[2]);
      }
      if (use_custom_normals) {
        stl_mesh.add_triangle(
            triangle_buf[0], triangle_buf[1], triangle_buf[2], custom_normal_buf);
      }
      else {
        stl_mesh.add_triangle(triangle_buf[0], triangle_buf[1], triangle_buf[2]);
      }
    }
    else if (str_buf.parse_token("facet", 5)) {
      str_buf.drop_token(); /* Expecting "normal" */
      parse_float3(str_buf, custom_normal_buf);
    }
    else {
      str_buf.drop_token();
    }
  }

  return stl_mesh.to_mesh();
}

}  // namespace blender::io::stl
