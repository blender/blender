/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for iterating mesh features.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "MEM_guardedalloc.h"

/* General note on iterating verts/loops/edges/faces and end mode.
 *
 * The edit mesh pointer is set for both final and cage meshes in both cases when there are
 * modifiers applied and not. This helps consistency of checks in the draw manager, where the
 * existence of the edit mesh pointer does not depend on object configuration.
 *
 * For the iterating, however, we need to follow the `CD_ORIGINDEX` code paths when there are
 * modifiers applied on the cage. In the code terms it means that the check for the edit mode code
 * path needs to consist of both edit mesh and edit data checks. */

void BKE_mesh_foreach_mapped_vert(
    const Mesh *mesh,
    void (*func)(void *userData, int index, const float co[3], const float no[3]),
    void *userData,
    MeshForeachFlag flag)
{
  if (mesh->edit_mesh != nullptr && mesh->runtime->edit_data != nullptr) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;
    BMVert *eve;
    int i;
    if (!mesh->runtime->edit_data->vertexCos.is_empty()) {
      const blender::Span<blender::float3> positions = mesh->runtime->edit_data->vertexCos;
      blender::Span<blender::float3> vert_normals;
      if (flag & MESH_FOREACH_USE_NORMAL) {
        BKE_editmesh_cache_ensure_vert_normals(em, mesh->runtime->edit_data);
        vert_normals = mesh->runtime->edit_data->vertexNos;
      }
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? &vert_normals[i].x : nullptr;
        func(userData, i, positions[i], no);
      }
    }
    else {
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? eve->no : nullptr;
        func(userData, i, eve->co, no);
      }
    }
  }
  else {
    const blender::Span<blender::float3> positions = mesh->vert_positions();
    const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX));
    blender::Span<blender::float3> vert_normals;
    if (flag & MESH_FOREACH_USE_NORMAL) {
      vert_normals = mesh->vert_normals();
    }

    if (index) {
      for (int i = 0; i < mesh->totvert; i++) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? &vert_normals[i].x : nullptr;
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        func(userData, orig, positions[i], no);
      }
    }
    else {
      for (int i = 0; i < mesh->totvert; i++) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? &vert_normals[i].x : nullptr;
        func(userData, i, positions[i], no);
      }
    }
  }
}

void BKE_mesh_foreach_mapped_edge(
    Mesh *mesh,
    const int tot_edges,
    void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
    void *userData)
{
  if (mesh->edit_mesh != nullptr && mesh->runtime->edit_data) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;
    BMEdge *eed;
    int i;
    if (!mesh->runtime->edit_data->vertexCos.is_empty()) {
      const blender::Span<blender::float3> positions = mesh->runtime->edit_data->vertexCos;
      BM_mesh_elem_index_ensure(bm, BM_VERT);
      BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
        func(userData,
             i,
             positions[BM_elem_index_get(eed->v1)],
             positions[BM_elem_index_get(eed->v2)]);
      }
    }
    else {
      BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
        func(userData, i, eed->v1->co, eed->v2->co);
      }
    }
  }
  else {
    const blender::Span<blender::float3> positions = mesh->vert_positions();
    const blender::Span<blender::int2> edges = mesh->edges();
    const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->edata, CD_ORIGINDEX));

    if (index) {
      for (const int i : edges.index_range()) {

        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        func(userData, orig, positions[edges[i][0]], positions[edges[i][1]]);
      }
    }
    else if (mesh->totedge == tot_edges) {
      for (const int i : edges.index_range()) {
        func(userData, i, positions[edges[i][0]], positions[edges[i][1]]);
      }
    }
  }
}

