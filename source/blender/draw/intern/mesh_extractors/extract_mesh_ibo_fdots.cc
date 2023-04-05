/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup draw
 */

#include "BLI_bitmap.h"

#include "extract_mesh.hh"

namespace blender::draw {
/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots Indices
 * \{ */

static void extract_fdots_init(const MeshRenderData *mr,
                               MeshBatchCache * /*cache*/,
                               void * /*buf*/,
                               void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->poly_len, mr->poly_len);
}

static void extract_fdots_iter_poly_bm(const MeshRenderData * /*mr*/,
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
                                         const int poly_index,
                                         void *_userdata)
{
  const bool hidden = mr->use_hide && mr->hide_poly && mr->hide_poly[poly_index];

  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  if (mr->use_subsurf_fdots) {
    const BitSpan facedot_tags = mr->me->runtime->subsurf_face_dot_tags;

    for (const int ml_index : mr->polys[poly_index]) {
      const int vert = mr->corner_verts[ml_index];
      if (facedot_tags[vert] && !hidden) {
        GPU_indexbuf_set_point_vert(elb, poly_index, poly_index);
        return;
      }
    }
    GPU_indexbuf_set_point_restart(elb, poly_index);
  }
  else {
    if (!hidden) {
      GPU_indexbuf_set_point_vert(elb, poly_index, poly_index);
    }
    else {
      GPU_indexbuf_set_point_restart(elb, poly_index);
    }
  }
}

static void extract_fdots_finish(const MeshRenderData * /*mr*/,
                                 MeshBatchCache * /*cache*/,
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

const MeshExtract extract_fdots = blender::draw::create_extractor_fdots();
