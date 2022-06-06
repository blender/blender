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

STLMeshHelper::STLMeshHelper(int num_tris, bool use_custom_normals)
    : m_use_custom_normals(use_custom_normals)
{
  m_num_degenerate_tris = 0;
  m_num_duplicate_tris = 0;
  m_tris.reserve(num_tris);
  /* Upper bound (all vertices are unique). */
  m_verts.reserve(num_tris * 3);
  if (use_custom_normals) {
    m_loop_normals.reserve(num_tris * 3);
  }
}

bool STLMeshHelper::add_triangle(const float3 &a, const float3 &b, const float3 &c)
{
  int v1_id = m_verts.index_of_or_add(a);
  int v2_id = m_verts.index_of_or_add(b);
  int v3_id = m_verts.index_of_or_add(c);
  if ((v1_id == v2_id) || (v1_id == v3_id) || (v2_id == v3_id)) {
    m_num_degenerate_tris++;
    return false;
  }
  if (!m_tris.add({v1_id, v2_id, v3_id})) {
    m_num_duplicate_tris++;
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
    m_loop_normals.append_n_times(custom_normal, 3);
  }
}

Mesh *STLMeshHelper::to_mesh(Main *bmain, char *mesh_name)
{
  if (m_num_degenerate_tris > 0) {
    std::cout << "STL Importer: " << m_num_degenerate_tris << "degenerate triangles were removed"
              << std::endl;
  }
  if (m_num_duplicate_tris > 0) {
    std::cout << "STL Importer: " << m_num_duplicate_tris << "duplicate triangles were removed"
              << std::endl;
  }

  Mesh *mesh = BKE_mesh_add(bmain, mesh_name);
  /* User count is already 1 here, but will be set later in #BKE_mesh_assign_object. */
  id_us_min(&mesh->id);

  mesh->totvert = m_verts.size();
  mesh->mvert = static_cast<MVert *>(
      CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_CALLOC, nullptr, mesh->totvert));
  for (int i = 0; i < mesh->totvert; i++) {
    copy_v3_v3(mesh->mvert[i].co, m_verts[i]);
  }

  mesh->totpoly = m_tris.size();
  mesh->totloop = m_tris.size() * 3;
  mesh->mpoly = static_cast<MPoly *>(
      CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_CALLOC, nullptr, mesh->totpoly));
  mesh->mloop = static_cast<MLoop *>(
      CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_CALLOC, nullptr, mesh->totloop));

  threading::parallel_for(m_tris.index_range(), 2048, [&](IndexRange tris_range) {
    for (const int i : tris_range) {
      mesh->mpoly[i].loopstart = 3 * i;
      mesh->mpoly[i].totloop = 3;

      mesh->mloop[3 * i].v = m_tris[i].v1;
      mesh->mloop[3 * i + 1].v = m_tris[i].v2;
      mesh->mloop[3 * i + 2].v = m_tris[i].v3;
    }
  });

  /* NOTE: edges must be calculated first before setting custom normals. */
  BKE_mesh_calc_edges(mesh, false, false);

  if (m_use_custom_normals && m_loop_normals.size() == mesh->totloop) {
    BKE_mesh_set_custom_normals(mesh, reinterpret_cast<float(*)[3]>(m_loop_normals.data()));
    mesh->flag |= ME_AUTOSMOOTH;
  }

  return mesh;
}

}  // namespace blender::io::stl