void BKE_mesh_foreach_mapped_loop(Mesh *mesh,
                                  void (*func)(void *userData,
                                               int vertex_index,
                                               int face_index,
                                               const float co[3],
                                               const float no[3]),
                                  void *userData,
                                  MeshForeachFlag flag)
{

  /* We can't use `dm->getLoopDataLayout(dm)` here,
   * we want to always access `dm->loopData`, `EditDerivedBMesh` would
   * return loop data from BMesh itself. */
  if (mesh->edit_mesh != nullptr && mesh->runtime->edit_data) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;
    BMFace *efa;

    const blender::Span<blender::float3> positions = mesh->runtime->edit_data->vertexCos;

    /* XXX: investigate using EditMesh data. */
    blender::Span<blender::float3> corner_normals;
    if (flag & MESH_FOREACH_USE_NORMAL) {
      corner_normals = {
          static_cast<const blender::float3 *>(CustomData_get_layer(&mesh->ldata, CD_NORMAL)),
          mesh->totloop};
    }

    int f_idx;

    BM_mesh_elem_index_ensure(bm, BM_VERT);

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, f_idx) {
      BMLoop *l_iter, *l_first;

      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        const BMVert *eve = l_iter->v;
        const int v_idx = BM_elem_index_get(eve);
        func(userData,
             v_idx,
             f_idx,
             positions.is_empty() ? positions[v_idx] : blender::float3(eve->co),
             corner_normals.is_empty() ? nullptr : &corner_normals[BM_elem_index_get(l_iter)].x);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    blender::Span<blender::float3> corner_normals;
    if (flag & MESH_FOREACH_USE_NORMAL) {
      corner_normals = {
          static_cast<const blender::float3 *>(CustomData_get_layer(&mesh->ldata, CD_NORMAL)),
          mesh->totloop};
    }

    const blender::Span<blender::float3> positions = mesh->vert_positions();
    const blender::OffsetIndices faces = mesh->faces();
    const blender::Span<int> corner_verts = mesh->corner_verts();
    const int *v_index = static_cast<const int *>(
        CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX));
    const int *f_index = static_cast<const int *>(
        CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX));

    if (v_index || f_index) {
      for (const int face_i : faces.index_range()) {
        for (const int corner : faces[face_i]) {
          const int vert = corner_verts[corner];
          const int v_idx = v_index ? v_index[vert] : vert;
          const int f_idx = f_index ? f_index[face_i] : face_i;
          const float *no = corner_normals.is_empty() ? nullptr : &corner_normals[corner].x;
          if (ELEM(ORIGINDEX_NONE, v_idx, f_idx)) {
            continue;
          }
          func(userData, v_idx, f_idx, positions[vert], no);
        }
      }
    }
    else {
      for (const int face_i : faces.index_range()) {
        for (const int corner : faces[face_i]) {
          const int vert = corner_verts[corner];
          const float *no = corner_normals.is_empty() ? nullptr : &corner_normals[corner].x;
          func(userData, vert, face_i, positions[vert], no);
        }
      }
    }
  }
}

void BKE_mesh_foreach_mapped_face_center(
    Mesh *mesh,
    void (*func)(void *userData, int index, const float cent[3], const float no[3]),
    void *userData,
    MeshForeachFlag flag)
{
  using namespace blender;
  if (mesh->edit_mesh != nullptr && mesh->runtime->edit_data != nullptr) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    blender::Span<blender::float3> face_centers;
    blender::Span<blender::float3> face_normals;
    BMFace *efa;
    BMIter iter;
    int i;

    BKE_editmesh_cache_ensure_face_centers(em, mesh->runtime->edit_data);
    face_centers = mesh->runtime->edit_data->faceCos; /* always set */

    if (flag & MESH_FOREACH_USE_NORMAL) {
      BKE_editmesh_cache_ensure_face_normals(em, mesh->runtime->edit_data);
      face_normals = mesh->runtime->edit_data->faceNos; /* maybe nullptr */
    }

    if (!face_normals.is_empty()) {
      BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
        const float *no = face_normals[i];
        func(userData, i, face_centers[i], no);
      }
    }
    else {
      BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? efa->no : nullptr;
        func(userData, i, face_centers[i], no);
      }
    }
  }
  else {
    const blender::Span<float3> positions = mesh->vert_positions();
    const blender::OffsetIndices faces = mesh->faces();
    const blender::Span<int> corner_verts = mesh->corner_verts();
    const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX));

    if (index) {
      for (const int i : faces.index_range()) {
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        const Span<int> face_verts = corner_verts.slice(faces[i]);
        const float3 center = bke::mesh::face_center_calc(positions, face_verts);
        if (flag & MESH_FOREACH_USE_NORMAL) {
          const float3 normal = bke::mesh::face_normal_calc(positions, face_verts);
          func(userData, orig, center, normal);
        }
        else {
          func(userData, orig, center, nullptr);
        }
      }
    }
    else {
      for (const int i : faces.index_range()) {
        const Span<int> face_verts = corner_verts.slice(faces[i]);
        const float3 center = bke::mesh::face_center_calc(positions, face_verts);
        if (flag & MESH_FOREACH_USE_NORMAL) {
          const float3 normal = bke::mesh::face_normal_calc(positions, face_verts);
          func(userData, i, center, normal);
        }
        else {
          func(userData, i, center, nullptr);
        }
      }
    }
  }
}

