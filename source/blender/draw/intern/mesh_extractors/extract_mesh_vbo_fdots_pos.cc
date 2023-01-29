/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#include "BLI_bitmap.h"

#include "extract_mesh.hh"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots positions
 * \{ */

static GPUVertFormat *get_fdots_pos_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  return &format;
}

static GPUVertFormat *get_fdots_nor_format_subdiv()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

static void extract_fdots_pos_init(const MeshRenderData *mr,
                                   MeshBatchCache * /*cache*/,
                                   void *buf,
                                   void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  GPUVertFormat *format = get_fdots_pos_format();
  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);
  void *vbo_data = GPU_vertbuf_get_data(vbo);
  *(float(**)[3])tls_data = static_cast<float(*)[3]>(vbo_data);
}

static void extract_fdots_pos_iter_poly_bm(const MeshRenderData *mr,
                                           const BMFace *f,
                                           const int f_index,
                                           void *data)
{
  float(*center)[3] = *static_cast<float(**)[3]>(data);

  float *co = center[f_index];
  zero_v3(co);

  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    add_v3_v3(co, bm_vert_co_get(mr, l_iter->v));
  } while ((l_iter = l_iter->next) != l_first);
  mul_v3_fl(co, 1.0f / float(f->len));
}

static void extract_fdots_pos_iter_poly_mesh(const MeshRenderData *mr,
                                             const MPoly *mp,
                                             const int mp_index,
                                             void *data)
{
  float(*center)[3] = *static_cast<float(**)[3]>(data);
  float *co = center[mp_index];
  zero_v3(co);

  const MLoop *mloop = mr->mloop;
  const BLI_bitmap *facedot_tags = mr->me->runtime->subsurf_face_dot_tags;

  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    if (mr->use_subsurf_fdots) {
      if (BLI_BITMAP_TEST(facedot_tags, ml->v)) {
        copy_v3_v3(center[mp_index], mr->vert_positions[ml->v]);
        break;
      }
    }
    else {
      add_v3_v3(center[mp_index], mr->vert_positions[ml->v]);
    }
  }

  if (!mr->use_subsurf_fdots) {
    mul_v3_fl(co, 1.0f / float(mp->totloop));
  }
}

static void extract_fdots_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                      const MeshRenderData * /*mr*/,
                                      MeshBatchCache *cache,
                                      void *buffer,
                                      void * /*data*/)
{
  /* We "extract" positions, normals, and indices at once. */
  GPUVertBuf *fdots_pos_vbo = static_cast<GPUVertBuf *>(buffer);
  GPUVertBuf *fdots_nor_vbo = cache->final.buff.vbo.fdots_nor;
  GPUIndexBuf *fdots_pos_ibo = cache->final.buff.ibo.fdots;

  /* The normals may not be requested. */
  if (fdots_nor_vbo) {
    GPU_vertbuf_init_build_on_device(
        fdots_nor_vbo, get_fdots_nor_format_subdiv(), subdiv_cache->num_coarse_poly);
  }
  GPU_vertbuf_init_build_on_device(
      fdots_pos_vbo, get_fdots_pos_format(), subdiv_cache->num_coarse_poly);
  GPU_indexbuf_init_build_on_device(fdots_pos_ibo, subdiv_cache->num_coarse_poly);
  draw_subdiv_build_fdots_buffers(subdiv_cache, fdots_pos_vbo, fdots_nor_vbo, fdots_pos_ibo);
}

constexpr MeshExtract create_extractor_fdots_pos()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_pos_init;
  extractor.init_subdiv = extract_fdots_init_subdiv;
  extractor.iter_poly_bm = extract_fdots_pos_iter_poly_bm;
  extractor.iter_poly_mesh = extract_fdots_pos_iter_poly_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(float(*)[3]);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdots_pos);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_fdots_pos = blender::draw::create_extractor_fdots_pos();
