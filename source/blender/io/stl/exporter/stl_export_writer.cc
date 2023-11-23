/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include <cstdint>
#include <cstdio>
#include <stdexcept>

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#include <fmt/format.h>

#include "stl_export_writer.hh"

#include "BLI_fileops.h"

namespace blender::io::stl {

constexpr size_t BINARY_HEADER_SIZE = 80;

#pragma pack(push, 1)
struct ExportBinaryTriangle {
  float3 normal;
  float3 vertices[3];
  uint16_t attribute_byte_count;
};
#pragma pack(pop)
static_assert(sizeof(ExportBinaryTriangle) == 12 + 12 * 3 + 2,
              "ExportBinaryTriangle expected size mismatch");

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
    char header[BINARY_HEADER_SIZE] = {};
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

void FileWriter::write_triangle(const Triangle &t)
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

               t.normal.x,
               t.normal.y,
               t.normal.z,
               t.vertices[0].x,
               t.vertices[0].y,
               t.vertices[0].z,
               t.vertices[1].x,
               t.vertices[1].y,
               t.vertices[1].z,
               t.vertices[2].x,
               t.vertices[2].y,
               t.vertices[2].z);
  }
  else {
    ExportBinaryTriangle bin_tri;
    bin_tri.normal = t.normal;
    bin_tri.vertices[0] = t.vertices[0];
    bin_tri.vertices[1] = t.vertices[1];
    bin_tri.vertices[2] = t.vertices[2];
    bin_tri.attribute_byte_count = 0;
    fwrite(&bin_tri, sizeof(ExportBinaryTriangle), 1, file_);
  }
}

}  // namespace blender::io::stl
