/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include <iostream>

#include "BKE_mesh.hh"

#include "BLI_array_utils.hh"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"

#include "stl_data.hh"
#include "stl_import_mesh.hh"

namespace blender::io::stl {

STLMeshHelper::STLMeshHelper(int tris_num, bool use_custom_normals)
    : use_custom_normals_(use_custom_normals)
{
  degenerate_tris_num_ = 0;
  duplicate_tris_num_ = 0;
  tris_.reserve(tris_num);
  /* Upper bound (all vertices are unique). */
  verts_.reserve(tris_num * 3);
  if (use_custom_normals) {
    loop_normals_.reserve(tris_num * 3);
  }
}

bool STLMeshHelper::add_triangle(const PackedTriangle &data)
{
  int v1_id = verts_.index_of_or_add(data.vertices[0]);
  int v2_id = verts_.index_of_or_add(data.vertices[1]);
  int v3_id = verts_.index_of_or_add(data.vertices[2]);
  if ((v1_id == v2_id) || (v1_id == v3_id) || (v2_id == v3_id)) {
    degenerate_tris_num_++;
    return false;
  }
  if (!tris_.add({v1_id, v2_id, v3_id})) {
    duplicate_tris_num_++;
    return false;
  }

  if (use_custom_normals_) {
    loop_normals_.append_n_times(data.normal, 3);
  }
  return true;
}

Mesh *STLMeshHelper::to_mesh()
{
  if (degenerate_tris_num_ > 0) {
    std::cout << "STL Importer: " << degenerate_tris_num_ << " degenerate triangles were removed"
              << std::endl;
  }
  if (duplicate_tris_num_ > 0) {
    std::cout << "STL Importer: " << duplicate_tris_num_ << " duplicate triangles were removed"
              << std::endl;
  }

  Mesh *mesh = BKE_mesh_new_nomain(verts_.size(), 0, tris_.size(), tris_.size() * 3);
  mesh->vert_positions_for_write().copy_from(verts_);
  offset_indices::fill_constant_group_size(3, 0, mesh->face_offsets_for_write());
  array_utils::copy(tris_.as_span().cast<int>(), mesh->corner_verts_for_write());

  /* NOTE: edges must be calculated first before setting custom normals. */
  bke::mesh_calc_edges(*mesh, false, false);

  if (use_custom_normals_ && loop_normals_.size() == mesh->corners_num) {
    BKE_mesh_set_custom_normals(mesh, reinterpret_cast<float(*)[3]>(loop_normals_.data()));
  }

  return mesh;
}

}  // namespace blender::io::stl
