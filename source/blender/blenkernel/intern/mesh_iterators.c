/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

void BKE_mesh_foreach_mapped_vert(
    Mesh *mesh,
    void (*func)(void *userData, int index, const float co[3], const float no[3]),
    void *userData,
    MeshForeachFlag flag)
{
  if (mesh->edit_mesh != NULL) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;
    BMVert *eve;
    int i;
    if (mesh->runtime.edit_data != NULL && mesh->runtime.edit_data->vertexCos != NULL) {
      const float(*vertexCos)[3] = mesh->runtime.edit_data->vertexCos;
      const float(*vertexNos)[3];
      if (flag & MESH_FOREACH_USE_NORMAL) {
        BKE_editmesh_cache_ensure_vert_normals(em, mesh->runtime.edit_data);
        vertexNos = mesh->runtime.edit_data->vertexNos;
      }
      else {
        vertexNos = NULL;
      }
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? vertexNos[i] : NULL;
        func(userData, i, vertexCos[i], no);
      }
    }
    else {
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? eve->no : NULL;
        func(userData, i, eve->co, no);
      }
    }
  }
  else {
    const MVert *mv = mesh->mvert;
    const int *index = CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX);
    const float(*vert_normals)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                        BKE_mesh_vertex_normals_ensure(mesh) :
                                        NULL;

    if (index) {
      for (int i = 0; i < mesh->totvert; i++, mv++) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[i] : NULL;
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        func(userData, orig, mv->co, no);
      }
    }
    else {
      for (int i = 0; i < mesh->totvert; i++, mv++) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[i] : NULL;
        func(userData, i, mv->co, no);
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
  if (mesh->edit_mesh != NULL) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;
    BMEdge *eed;
    int i;
    if (mesh->runtime.edit_data != NULL && mesh->runtime.edit_data->vertexCos != NULL) {
      const float(*vertexCos)[3] = mesh->runtime.edit_data->vertexCos;
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
    const MVert *mv = mesh->mvert;
    const MEdge *med = mesh->medge;
    const int *index = CustomData_get_layer(&mesh->edata, CD_ORIGINDEX);

    if (index) {
      for (int i = 0; i < mesh->totedge; i++, med++) {
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        func(userData, orig, mv[med->v1].co, mv[med->v2].co);
      }
    }
    else if (mesh->totedge == tot_edges) {
      for (int i = 0; i < mesh->totedge; i++, med++) {
        func(userData, i, mv[med->v1].co, mv[med->v2].co);
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

  /* We can't use dm->getLoopDataLayout(dm) here,
   * we want to always access dm->loopData, EditDerivedBMesh would
   * return loop data from bmesh itself. */
  if (mesh->edit_mesh != NULL) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;
    BMFace *efa;

    const float(*vertexCos)[3] = mesh->runtime.edit_data ? mesh->runtime.edit_data->vertexCos :
                                                           NULL;

    /* XXX: investigate using EditMesh data. */
    const float(*lnors)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                 CustomData_get_layer(&mesh->ldata, CD_NORMAL) :
                                 NULL;

    int f_idx;

    BM_mesh_elem_index_ensure(bm, BM_VERT);

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, f_idx) {
      BMLoop *l_iter, *l_first;

      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        const BMVert *eve = l_iter->v;
        const int v_idx = BM_elem_index_get(eve);
        const float *no = lnors ? *lnors++ : NULL;
        func(userData, v_idx, f_idx, vertexCos ? vertexCos[v_idx] : eve->co, no);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    const float(*lnors)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                 CustomData_get_layer(&mesh->ldata, CD_NORMAL) :
                                 NULL;

    const MVert *mv = mesh->mvert;
    const MLoop *ml = mesh->mloop;
    const MPoly *mp = mesh->mpoly;
    const int *v_index = CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX);
    const int *f_index = CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX);
    int p_idx, i;

    if (v_index || f_index) {
      for (p_idx = 0; p_idx < mesh->totpoly; p_idx++, mp++) {
        for (i = 0; i < mp->totloop; i++, ml++) {
          const int v_idx = v_index ? v_index[ml->v] : ml->v;
          const int f_idx = f_index ? f_index[p_idx] : p_idx;
          const float *no = lnors ? *lnors++ : NULL;
          if (ELEM(ORIGINDEX_NONE, v_idx, f_idx)) {
            continue;
          }
          func(userData, v_idx, f_idx, mv[ml->v].co, no);
        }
      }
    }
    else {
      for (p_idx = 0; p_idx < mesh->totpoly; p_idx++, mp++) {
        for (i = 0; i < mp->totloop; i++, ml++) {
          const int v_idx = ml->v;
          const int f_idx = p_idx;
          const float *no = lnors ? *lnors++ : NULL;
          func(userData, v_idx, f_idx, mv[ml->v].co, no);
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
  if (mesh->edit_mesh != NULL && mesh->runtime.edit_data != NULL) {
    BMEditMesh *em = mesh->edit_mesh;
    BMesh *bm = em->bm;
    const float(*polyCos)[3];
    const float(*polyNos)[3];
    BMFace *efa;
    BMIter iter;
    int i;

    BKE_editmesh_cache_ensure_poly_centers(em, mesh->runtime.edit_data);
    polyCos = mesh->runtime.edit_data->polyCos; /* always set */

    if (flag & MESH_FOREACH_USE_NORMAL) {
      BKE_editmesh_cache_ensure_poly_normals(em, mesh->runtime.edit_data);
      polyNos = mesh->runtime.edit_data->polyNos; /* maybe NULL */
    }
    else {
      polyNos = NULL;
    }

    if (polyNos) {
      BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
        const float *no = polyNos[i];
        func(userData, i, polyCos[i], no);
      }
    }
    else {
      BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
        const float *no = (flag & MESH_FOREACH_USE_NORMAL) ? efa->no : NULL;
        func(userData, i, polyCos[i], no);
      }
    }
  }
  else {
    const MVert *mvert = mesh->mvert;
    const MPoly *mp = mesh->mpoly;
    const MLoop *ml;
    float _no_buf[3];
    float *no = (flag & MESH_FOREACH_USE_NORMAL) ? _no_buf : NULL;
    const int *index = CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX);

    if (index) {
      for (int i = 0; i < mesh->totpoly; i++, mp++) {
        const int orig = *index++;
        if (orig == ORIGINDEX_NONE) {
          continue;
        }
        float cent[3];
        ml = &mesh->mloop[mp->loopstart];
        BKE_mesh_calc_poly_center(mp, ml, mvert, cent);
        if (flag & MESH_FOREACH_USE_NORMAL) {
          BKE_mesh_calc_poly_normal(mp, ml, mvert, no);
        }
        func(userData, orig, cent, no);
      }
    }
    else {
      for (int i = 0; i < mesh->totpoly; i++, mp++) {
        float cent[3];
        ml = &mesh->mloop[mp->loopstart];
        BKE_mesh_calc_poly_center(mp, ml, mvert, cent);
        if (flag & MESH_FOREACH_USE_NORMAL) {
          BKE_mesh_calc_poly_normal(mp, ml, mvert, no);
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
  const MPoly *mp = mesh->mpoly;
  const MLoop *ml;
  const MVert *mv;
  const float(*vert_normals)[3] = (flag & MESH_FOREACH_USE_NORMAL) ?
                                      BKE_mesh_vertex_normals_ensure(mesh) :
                                      NULL;
  const int *index = CustomData_get_layer(&mesh->pdata, CD_ORIGINDEX);

  if (index) {
    for (int i = 0; i < mesh->totpoly; i++, mp++) {
      const int orig = *index++;
      if (orig == ORIGINDEX_NONE) {
        continue;
      }
      ml = &mesh->mloop[mp->loopstart];
      for (int j = 0; j < mp->totloop; j++, ml++) {
        mv = &mesh->mvert[ml->v];
        if (mv->flag & ME_VERT_FACEDOT) {

          func(userData,
               orig,
               mv->co,
               (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[ml->v] : NULL);
        }
      }
    }
  }
  else {
    for (int i = 0; i < mesh->totpoly; i++, mp++) {
      ml = &mesh->mloop[mp->loopstart];
      for (int j = 0; j < mp->totloop; j++, ml++) {
        mv = &mesh->mvert[ml->v];
        if (mv->flag & ME_VERT_FACEDOT) {
          func(userData, i, mv->co, (flag & MESH_FOREACH_USE_NORMAL) ? vert_normals[ml->v] : NULL);
        }
      }
    }
  }
}

/* Helpers based on above foreach loopers> */

typedef struct MappedVCosData {
  float (*vertexcos)[3];
  BLI_bitmap *vertex_visit;
} MappedVCosData;

static void get_vertexcos__mapFunc(void *user_data,
                                   int index,
                                   const float co[3],
                                   const float UNUSED(no[3]))
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
