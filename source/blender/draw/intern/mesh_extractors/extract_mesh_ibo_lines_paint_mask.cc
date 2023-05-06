/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup draw
 */

#include "BLI_bitmap.h"
#include "BLI_vector.hh"
#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "draw_subdivision.h"
#include "extract_mesh.hh"

namespace blender::draw {
/* ---------------------------------------------------------------------- */
/** \name Extract Paint Mask Line Indices
 * \{ */

struct MeshExtract_LinePaintMask_Data {
  GPUIndexBufBuilder elb;
  /** One bit per edge set if face is selected. */
  BLI_bitmap *select_map;
};

static void extract_lines_paint_mask_init(const MeshRenderData *mr,
                                          MeshBatchCache * /*cache*/,
                                          void * /*ibo*/,
                                          void *tls_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(tls_data);
  data->select_map = BLI_BITMAP_NEW(mr->edge_len, __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES, mr->edge_len, mr->loop_len);
}

static void extract_lines_paint_mask_iter_poly_mesh(const MeshRenderData *mr,
                                                    const int poly_index,
                                                    void *_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(_data);
  const IndexRange poly = mr->polys[poly_index];

  const int ml_index_end = poly.start() + poly.size();
  for (int ml_index = poly.start(); ml_index < ml_index_end; ml_index += 1) {
    const int e_index = mr->corner_edges[ml_index];

    if (!((mr->use_hide && mr->hide_edge && mr->hide_edge[e_index]) ||
          ((mr->e_origindex) && (mr->e_origindex[e_index] == ORIGINDEX_NONE))))
    {

      const int ml_index_last = poly.size() + poly.start() - 1;
      const int ml_index_other = (ml_index == ml_index_last) ? poly.start() : (ml_index + 1);
      if (mr->select_poly && mr->select_poly[poly_index]) {
        if (BLI_BITMAP_TEST_AND_SET_ATOMIC(data->select_map, e_index)) {
          /* Hide edge as it has more than 2 selected loop. */
          GPU_indexbuf_set_line_restart(&data->elb, e_index);
        }
        else {
          /* First selected loop. Set edge visible, overwriting any unselected loop. */
          GPU_indexbuf_set_line_verts(&data->elb, e_index, ml_index, ml_index_other);
        }
      }
      else {
        /* Set these unselected loop only if this edge has no other selected loop. */
        if (!BLI_BITMAP_TEST(data->select_map, e_index)) {
          GPU_indexbuf_set_line_verts(&data->elb, e_index, ml_index, ml_index_other);
        }
      }
    }
    else {
      GPU_indexbuf_set_line_restart(&data->elb, e_index);
    }
  }
}

static void extract_lines_paint_mask_finish(const MeshRenderData * /*mr*/,
                                            MeshBatchCache * /*cache*/,
                                            void *buf,
                                            void *_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(_data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data->select_map);
}

static void extract_lines_paint_mask_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                                 const MeshRenderData *mr,
                                                 MeshBatchCache * /*cache*/,
                                                 void * /*buf*/,
                                                 void *tls_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(tls_data);
  data->select_map = BLI_BITMAP_NEW(mr->edge_len, __func__);
  GPU_indexbuf_init(&data->elb,
                    GPU_PRIM_LINES,
                    subdiv_cache->num_subdiv_edges,
                    subdiv_cache->num_subdiv_loops * 2);
}

static void extract_lines_paint_mask_iter_subdiv_mesh(const DRWSubdivCache *subdiv_cache,
                                                      const MeshRenderData *mr,
                                                      void *_data,
                                                      uint subdiv_quad_index,
                                                      const int coarse_quad_index)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(_data);
  int *subdiv_loop_edge_index = (int *)GPU_vertbuf_get_data(subdiv_cache->edges_orig_index);
  int *subdiv_loop_subdiv_edge_index = subdiv_cache->subdiv_loop_subdiv_edge_index;

  uint start_loop_idx = subdiv_quad_index * 4;
  uint end_loop_idx = (subdiv_quad_index + 1) * 4;
  for (uint loop_idx = start_loop_idx; loop_idx < end_loop_idx; loop_idx++) {
    const uint coarse_edge_index = uint(subdiv_loop_edge_index[loop_idx]);
    const uint subdiv_edge_index = uint(subdiv_loop_subdiv_edge_index[loop_idx]);

    if (coarse_edge_index == -1u) {
      GPU_indexbuf_set_line_restart(&data->elb, subdiv_edge_index);
    }
    else {
      if (!((mr->use_hide && mr->hide_edge && mr->hide_edge[coarse_edge_index]) ||
            ((mr->e_origindex) && (mr->e_origindex[coarse_edge_index] == ORIGINDEX_NONE))))
      {
        const uint ml_index_other = (loop_idx == (end_loop_idx - 1)) ? start_loop_idx :
                                                                       loop_idx + 1;
        if (mr->select_poly && mr->select_poly[coarse_quad_index]) {
          if (BLI_BITMAP_TEST_AND_SET_ATOMIC(data->select_map, coarse_edge_index)) {
            /* Hide edge as it has more than 2 selected loop. */
            GPU_indexbuf_set_line_restart(&data->elb, subdiv_edge_index);
          }
          else {
            /* First selected loop. Set edge visible, overwriting any unselected loop. */
            GPU_indexbuf_set_line_verts(&data->elb, subdiv_edge_index, loop_idx, ml_index_other);
          }
        }
        else {
          /* Set these unselected loop only if this edge has no other selected loop. */
          if (!BLI_BITMAP_TEST(data->select_map, coarse_edge_index)) {
            GPU_indexbuf_set_line_verts(&data->elb, subdiv_edge_index, loop_idx, ml_index_other);
          }
        }
      }
      else {
        GPU_indexbuf_set_line_restart(&data->elb, subdiv_edge_index);
      }
    }
  }
}

static void extract_lines_paint_mask_finish_subdiv(const struct DRWSubdivCache * /*subdiv_cache*/,
                                                   const MeshRenderData *mr,
                                                   MeshBatchCache *cache,
                                                   void *buf,
                                                   void *_data)
{
  extract_lines_paint_mask_finish(mr, cache, buf, _data);
}

constexpr MeshExtract create_extractor_lines_paint_mask()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_paint_mask_init;
  extractor.iter_poly_mesh = extract_lines_paint_mask_iter_poly_mesh;
  extractor.finish = extract_lines_paint_mask_finish;
  extractor.init_subdiv = extract_lines_paint_mask_init_subdiv;
  extractor.iter_subdiv_mesh = extract_lines_paint_mask_iter_subdiv_mesh;
  extractor.finish_subdiv = extract_lines_paint_mask_finish_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_LinePaintMask_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.lines_paint_mask);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_lines_paint_mask = blender::draw::create_extractor_lines_paint_mask();
