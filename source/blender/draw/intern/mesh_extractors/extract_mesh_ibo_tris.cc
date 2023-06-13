/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "extract_mesh.hh"

#include "draw_subdivision.h"

namespace blender::draw {

static void extract_tris_mat_task_reduce(void *_userdata_to, void *_userdata_from)
{
  GPUIndexBufBuilder *elb_to = static_cast<GPUIndexBufBuilder *>(_userdata_to);
  GPUIndexBufBuilder *elb_from = static_cast<GPUIndexBufBuilder *>(_userdata_from);
  GPU_indexbuf_join(elb_to, elb_from);
}

/* ---------------------------------------------------------------------- */
/** \name Extract Triangles Indices (multi material)
 * \{ */

static void extract_tris_init(const MeshRenderData *mr,
                              MeshBatchCache * /*cache*/,
                              void * /*ibo*/,
                              void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_TRIS, mr->poly_sorted->visible_tri_len, mr->loop_len);
}

static void extract_tris_iter_poly_bm(const MeshRenderData *mr,
                                      const BMFace *f,
                                      const int f_index,
                                      void *_data)
{
  int tri_first_index = mr->poly_sorted->tri_first_index[f_index];
  if (tri_first_index == -1) {
    return;
  }

  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  int tri_first_index_real = poly_to_tri_count(f_index, BM_elem_index_get(f->l_first));

  BMLoop *(*looptris)[3] = mr->edit_bmesh->looptris;
  int tri_len = f->len - 2;
  for (int offs = 0; offs < tri_len; offs++) {
    BMLoop **elt = looptris[tri_first_index_real + offs];
    int tri_index = tri_first_index + offs;
    GPU_indexbuf_set_tri_verts(elb,
                               tri_index,
                               BM_elem_index_get(elt[0]),
                               BM_elem_index_get(elt[1]),
                               BM_elem_index_get(elt[2]));
  }
}

static void extract_tris_iter_poly_mesh(const MeshRenderData *mr,
                                        const int poly_index,
                                        void *_data)
{
  int tri_first_index = mr->poly_sorted->tri_first_index[poly_index];
  if (tri_first_index == -1) {
    return;
  }

  const IndexRange poly = mr->polys[poly_index];

  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  int tri_first_index_real = poly_to_tri_count(poly_index, poly.start());

  int tri_len = poly.size() - 2;
  for (int offs = 0; offs < tri_len; offs++) {
    const MLoopTri *mlt = &mr->looptris[tri_first_index_real + offs];
    int tri_index = tri_first_index + offs;
    GPU_indexbuf_set_tri_verts(elb, tri_index, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
  }
}

static void extract_tris_finish(const MeshRenderData *mr,
                                MeshBatchCache *cache,
                                void *buf,
                                void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  GPU_indexbuf_build_in_place(elb, ibo);

  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr->use_final_mesh && cache->tris_per_mat) {
    int mat_start = 0;
    for (int i = 0; i < mr->mat_len; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (cache->tris_per_mat[i] == nullptr) {
        cache->tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      const int mat_tri_len = mr->poly_sorted->mat_tri_len[i];
      /* Multiply by 3 because these are triangle indices. */
      const int start = mat_start * 3;
      const int len = mat_tri_len * 3;
      GPU_indexbuf_create_subrange_in_place(cache->tris_per_mat[i], ibo, start, len);
      mat_start += mat_tri_len;
    }
  }
}

static void extract_tris_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                     const MeshRenderData * /*mr*/,
                                     MeshBatchCache *cache,
                                     void *buffer,
                                     void * /*data*/)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buffer);
  /* Initialize the index buffer, it was already allocated, it will be filled on the device. */
  GPU_indexbuf_init_build_on_device(ibo, subdiv_cache->num_subdiv_triangles * 3);

  if (cache->tris_per_mat) {
    for (int i = 0; i < cache->mat_len; i++) {
      if (cache->tris_per_mat[i] == nullptr) {
        cache->tris_per_mat[i] = GPU_indexbuf_calloc();
      }

      /* Multiply by 6 since we have 2 triangles per quad. */
      const int start = subdiv_cache->mat_start[i] * 6;
      const int len = (subdiv_cache->mat_end[i] - subdiv_cache->mat_start[i]) * 6;
      GPU_indexbuf_create_subrange_in_place(cache->tris_per_mat[i], ibo, start, len);
    }
  }

  draw_subdiv_build_tris_buffer(subdiv_cache, ibo, cache->mat_len);
}

constexpr MeshExtract create_extractor_tris()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tris_init;
  extractor.init_subdiv = extract_tris_init_subdiv;
  extractor.iter_poly_bm = extract_tris_iter_poly_bm;
  extractor.iter_poly_mesh = extract_tris_iter_poly_mesh;
  extractor.task_reduce = extract_tris_mat_task_reduce;
  extractor.finish = extract_tris_finish;
  extractor.data_type = MR_DATA_LOOPTRI | MR_DATA_POLYS_SORTED;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.tris);
  return extractor;
}

/** \} */

/** \name Extract Triangles Indices (single material)
 * \{ */

static void extract_tris_single_mat_init(const MeshRenderData *mr,
                                         MeshBatchCache * /*cache*/,
                                         void * /*ibo*/,
                                         void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_TRIS, mr->tri_len, mr->loop_len);
}

static void extract_tris_single_mat_iter_looptri_bm(const MeshRenderData * /*mr*/,
                                                    BMLoop **elt,
                                                    const int elt_index,
                                                    void *_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  if (!BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN)) {
    GPU_indexbuf_set_tri_verts(elb,
                               elt_index,
                               BM_elem_index_get(elt[0]),
                               BM_elem_index_get(elt[1]),
                               BM_elem_index_get(elt[2]));
  }
  else {
    GPU_indexbuf_set_tri_restart(elb, elt_index);
  }
}

static void extract_tris_single_mat_iter_looptri_mesh(const MeshRenderData *mr,
                                                      const MLoopTri *mlt,
                                                      const int mlt_index,
                                                      void *_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  const int poly_i = mr->looptri_polys[mlt_index];
  const bool hidden = mr->use_hide && mr->hide_poly && mr->hide_poly[poly_i];
  if (hidden) {
    GPU_indexbuf_set_tri_restart(elb, mlt_index);
  }
  else {
    GPU_indexbuf_set_tri_verts(elb, mlt_index, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
  }
}

static void extract_tris_single_mat_finish(const MeshRenderData *mr,
                                           MeshBatchCache *cache,
                                           void *buf,
                                           void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  GPU_indexbuf_build_in_place(elb, ibo);

  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr->use_final_mesh && cache->tris_per_mat) {
    for (int i = 0; i < mr->mat_len; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (cache->tris_per_mat[i] == nullptr) {
        cache->tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      /* Multiply by 3 because these are triangle indices. */
      const int len = mr->tri_len * 3;
      GPU_indexbuf_create_subrange_in_place(cache->tris_per_mat[i], ibo, 0, len);
    }
  }
}

constexpr MeshExtract create_extractor_tris_single_mat()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tris_single_mat_init;
  extractor.init_subdiv = extract_tris_init_subdiv;
  extractor.iter_looptri_bm = extract_tris_single_mat_iter_looptri_bm;
  extractor.iter_looptri_mesh = extract_tris_single_mat_iter_looptri_mesh;
  extractor.task_reduce = extract_tris_mat_task_reduce;
  extractor.finish = extract_tris_single_mat_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.tris);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_tris = blender::draw::create_extractor_tris();
const MeshExtract extract_tris_single_mat = blender::draw::create_extractor_tris_single_mat();
