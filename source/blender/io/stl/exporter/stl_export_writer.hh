/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include <cstdio>

namespace blender::io::stl {

struct PackedTriangle;

class FileWriter {
 public:
  FileWriter(const char *filepath, bool ascii);
  ~FileWriter();
  void write_triangle(const PackedTriangle &data);

 private:
  FILE *file_;
  uint32_t tris_num_;
  bool ascii_;
};

}  // namespace blender::io::stl
