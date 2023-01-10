/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Functions for iterating mesh features.
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "MEM_guardedalloc.h"

/* General note on iterating verts/loops/edges/polys and end mode.
 *
 * The edit mesh pointer is set for both final and cage meshes in both cases when there are
 * modifiers applied and not. This helps consistency of checks in the draw manager, where the
 * existence of the edit mesh pointer does not depend on object configuration.
 *
 * For the iterating, however, we need to follow the `CD_ORIGINDEX` code paths when there are
 * modifiers applied on the cage. In the code terms it means that the check for the edit mode code
 * path needs to consist of both edit mesh and edit data checks. */

void BKE_mesh_foreach_mapped_vert(
    Mesh *mesh,
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
    if (mesh->runtime->edit_data->vertexCos != nullptr) {
      const float(*vertexCos)[3] = mesh->runtime->edit_data->vertexCos;
      const float(*vertexNos)[3];
      if (flag & MESH_FOREACH_USE_NORMAL) {
        BKE_editmesh_cache_ensure_vert_normals(em, mesh->runtime->edit_data);
        vertexNos = mesh->runtime->edit_data->vertexNos;
      }
      else {
        vertexNos = nullptr;
      }
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? vertexNos[i] : nullptr;
        func(userData, i, vertexCos[i], no);
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
    const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
    const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX));
    const float(*vert_normals)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                        BKE_mesh_vertex_normals_ensure(mesh) :
                                        nullptr;

    if (index) {
      for (int i = 0; i < mesh->totvert; i++) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[i] : nullptr;
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        func(userData, orig, positions[i], no);
      }
    }
    else {
      for (int i = 0; i < mesh->totvert; i++) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[i] : nullptr;
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
    if (mesh->runtime->edit_data->vertexCos != nullptr) {
      const float(*vertexCos)[3] = mesh->runtime->edit_data->vertexCos;
      BM_mesh_elem_index_ensure(bm, BM_VERT);

      BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
        func(userData,
             i,
             vertexCos[BM_elem_index_get(eed->v1)],
             vertexCos[BM_elem_index_get(eed->v2)]);
      }
    }
    else {
      BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
        func(userData, i, eed->v1->co, eed->v2->co);
      }
    }
  }
  else {
    const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
    const MEdge *med = BKE_mesh_edges(mesh);
    const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->edata, CD_ORIGINDEX));

    if (index) {
      for (int i = 0; i < mesh->totedge; i++, med++) {
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        func(userData, orig, positions[med->v1], positions[med->v2]);
      }
    }
    else if (mesh->totedge == tot_edges) {
      for (int i = 0; i < mesh->totedge; i++, med++) {
        func(userData, i, positions[med->v1], positions[med->v2]);
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

    const float(*vertexCos)[3] = mesh->runtime->edit_data->vertexCos;

    /* XXX: investigate using EditMesh data. */
    const float(*loop_normals)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                        static_cast<const float(*)[3]>(
                                            CustomData_get_layer(&mesh->ldata, CD_NORMAL)) :
                                        nullptr;

    int f_idx;

    BM_mesh_elem_index_ensure(bm, BM_VERT);

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, f_idx) {
      BMLoop *l_iter, *l_first;

      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        const BMVert *eve = l_iter->v;
        const int v_idx = BM_elem_index_get(eve);
        const float *no = loop_normals ? *loop_normals++ : nullptr;
        func(userData, v_idx, f_idx, vertexCos ? vertexCos[v_idx] : eve->co, no);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    const float(*loop_normals)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                        static_cast<const float(*)[3]>(
                                            CustomData_get_layer(&mesh->ldata, CD_NORMAL)) :
                                        nullptr;

    const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
    const MLoop *ml = BKE_mesh_loops(mesh);
    const MPoly *mp = BKE_mesh_polys(mesh);
    const int *v_index = static_cast<const int *>(
        CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX));
    const int *f_index = static_cast<const int *>(
        CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX));
    int p_idx, i;

    if (v_index || f_index) {
      for (p_idx = 0; p_idx < mesh->totpoly; p_idx++, mp++) {
        for (i = 0; i < mp->totloop; i++, ml++) {
          const int v_idx = v_index ? v_index[ml->v] : ml->v;
          const int f_idx = f_index ? f_index[p_idx] : p_idx;
          const float *no = loop_normals ? *loop_normals++ : nullptr;
          if (ELEM(ORIGINDEX_NONE, v_idx, f_idx)) {
            continue;
          }
          func(userData, v_idx, f_idx, positions[ml->v], no);
        }
      }
    }
    else {
      for (p_idx = 0; p_idx < mesh->totpoly; p_idx++, mp++) {
        for (i = 0; i < mp->totloop; i++, ml++) {
          const int v_idx = ml->v;
          const int f_idx = p_idx;
          const float *no = loop_normals ? *loop_normals++ : nullptr;
          func(userData, v_idx, f_idx, positions[ml->v], no);
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
  if (mesh->edit_mesh != nullptr && mesh->runtime->edit_data != nullptr) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    const float(*polyCos)[3];
    const float(*polyNos)[3];
    BMFace *efa;
    BMIter iter;
    int i;

    BKE_editmesh_cache_ensure_poly_centers(em, mesh->runtime->edit_data);
    polyCos = mesh->runtime->edit_data->polyCos; /* always set */

    if (flag & MESH_FOREACH_USE_NORMAL) {
      BKE_editmesh_cache_ensure_poly_normals(em, mesh->runtime->edit_data);
      polyNos = mesh->runtime->edit_data->polyNos; /* maybe nullptr */
    }
    else {
      polyNos = nullptr;
    }

    if (polyNos) {
      BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
        const float *no = polyNos[i];
        func(userData, i, polyCos[i], no);
      }
    }
    else {
      BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? efa->no : nullptr;
        func(userData, i, polyCos[i], no);
      }
    }
  }
  else {
    const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
    const MPoly *mp = BKE_mesh_polys(mesh);
    const MLoop *loops = BKE_mesh_loops(mesh);
    const MLoop *ml;
    float _no_buf[3];
    float *no = (flag & MESH_FOREACH_USE_NORMAL) ? _no_buf : nullptr;
    const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX));

    if (index) {
      for (int i = 0; i < mesh->totpoly; i++, mp++) {
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        float cent[3];
        ml = &loops[mp->loopstart];
        BKE_mesh_calc_poly_center(mp, ml, positions, cent);
        if (flag & MESH_FOREACH_USE_NORMAL) {
          BKE_mesh_calc_poly_normal(mp, ml, positions, no);
        }
        func(userData, orig, cent, no);
      }
    }
    else {
      for (int i = 0; i < mesh->totpoly; i++, mp++) {
        float cent[3];
        ml = &loops[mp->loopstart];
        BKE_mesh_calc_poly_center(mp, ml, positions, cent);
        if (flag & MESH_FOREACH_USE_NORMAL) {
          BKE_mesh_calc_poly_normal(mp, ml, positions, no);
        }
        func(userData, i, cent, no);
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
  const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
  const MPoly *mp = BKE_mesh_polys(mesh);
  const MLoop *loops = BKE_mesh_loops(mesh);
  const MLoop *ml;
  const float(*vert_normals)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                      BKE_mesh_vertex_normals_ensure(mesh) :
                                      nullptr;
  const int *index = static_cast<const int *>(CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX));
  const BLI_bitmap *facedot_tags = mesh->runtime->subsurf_face_dot_tags;
  BLI_assert(facedot_tags != nullptr);

  if (index) {
    for (int i = 0; i < mesh->totpoly; i++, mp++) {
      const int orig = *index++;
      if (orig == ORIGINDEX_NONE) {
        continue;
      }
      ml = &loops[mp->loopstart];
      for (int j = 0; j < mp->totloop; j++, ml++) {
        if (BLI_BITMAP_TEST(facedot_tags, ml->v)) {
          func(userData,
               orig,
               positions[ml->v],
               (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[ml->v] : nullptr);
        }
      }
    }
  }
  else {
    for (int i = 0; i < mesh->totpoly; i++, mp++) {
      ml = &loops[mp->loopstart];
      for (int j = 0; j < mp->totloop; j++, ml++) {
        if (BLI_BITMAP_TEST(facedot_tags, ml->v)) {
          func(userData,
               i,
               positions[ml->v],
               (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[ml->v] : nullptr);
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

void BKE_mesh_foreach_mapped_vert_coords_get(Mesh *me_eval, float (*r_cos)[3], const int totcos)
{
  MappedVCosData user_data;
  memset(r_cos, 0, sizeof(*r_cos) * totcos);
  user_data.vertexcos = r_cos;
  user_data.vertex_visit = BLI_BITMAP_NEW(totcos, __func__);
  BKE_mesh_foreach_mapped_vert(me_eval, get_vertexcos__mapFunc, &user_data, MESH_FOREACH_NOP);
  MEM_freeN(user_data.vertex_visit);
}
