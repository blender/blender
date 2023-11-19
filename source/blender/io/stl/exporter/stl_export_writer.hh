/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender::io::stl {

struct Triangle {
  float3 normal;
  float3 vertices[3];
};

class FileWriter {
 public:
  FileWriter(const char *filepath, bool ascii);
  ~FileWriter();
  void write_triangle(const Triangle &t);

 private:
  FILE *file_;
  uint32_t tris_num_;
  bool ascii_;
};

}  // namespace blender::io::stl
