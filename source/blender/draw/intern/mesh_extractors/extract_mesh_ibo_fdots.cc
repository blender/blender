/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#include "BLI_bitmap.h"

#include "extract_mesh.h"

namespace blender::draw {
/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots Indices
 * \{ */

static void extract_fdots_init(const MeshRenderData *mr,
                               struct MeshBatchCache *UNUSED(cache),
                               void *UNUSED(buf),
                               void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->poly_len, mr->poly_len);
}

static void extract_fdots_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                       const BMFace *f,
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
    const BLI_bitmap *facedot_tags = mr->me->runtime.subsurf_face_dot_tags;

    const MLoop *mloop = mr->mloop;
    const int ml_index_end = mp->loopstart + mp->totloop;
    for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
      const MLoop *ml = &mloop[ml_index];
      if (BLI_BITMAP_TEST(facedot_tags, ml->v) && !(mr->use_hide && (mp->flag & ME_HIDE))) {
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
}

constexpr MeshExtract create_extractor_fdots()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_init;
  extractor.iter_poly_bm = extract_fdots_iter_poly_bm;
  extractor.iter_poly_mesh = extract_fdots_iter_poly_mesh;
  extractor.finish = extract_fdots_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.fdots);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_fdots = blender::draw::create_extractor_fdots();
}
