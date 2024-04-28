/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include <cstdint>
#include <cstdio>

#include "BKE_mesh.hh"

#include "BLI_array.hh"

#include "DNA_mesh_types.h"

#include "stl_data.hh"
#include "stl_import.hh"
#include "stl_import_binary_reader.hh"
#include "stl_import_mesh.hh"

namespace blender::io::stl {

Mesh *read_stl_binary(FILE *file, const bool use_custom_normals)
{
  const int chunk_size = 1024;
  uint32_t num_tris = 0;
  fseek(file, BINARY_HEADER_SIZE, SEEK_SET);
  if (fread(&num_tris, sizeof(uint32_t), 1, file) != 1) {
    stl_import_report_error(file);
    return nullptr;
  }

  if (num_tris == 0) {
    return BKE_mesh_new_nomain(0, 0, 0, 0);
  }

  Array<PackedTriangle> tris_buf(chunk_size);
  STLMeshHelper stl_mesh(num_tris, use_custom_normals);
  size_t num_read_tris;
  while ((num_read_tris = fread(tris_buf.data(), sizeof(PackedTriangle), chunk_size, file))) {
    for (size_t i = 0; i < num_read_tris; i++) {
      stl_mesh.add_triangle(tris_buf[i]);
    }
  }

  return stl_mesh.to_mesh();
}

}  // namespace blender::io::stl
