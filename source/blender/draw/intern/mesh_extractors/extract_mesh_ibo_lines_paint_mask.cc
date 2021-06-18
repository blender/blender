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

#include "BLI_bitmap.h"
#include "BLI_vector.hh"
#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

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
                                          struct MeshBatchCache *UNUSED(cache),
                                          void *UNUSED(ibo),
                                          void *tls_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(tls_data);
  data->select_map = BLI_BITMAP_NEW(mr->edge_len, __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES, mr->edge_len, mr->loop_len);
}

static void extract_lines_paint_mask_iter_poly_mesh(const MeshRenderData *mr,
                                                    const MPoly *mp,
                                                    const int UNUSED(mp_index),
                                                    void *_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(_data);
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    const int e_index = ml->e;
    const MEdge *me = &mr->medge[e_index];
    if (!((mr->use_hide && (me->flag & ME_HIDE)) ||
          ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->e_origindex) &&
           (mr->e_origindex[e_index] == ORIGINDEX_NONE)))) {

      const int ml_index_last = mp->totloop + mp->loopstart - 1;
      const int ml_index_other = (ml_index == ml_index_last) ? mp->loopstart : (ml_index + 1);
      if (mp->flag & ME_FACE_SEL) {
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

static void extract_lines_paint_mask_finish(const MeshRenderData *UNUSED(mr),
                                            struct MeshBatchCache *UNUSED(cache),
                                            void *buf,
                                            void *_data)
{
  MeshExtract_LinePaintMask_Data *data = static_cast<MeshExtract_LinePaintMask_Data *>(_data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data->select_map);
}

/** \} */

constexpr MeshExtract create_extractor_lines_paint_mask()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_paint_mask_init;
  extractor.iter_poly_mesh = extract_lines_paint_mask_iter_poly_mesh;
  extractor.finish = extract_lines_paint_mask_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_LinePaintMask_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.lines_paint_mask);
  return extractor;
}

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_lines_paint_mask = blender::draw::create_extractor_lines_paint_mask();
}

/** \} */
