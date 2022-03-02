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

#include "MEM_guardedalloc.h"

#include "extract_mesh.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edges Indices
 * \{ */

static void extract_lines_init(const MeshRenderData *mr,
                               struct MeshBatchCache *UNUSED(cache),
                               void *UNUSED(buf),
                               void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  /* Put loose edges at the end. */
  GPU_indexbuf_init(
      elb, GPU_PRIM_LINES, mr->edge_len + mr->edge_loose_len, mr->loop_len + mr->loop_loose_len);
}

static void extract_lines_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                       const BMFace *f,
                                       const int UNUSED(f_index),
                                       void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  BMLoop *l_iter, *l_first;
  /* Use #BMLoop.prev to match mesh order (to avoid minor differences in data extraction). */
  l_iter = l_first = BM_FACE_FIRST_LOOP(f)->prev;
  do {
    if (!BM_elem_flag_test(l_iter->e, BM_ELEM_HIDDEN)) {
      GPU_indexbuf_set_line_verts(elb,
                                  BM_elem_index_get(l_iter->e),
                                  BM_elem_index_get(l_iter),
                                  BM_elem_index_get(l_iter->next));
    }
    else {
      GPU_indexbuf_set_line_restart(elb, BM_elem_index_get(l_iter->e));
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_lines_iter_poly_mesh(const MeshRenderData *mr,
                                         const MPoly *mp,
                                         const int UNUSED(mp_index),
                                         void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  /* Using poly & loop iterator would complicate accessing the adjacent loop. */
  const MLoop *mloop = mr->mloop;
  const MEdge *medge = mr->medge;
  if (mr->use_hide || (mr->extract_type == MR_EXTRACT_MAPPED) || (mr->e_origindex != nullptr)) {
    const int ml_index_last = mp->loopstart + (mp->totloop - 1);
    int ml_index = ml_index_last, ml_index_next = mp->loopstart;
    do {
      const MLoop *ml = &mloop[ml_index];
      const MEdge *med = &medge[ml->e];
      if (!((mr->use_hide && (med->flag & ME_HIDE)) ||
            ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->e_origindex) &&
             (mr->e_origindex[ml->e] == ORIGINDEX_NONE)))) {
        GPU_indexbuf_set_line_verts(elb, ml->e, ml_index, ml_index_next);
      }
      else {
        GPU_indexbuf_set_line_restart(elb, ml->e);
      }
    } while ((ml_index = ml_index_next++) != ml_index_last);
  }
  else {
    const int ml_index_last = mp->loopstart + (mp->totloop - 1);
    int ml_index = ml_index_last, ml_index_next = mp->loopstart;
    do {
      const MLoop *ml = &mloop[ml_index];
      GPU_indexbuf_set_line_verts(elb, ml->e, ml_index, ml_index_next);
    } while ((ml_index = ml_index_next++) != ml_index_last);
  }
}

static void extract_lines_iter_ledge_bm(const MeshRenderData *mr,
                                        const BMEdge *eed,
                                        const int ledge_index,
                                        void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  const int l_index_offset = mr->edge_len + ledge_index;
  if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
    const int l_index = mr->loop_len + ledge_index * 2;
    GPU_indexbuf_set_line_verts(elb, l_index_offset, l_index, l_index + 1);
  }
  else {
    GPU_indexbuf_set_line_restart(elb, l_index_offset);
  }
  /* Don't render the edge twice. */
  GPU_indexbuf_set_line_restart(elb, BM_elem_index_get(eed));
}

static void extract_lines_iter_ledge_mesh(const MeshRenderData *mr,
                                          const MEdge *med,
                                          const int ledge_index,
                                          void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  const int l_index_offset = mr->edge_len + ledge_index;
  const int e_index = mr->ledges[ledge_index];
  if (!((mr->use_hide && (med->flag & ME_HIDE)) ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->e_origindex) &&
         (mr->e_origindex[e_index] == ORIGINDEX_NONE)))) {
    const int l_index = mr->loop_len + ledge_index * 2;
    GPU_indexbuf_set_line_verts(elb, l_index_offset, l_index, l_index + 1);
  }
  else {
    GPU_indexbuf_set_line_restart(elb, l_index_offset);
  }
  /* Don't render the edge twice. */
  GPU_indexbuf_set_line_restart(elb, e_index);
}

static void extract_lines_task_reduce(void *_userdata_to, void *_userdata_from)
{
  GPUIndexBufBuilder *elb_to = static_cast<GPUIndexBufBuilder *>(_userdata_to);
  GPUIndexBufBuilder *elb_from = static_cast<GPUIndexBufBuilder *>(_userdata_from);
  GPU_indexbuf_join(elb_to, elb_from);
}

static void extract_lines_finish(const MeshRenderData *UNUSED(mr),
                                 struct MeshBatchCache *UNUSED(cache),
                                 void *buf,
                                 void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(elb, ibo);
}

