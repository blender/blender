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
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 */

#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "draw_subdivision.h"
#include "extract_mesh.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Point Indices
 * \{ */

static void extract_points_init(const MeshRenderData *mr,
                                struct MeshBatchCache *UNUSED(cache),
                                void *UNUSED(buf),
                                void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->vert_len, mr->loop_len + mr->loop_loose_len);
}

BLI_INLINE void vert_set_bm(GPUIndexBufBuilder *elb, const BMVert *eve, int l_index)
{
  const int v_index = BM_elem_index_get(eve);
  if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
    GPU_indexbuf_set_point_vert(elb, v_index, l_index);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, v_index);
  }
}

BLI_INLINE void vert_set_mesh(GPUIndexBufBuilder *elb,
                              const MeshRenderData *mr,
                              const int v_index,
                              const int l_index)
{
  const MVert *mv = &mr->mvert[v_index];
  if (!((mr->use_hide && (mv->flag & ME_HIDE)) ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->v_origindex) &&
         (mr->v_origindex[v_index] == ORIGINDEX_NONE)))) {
    GPU_indexbuf_set_point_vert(elb, v_index, l_index);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, v_index);
  }
}

static void extract_points_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                        const BMFace *f,
                                        const int UNUSED(f_index),
                                        void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    vert_set_bm(elb, l_iter->v, l_index);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_points_iter_poly_mesh(const MeshRenderData *mr,
                                          const MPoly *mp,
                                          const int UNUSED(mp_index),
                                          void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    vert_set_mesh(elb, mr, ml->v, ml_index);
  }
}

static void extract_points_iter_ledge_bm(const MeshRenderData *mr,
                                         const BMEdge *eed,
                                         const int ledge_index,
                                         void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  vert_set_bm(elb, eed->v1, mr->loop_len + (ledge_index * 2));
  vert_set_bm(elb, eed->v2, mr->loop_len + (ledge_index * 2) + 1);
}

static void extract_points_iter_ledge_mesh(const MeshRenderData *mr,
                                           const MEdge *med,
                                           const int ledge_index,
                                           void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  vert_set_mesh(elb, mr, med->v1, mr->loop_len + (ledge_index * 2));
  vert_set_mesh(elb, mr, med->v2, mr->loop_len + (ledge_index * 2) + 1);
}

static void extract_points_iter_lvert_bm(const MeshRenderData *mr,
                                         const BMVert *eve,
                                         const int lvert_index,
                                         void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);
  vert_set_bm(elb, eve, offset + lvert_index);
}

static void extract_points_iter_lvert_mesh(const MeshRenderData *mr,
                                           const MVert *UNUSED(mv),
                                           const int lvert_index,
                                           void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);
  vert_set_mesh(elb, mr, mr->lverts[lvert_index], offset + lvert_index);
}

static void extract_points_task_reduce(void *_userdata_to, void *_userdata_from)
{
  GPUIndexBufBuilder *elb_to = static_cast<GPUIndexBufBuilder *>(_userdata_to);
  GPUIndexBufBuilder *elb_from = static_cast<GPUIndexBufBuilder *>(_userdata_from);
  GPU_indexbuf_join(elb_to, elb_from);
}

static void extract_points_finish(const MeshRenderData *UNUSED(mr),
                                  struct MeshBatchCache *UNUSED(cache),
                                  void *buf,
                                  void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(elb, ibo);
}

static void extract_points_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                       const MeshRenderData *UNUSED(mr),
                                       struct MeshBatchCache *UNUSED(cache),
                                       void *UNUSED(buffer),
                                       void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  /* Copy the points as the data upload will free them. */
  elb->data = (uint *)MEM_dupallocN(subdiv_cache->point_indices);
  elb->index_len = subdiv_cache->num_subdiv_verts;
  elb->index_min = 0;
  elb->index_max = subdiv_cache->num_subdiv_loops - 1;
  elb->prim_type = GPU_PRIM_POINTS;
}

static void extract_points_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                             const MeshRenderData *UNUSED(mr),
                                             const MeshExtractLooseGeom *loose_geom,
                                             void *UNUSED(buffer),
                                             void *data)
{
  const int loop_loose_len = loose_geom->edge_len + loose_geom->vert_len;
  if (loop_loose_len == 0) {
    return;
  }

  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);

  elb->data = static_cast<uint32_t *>(
      MEM_reallocN(elb->data, sizeof(uint) * (subdiv_cache->num_subdiv_loops + loop_loose_len)));

  const Mesh *coarse_mesh = subdiv_cache->mesh;
  const MEdge *coarse_edges = coarse_mesh->medge;

  uint offset = subdiv_cache->num_subdiv_loops;

  for (int i = 0; i < loose_geom->edge_len; i++) {
    const MEdge *loose_edge = &coarse_edges[loose_geom->edges[i]];
    if (elb->data[loose_edge->v1] == -1u) {
      elb->data[loose_edge->v1] = offset;
    }
    if (elb->data[loose_edge->v2] == -1u) {
      elb->data[loose_edge->v2] = offset + 1;
    }
    elb->index_max += 2;
    elb->index_len += 2;
    offset += 2;
  }

  for (int i = 0; i < loose_geom->vert_len; i++) {
    if (elb->data[loose_geom->verts[i]] == -1u) {
      elb->data[loose_geom->verts[i]] = offset;
    }
    elb->index_max += 1;
    elb->index_len += 1;
    offset += 1;
  }
}

static void extract_points_finish_subdiv(const DRWSubdivCache *UNUSED(subdiv_cache),
                                         const MeshRenderData *UNUSED(mr),
                                         struct MeshBatchCache *UNUSED(cache),
                                         void *buf,
                                         void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(elb, ibo);
}

constexpr MeshExtract create_extractor_points()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_points_init;
  extractor.iter_poly_bm = extract_points_iter_poly_bm;
  extractor.iter_poly_mesh = extract_points_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_points_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_points_iter_ledge_mesh;
  extractor.iter_lvert_bm = extract_points_iter_lvert_bm;
  extractor.iter_lvert_mesh = extract_points_iter_lvert_mesh;
  extractor.task_reduce = extract_points_task_reduce;
  extractor.finish = extract_points_finish;
  extractor.init_subdiv = extract_points_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_points_loose_geom_subdiv;
  extractor.finish_subdiv = extract_points_finish_subdiv;
  extractor.use_threading = true;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.points);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_points = blender::draw::create_extractor_points();
}