void BKE_mesh_foreach_mapped_subdiv_face_center(
    Mesh *mesh,
    void (*func)(void *userData, int index, const float cent[3], const float no[3]),
    void *userData,
    MeshForeachFlag flag)
{
  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  blender::Span<blender::float3> vert_normals;
  if (flag & MESH_FOREACH_USE_NORMAL) {
    vert_normals = mesh->vert_normals();
  }
  const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX));
  const blender::BitSpan facedot_tags = mesh->runtime->subsurf_face_dot_tags;

  if (index) {
    for (const int i : faces.index_range()) {
      const int orig = *index++;
      if (orig == ORIGINDEX_NONE) {
        continue;
      }
      for (const int vert : corner_verts.slice(faces[i])) {
        if (facedot_tags[vert]) {
          func(userData,
               orig,
               positions[vert],
               (flag & MESH_FOREACH_USE_NORMAL) ? &vert_normals[vert].x : nullptr);
        }
      }
    }
  }
  else {
    for (const int i : faces.index_range()) {
      for (const int vert : corner_verts.slice(faces[i])) {
        if (facedot_tags[vert]) {
          func(userData,
               i,
               positions[vert],
               (flag & MESH_FOREACH_USE_NORMAL) ? &vert_normals[vert].x : nullptr);
        }
      }
    }
  }
}

/* Helpers based on above foreach loopers> */

struct MappedVCosData {
  float (*vertexcos)[3];
  BLI_bitmap *vertex_visit;
};

static void get_vertexcos__mapFunc(void *user_data,
                                   int index,
                                   const float co[3],
                                   const float /*no*/[3])
{
  MappedVCosData *mapped_vcos_data = (MappedVCosData *)user_data;

  if (BLI_BITMAP_TEST(mapped_vcos_data->vertex_visit, index) == 0) {
    /* We need coord from prototype vertex, not from copies,
     * we assume they stored in the beginning of vertex array stored in evaluated mesh
     * (mirror modifier for eg does this). */
    copy_v3_v3(mapped_vcos_data->vertexcos[index], co);
    BLI_BITMAP_ENABLE(mapped_vcos_data->vertex_visit, index);
  }
}

void BKE_mesh_foreach_mapped_vert_coords_get(const Mesh *me_eval,
                                             float (*r_cos)[3],
                                             const int totcos)
{
  MappedVCosData user_data;
  memset(r_cos, 0, sizeof(*r_cos) * totcos);
  user_data.vertexcos = r_cos;
  user_data.vertex_visit = BLI_BITMAP_NEW(totcos, __func__);
  BKE_mesh_foreach_mapped_vert(me_eval, get_vertexcos__mapFunc, &user_data, MESH_FOREACH_NOP);
  MEM_freeN(user_data.vertex_visit);
}