static void extract_lines_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                      const MeshRenderData *UNUSED(mr),
                                      struct MeshBatchCache *UNUSED(cache),
                                      void *buffer,
                                      void *UNUSED(data))
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buffer);
  GPU_indexbuf_init_build_on_device(ibo,
                                    subdiv_cache->num_subdiv_loops * 2 + loose_geom.edge_len * 2);

  if (subdiv_cache->num_subdiv_loops == 0) {
    return;
  }

  draw_subdiv_build_lines_buffer(subdiv_cache, ibo);
}

static void extract_lines_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                            const MeshRenderData *UNUSED(mr),
                                            void *buffer,
                                            void *UNUSED(data))
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  if (loose_geom.edge_len == 0) {
    return;
  }

  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buffer);
  draw_subdiv_build_lines_loose_buffer(subdiv_cache, ibo, static_cast<uint>(loose_geom.edge_len));
}

constexpr MeshExtract create_extractor_lines()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_init;
  extractor.iter_poly_bm = extract_lines_iter_poly_bm;
  extractor.iter_poly_mesh = extract_lines_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_lines_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_lines_iter_ledge_mesh;
  extractor.init_subdiv = extract_lines_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_lines_loose_geom_subdiv;
  extractor.task_reduce = extract_lines_task_reduce;
  extractor.finish = extract_lines_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.lines);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Lines and Loose Edges Sub Buffer
 * \{ */

static void extract_lines_loose_subbuffer(const MeshRenderData *mr, struct MeshBatchCache *cache)
{
  BLI_assert(cache->final.buff.ibo.lines);
  /* Multiply by 2 because these are edges indices. */
  const int start = mr->edge_len * 2;
  const int len = mr->edge_loose_len * 2;
  GPU_indexbuf_create_subrange_in_place(
      cache->final.buff.ibo.lines_loose, cache->final.buff.ibo.lines, start, len);
  cache->no_loose_wire = (len == 0);
}

static void extract_lines_with_lines_loose_finish(const MeshRenderData *mr,
                                                  struct MeshBatchCache *cache,
                                                  void *buf,
                                                  void *data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(elb, ibo);
  extract_lines_loose_subbuffer(mr, cache);
}

static void extract_lines_with_lines_loose_finish_subdiv(const struct DRWSubdivCache *subdiv_cache,
                                                         const MeshRenderData *UNUSED(mr),
                                                         struct MeshBatchCache *cache,
                                                         void *UNUSED(buf),
                                                         void *UNUSED(_data))
{
  /* Multiply by 2 because these are edges indices. */
  const int start = subdiv_cache->num_subdiv_loops * 2;
  const int len = subdiv_cache->loose_geom.edge_len * 2;
  GPU_indexbuf_create_subrange_in_place(
      cache->final.buff.ibo.lines_loose, cache->final.buff.ibo.lines, start, len);
  cache->no_loose_wire = (len == 0);
}

constexpr MeshExtract create_extractor_lines_with_lines_loose()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_init;
  extractor.iter_poly_bm = extract_lines_iter_poly_bm;
  extractor.iter_poly_mesh = extract_lines_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_lines_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_lines_iter_ledge_mesh;
  extractor.task_reduce = extract_lines_task_reduce;
  extractor.finish = extract_lines_with_lines_loose_finish;
  extractor.init_subdiv = extract_lines_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_lines_loose_geom_subdiv;
  extractor.finish_subdiv = extract_lines_with_lines_loose_finish_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.lines);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Loose Edges Sub Buffer
 * \{ */

static void extract_lines_loose_only_init(const MeshRenderData *mr,
                                          struct MeshBatchCache *cache,
                                          void *buf,
                                          void *UNUSED(tls_data))
{
  BLI_assert(buf == cache->final.buff.ibo.lines_loose);
  UNUSED_VARS_NDEBUG(buf);
  extract_lines_loose_subbuffer(mr, cache);
}

static void extract_lines_loose_only_init_subdiv(const DRWSubdivCache *UNUSED(subdiv_cache),
                                                 const MeshRenderData *mr,
                                                 struct MeshBatchCache *cache,
                                                 void *buffer,
                                                 void *UNUSED(data))
{
  BLI_assert(buffer == cache->final.buff.ibo.lines_loose);
  UNUSED_VARS_NDEBUG(buffer);
  extract_lines_loose_subbuffer(mr, cache);
}

constexpr MeshExtract create_extractor_lines_loose_only()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_loose_only_init;
  extractor.init_subdiv = extract_lines_loose_only_init_subdiv;
  extractor.data_type = MR_DATA_LOOSE_GEOM;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.lines_loose);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_lines = blender::draw::create_extractor_lines();
const MeshExtract extract_lines_with_lines_loose =
    blender::draw::create_extractor_lines_with_lines_loose();
const MeshExtract extract_lines_loose_only = blender::draw::create_extractor_lines_loose_only();
}
