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
/** \name Extract Face-dots Indices
 * \{ */

static void *extract_fdots_init(const MeshRenderData *mr,
                                struct MeshBatchCache *UNUSED(cache),
                                void *UNUSED(buf))
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(MEM_mallocN(sizeof(*elb), __func__));
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->poly_len, mr->poly_len);
  return elb;
}

static void extract_fdots_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                       BMFace *f,
                                       const int f_index,
                                       void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    GPU_indexbuf_set_point_vert(elb, f_index, f_index);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, f_index);
  }
}

static void extract_fdots_iter_poly_mesh(const MeshRenderData *mr,
                                         const MPoly *mp,
                                         const int mp_index,
                                         void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  if (mr->use_subsurf_fdots) {
    /* Check #ME_VERT_FACEDOT. */
    const MLoop *mloop = mr->mloop;
    const int ml_index_end = mp->loopstart + mp->totloop;
    for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
      const MLoop *ml = &mloop[ml_index];
      const MVert *mv = &mr->mvert[ml->v];
      if ((mv->flag & ME_VERT_FACEDOT) && !(mr->use_hide && (mp->flag & ME_HIDE))) {
        GPU_indexbuf_set_point_vert(elb, mp_index, mp_index);
        return;
      }
    }
    GPU_indexbuf_set_point_restart(elb, mp_index);
  }
  else {
    if (!(mr->use_hide && (mp->flag & ME_HIDE))) {
      GPU_indexbuf_set_point_vert(elb, mp_index, mp_index);
    }
    else {
      GPU_indexbuf_set_point_restart(elb, mp_index);
    }
  }
}

static void extract_fdots_finish(const MeshRenderData *UNUSED(mr),
                                 struct MeshBatchCache *UNUSED(cache),
                                 void *buf,
                                 void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(elb, ibo);
  MEM_freeN(elb);
}

constexpr MeshExtract create_extractor_fdots()
{
  MeshExtract extractor = {0};
  extractor.init = extract_fdots_init;
  extractor.iter_poly_bm = extract_fdots_iter_poly_bm;
  extractor.iter_poly_mesh = extract_fdots_iter_poly_mesh;
  extractor.finish = extract_fdots_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.fdots);
  return extractor;
}

/** \} */
}  // namespace blender::draw

extern "C" {
const MeshExtract extract_fdots = blender::draw::create_extractor_fdots();
}

/** \} */
