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

#include "draw_cache_extract_mesh_private.h"

#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Point Indices
 * \{ */
static void *extract_points_init(const MeshRenderData *mr,
                                 struct MeshBatchCache *UNUSED(cache),
                                 void *UNUSED(buf))
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(MEM_mallocN(sizeof(*elb), __func__));
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->vert_len, mr->loop_len + mr->loop_loose_len);
  return elb;
}

BLI_INLINE void vert_set_bm(GPUIndexBufBuilder *elb, BMVert *eve, int l_index)
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
                                        BMFace *f,
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
                                         BMEdge *eed,
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
                                         BMVert *eve,
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

static void extract_points_finish(const MeshRenderData *UNUSED(mr),
                                  struct MeshBatchCache *UNUSED(cache),
                                  void *buf,
                                  void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(elb, ibo);
  MEM_freeN(elb);
}

constexpr MeshExtract create_extractor_points()
{
  MeshExtract extractor = {0};
  extractor.init = extract_points_init;
  extractor.iter_poly_bm = extract_points_iter_poly_bm;
  extractor.iter_poly_mesh = extract_points_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_points_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_points_iter_ledge_mesh;
  extractor.iter_lvert_bm = extract_points_iter_lvert_bm;
  extractor.iter_lvert_mesh = extract_points_iter_lvert_mesh;
  extractor.finish = extract_points_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.points);
  return extractor;
}

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_points = blender::draw::create_extractor_points();
}

/** \} */
