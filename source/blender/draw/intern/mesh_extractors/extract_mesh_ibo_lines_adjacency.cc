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

#include "BLI_edgehash.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Line Adjacency Indices
 * \{ */

#define NO_EDGE INT_MAX

struct MeshExtract_LineAdjacency_Data {
  GPUIndexBufBuilder elb;
  EdgeHash *eh;
  bool is_manifold;
  /* Array to convert vert index to any loop index of this vert. */
  uint *vert_to_loop;
};

static void extract_lines_adjacency_init(const MeshRenderData *mr,
                                         struct MeshBatchCache *UNUSED(cache),
                                         void *UNUSED(buf),
                                         void *tls_data)
{
  /* Similar to poly_to_tri_count().
   * There is always (loop + triangle - 1) edges inside a polygon.
   * Accumulate for all polys and you get : */
  uint tess_edge_len = mr->loop_len + mr->tri_len - mr->poly_len;

  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(tls_data);
  data->vert_to_loop = static_cast<uint *>(MEM_callocN(sizeof(uint) * mr->vert_len, __func__));

  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES_ADJ, tess_edge_len, mr->loop_len);
  data->eh = BLI_edgehash_new_ex(__func__, tess_edge_len);
  data->is_manifold = true;
}

BLI_INLINE void lines_adjacency_triangle(
    uint v1, uint v2, uint v3, uint l1, uint l2, uint l3, MeshExtract_LineAdjacency_Data *data)
{
  GPUIndexBufBuilder *elb = &data->elb;
  /* Iterate around the triangle's edges. */
  for (int e = 0; e < 3; e++) {
    SHIFT3(uint, v3, v2, v1);
    SHIFT3(uint, l3, l2, l1);

    bool inv_indices = (v2 > v3);
    void **pval;
    bool value_is_init = BLI_edgehash_ensure_p(data->eh, v2, v3, &pval);
    int v_data = POINTER_AS_INT(*pval);
    if (!value_is_init || v_data == NO_EDGE) {
      /* Save the winding order inside the sign bit. Because the
       * Edge-hash sort the keys and we need to compare winding later. */
      int value = (int)l1 + 1; /* 0 cannot be signed so add one. */
      *pval = POINTER_FROM_INT((inv_indices) ? -value : value);
      /* Store loop indices for remaining non-manifold edges. */
      data->vert_to_loop[v2] = l2;
      data->vert_to_loop[v3] = l3;
    }
    else {
      /* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
      *pval = POINTER_FROM_INT(NO_EDGE);
      bool inv_opposite = (v_data < 0);
      uint l_opposite = (uint)abs(v_data) - 1;
      /* TODO Make this part thread-safe. */
      if (inv_opposite == inv_indices) {
        /* Don't share edge if triangles have non matching winding. */
        GPU_indexbuf_add_line_adj_verts(elb, l1, l2, l3, l1);
        GPU_indexbuf_add_line_adj_verts(elb, l_opposite, l2, l3, l_opposite);
        data->is_manifold = false;
      }
      else {
        GPU_indexbuf_add_line_adj_verts(elb, l1, l2, l3, l_opposite);
      }
    }
  }
}

static void extract_lines_adjacency_iter_looptri_bm(const MeshRenderData *UNUSED(mr),
                                                    BMLoop **elt,
                                                    const int UNUSED(elt_index),
                                                    void *_data)
{
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);
  if (!BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN)) {
    lines_adjacency_triangle(BM_elem_index_get(elt[0]->v),
                             BM_elem_index_get(elt[1]->v),
                             BM_elem_index_get(elt[2]->v),
                             BM_elem_index_get(elt[0]),
                             BM_elem_index_get(elt[1]),
                             BM_elem_index_get(elt[2]),
                             data);
  }
}

static void extract_lines_adjacency_iter_looptri_mesh(const MeshRenderData *mr,
                                                      const MLoopTri *mlt,
                                                      const int UNUSED(elt_index),
                                                      void *_data)
{
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);
  const MPoly *mp = &mr->mpoly[mlt->poly];
  if (!(mr->use_hide && (mp->flag & ME_HIDE))) {
    lines_adjacency_triangle(mr->mloop[mlt->tri[0]].v,
                             mr->mloop[mlt->tri[1]].v,
                             mr->mloop[mlt->tri[2]].v,
                             mlt->tri[0],
                             mlt->tri[1],
                             mlt->tri[2],
                             data);
  }
}

static void extract_lines_adjacency_finish(const MeshRenderData *UNUSED(mr),
                                           struct MeshBatchCache *cache,
                                           void *buf,
                                           void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);
  /* Create edges for remaining non manifold edges. */
  EdgeHashIterator *ehi = BLI_edgehashIterator_new(data->eh);
  for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
    uint v2, v3, l1, l2, l3;
    int v_data = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));
    if (v_data != NO_EDGE) {
      BLI_edgehashIterator_getKey(ehi, &v2, &v3);
      l1 = (uint)abs(v_data) - 1;
      if (v_data < 0) { /* `inv_opposite`. */
        SWAP(uint, v2, v3);
      }
      l2 = data->vert_to_loop[v2];
      l3 = data->vert_to_loop[v3];
      GPU_indexbuf_add_line_adj_verts(&data->elb, l1, l2, l3, l1);
      data->is_manifold = false;
    }
  }
  BLI_edgehashIterator_free(ehi);
  BLI_edgehash_free(data->eh, nullptr);

  cache->is_manifold = data->is_manifold;

  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data->vert_to_loop);
}

#undef NO_EDGE

/** \} */

constexpr MeshExtract create_extractor_lines_adjacency()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_adjacency_init;
  extractor.iter_looptri_bm = extract_lines_adjacency_iter_looptri_bm;
  extractor.iter_looptri_mesh = extract_lines_adjacency_iter_looptri_mesh;
  extractor.finish = extract_lines_adjacency_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_LineAdjacency_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.lines_adjacency);
  return extractor;
}

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_lines_adjacency = blender::draw::create_extractor_lines_adjacency();
}

/** \} */
