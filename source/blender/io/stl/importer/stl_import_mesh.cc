/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"

#include "BLI_array.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

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

bool STLMeshHelper::add_triangle(const float3 &a, const float3 &b, const float3 &c)
{
  int v1_id = verts_.index_of_or_add(a);
  int v2_id = verts_.index_of_or_add(b);
  int v3_id = verts_.index_of_or_add(c);
  if ((v1_id == v2_id) || (v1_id == v3_id) || (v2_id == v3_id)) {
    degenerate_tris_num_++;
    return false;
  }
  if (!tris_.add({v1_id, v2_id, v3_id})) {
    duplicate_tris_num_++;
    return false;
  }
  return true;
}

void STLMeshHelper::add_triangle(const float3 &a,
                                 const float3 &b,
                                 const float3 &c,
                                 const float3 &custom_normal)
{
  if (add_triangle(a, b, c)) {
    loop_normals_.append_n_times(custom_normal, 3);
  }
}

Mesh *STLMeshHelper::to_mesh(Main *bmain, char *mesh_name)
{
  if (degenerate_tris_num_ > 0) {
    std::cout << "STL Importer: " << degenerate_tris_num_ << " degenerate triangles were removed"
              << std::endl;
  }
  if (duplicate_tris_num_ > 0) {
    std::cout << "STL Importer: " << duplicate_tris_num_ << " duplicate triangles were removed"
              << std::endl;
  }

  Mesh *mesh = BKE_mesh_add(bmain, mesh_name);
  /* User count is already 1 here, but will be set later in #BKE_mesh_assign_object. */
  id_us_min(&mesh->id);

  mesh->totvert = verts_.size();
  CustomData_add_layer_named(
      &mesh->vdata, CD_PROP_FLOAT3, CD_CONSTRUCT, nullptr, mesh->totvert, "position");
  mesh->vert_positions_for_write().copy_from(verts_);

  mesh->totpoly = tris_.size();
  mesh->totloop = tris_.size() * 3;
  CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, mesh->totpoly);
  CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, mesh->totloop);
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();
  threading::parallel_for(tris_.index_range(), 2048, [&](IndexRange tris_range) {
    for (const int i : tris_range) {
      polys[i].loopstart = 3 * i;
      polys[i].totloop = 3;

      loops[3 * i].v = tris_[i].v1;
      loops[3 * i + 1].v = tris_[i].v2;
      loops[3 * i + 2].v = tris_[i].v3;
    }
  });

  /* NOTE: edges must be calculated first before setting custom normals. */
  BKE_mesh_calc_edges(mesh, false, false);

  if (use_custom_normals_ && loop_normals_.size() == mesh->totloop) {
    BKE_mesh_set_custom_normals(mesh, reinterpret_cast<float(*)[3]>(loop_normals_.data()));
    mesh->flag |= ME_AUTOSMOOTH;
  }

  return mesh;
}

}  // namespace blender::io::stl
