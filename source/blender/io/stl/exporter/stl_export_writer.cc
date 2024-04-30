/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include <cstdint>
#include <cstdio>
#include <stdexcept>

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#include <fmt/format.h>

#include "stl_data.hh"
#include "stl_export_writer.hh"

#include "BLI_fileops.h"

namespace blender::io::stl {

FileWriter::FileWriter(const char *filepath, bool ascii) : tris_num_(0), ascii_(ascii)
{
  file_ = BLI_fopen(filepath, "wb");
  if (file_ == nullptr) {
    throw std::runtime_error("STL export: failed to open file");
  }

  /* Write header */
  if (ascii_) {
    fmt::print(file_, "solid \n");
  }
  else {
    const char header[BINARY_HEADER_SIZE] = {};
    fwrite(header, 1, BINARY_HEADER_SIZE, file_);
    /* Write placeholder for number of triangles, so that it can be updated later (after all
     * triangles have been written). */
    fwrite(&tris_num_, sizeof(uint32_t), 1, file_);
  }
}

FileWriter::~FileWriter()
{
  if (file_ == nullptr) {
    return;
  }
  if (ascii_) {
    fmt::print(file_, "endsolid \n");
  }
  else {
    fseek(file_, BINARY_HEADER_SIZE, SEEK_SET);
    fwrite(&tris_num_, sizeof(uint32_t), 1, file_);
  }
  fclose(file_);
}

void FileWriter::write_triangle(const PackedTriangle &data)
{
  tris_num_++;
  if (ascii_) {
    fmt::print(file_,
               "facet normal {} {} {}\n"
               " outer loop\n"
               "  vertex {} {} {}\n"
               "  vertex {} {} {}\n"
               "  vertex {} {} {}\n"
               " endloop\n"
               "endfacet\n",

               data.normal.x,
               data.normal.y,
               data.normal.z,
               data.vertices[0].x,
               data.vertices[0].y,
               data.vertices[0].z,
               data.vertices[1].x,
               data.vertices[1].y,
               data.vertices[1].z,
               data.vertices[2].x,
               data.vertices[2].y,
               data.vertices[2].z);
  }
  else {
    fwrite(&data, sizeof(data), 1, file_);
  }
}

}  // namespace blender::io::stl
