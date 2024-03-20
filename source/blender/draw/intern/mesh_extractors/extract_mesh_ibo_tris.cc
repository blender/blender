/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_editmesh.hh"

#include "GPU_index_buffer.h"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

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

static void extract_tris_init(const MeshRenderData &mr,
                              MeshBatchCache & /*cache*/,
                              void * /*ibo*/,
                              void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_TRIS, mr.face_sorted->visible_tris_num, mr.corners_num);
}

static void extract_tris_iter_face_bm(const MeshRenderData &mr,
                                      const BMFace *f,
                                      const int f_index,
                                      void *_data)
{
  int tri_offset = mr.face_sorted->face_tri_offsets[f_index];
  if (tri_offset == -1) {
    return;
  }

  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  int tri_first_index_real = poly_to_tri_count(f_index, BM_elem_index_get(f->l_first));

  BMLoop *(*looptris)[3] = mr.edit_bmesh->looptris;
  int tri_len = f->len - 2;
  for (int offs = 0; offs < tri_len; offs++) {
    BMLoop **elt = looptris[tri_first_index_real + offs];
    int tri_index = tri_offset + offs;
    GPU_indexbuf_set_tri_verts(elb,
                               tri_index,
                               BM_elem_index_get(elt[0]),
                               BM_elem_index_get(elt[1]),
                               BM_elem_index_get(elt[2]));
  }
}

static void extract_tris_iter_face_mesh(const MeshRenderData &mr,
                                        const int face_index,
                                        void *_data)
{
  int tri_offset = mr.face_sorted->face_tri_offsets[face_index];
  if (tri_offset == -1) {
    return;
  }

  const IndexRange face = mr.faces[face_index];

  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  int tri_first_index_real = poly_to_tri_count(face_index, face.start());

  int tri_len = face.size() - 2;
  for (int offs = 0; offs < tri_len; offs++) {
    const int3 &tri = mr.corner_tris[tri_first_index_real + offs];
    int tri_index = tri_offset + offs;
    GPU_indexbuf_set_tri_verts(elb, tri_index, tri[0], tri[1], tri[2]);
  }
}

static void extract_tris_finish(const MeshRenderData &mr,
                                MeshBatchCache &cache,
                                void *buf,
                                void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  GPU_indexbuf_build_in_place(elb, ibo);

  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr.use_final_mesh && cache.tris_per_mat) {
    int mat_start = 0;
    for (int i = 0; i < mr.materials_num; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (cache.tris_per_mat[i] == nullptr) {
        cache.tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      const int mat_tri_len = mr.face_sorted->tris_num_by_material[i];
      /* Multiply by 3 because these are triangle indices. */
      const int start = mat_start * 3;
      const int len = mat_tri_len * 3;
      GPU_indexbuf_create_subrange_in_place(cache.tris_per_mat[i], ibo, start, len);
      mat_start += mat_tri_len;
    }
  }
}

static void extract_tris_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                     const MeshRenderData & /*mr*/,
                                     MeshBatchCache &cache,
                                     void *buffer,
                                     void * /*data*/)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buffer);
  /* Initialize the index buffer, it was already allocated, it will be filled on the device. */
  GPU_indexbuf_init_build_on_device(ibo, subdiv_cache.num_subdiv_triangles * 3);

  if (cache.tris_per_mat) {
    for (int i = 0; i < cache.mat_len; i++) {
      if (cache.tris_per_mat[i] == nullptr) {
        cache.tris_per_mat[i] = GPU_indexbuf_calloc();
      }

      /* Multiply by 6 since we have 2 triangles per quad. */
      const int start = subdiv_cache.mat_start[i] * 6;
      const int len = (subdiv_cache.mat_end[i] - subdiv_cache.mat_start[i]) * 6;
      GPU_indexbuf_create_subrange_in_place(cache.tris_per_mat[i], ibo, start, len);
    }
  }

  draw_subdiv_build_tris_buffer(subdiv_cache, ibo, cache.mat_len);
}

constexpr MeshExtract create_extractor_tris()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tris_init;
  extractor.init_subdiv = extract_tris_init_subdiv;
  extractor.iter_face_bm = extract_tris_iter_face_bm;
  extractor.iter_face_mesh = extract_tris_iter_face_mesh;
  extractor.task_reduce = extract_tris_mat_task_reduce;
  extractor.finish = extract_tris_finish;
  extractor.data_type = MR_DATA_CORNER_TRI | MR_DATA_POLYS_SORTED;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.tris);
  return extractor;
}

/** \} */

/** \name Extract Triangles Indices (single material)
 * \{ */

static void extract_tris_single_mat_init(const MeshRenderData &mr,
                                         MeshBatchCache & /*cache*/,
                                         void * /*ibo*/,
                                         void *tls_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(tls_data);
  GPU_indexbuf_init(elb, GPU_PRIM_TRIS, mr.corner_tris_num, mr.corners_num);
}

static void extract_tris_single_mat_iter_looptri_bm(const MeshRenderData & /*mr*/,
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

static void extract_tris_single_mat_iter_corner_tri_mesh(const MeshRenderData &mr,
                                                         const int3 &tri,
                                                         const int tri_index,
                                                         void *_data)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  const int face_i = mr.corner_tri_faces[tri_index];
  const bool hidden = mr.use_hide && !mr.hide_poly.is_empty() && mr.hide_poly[face_i];
  if (hidden) {
    GPU_indexbuf_set_tri_restart(elb, tri_index);
  }
  else {
    GPU_indexbuf_set_tri_verts(elb, tri_index, tri[0], tri[1], tri[2]);
  }
}

static void extract_tris_single_mat_finish(const MeshRenderData &mr,
                                           MeshBatchCache &cache,
                                           void *buf,
                                           void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  GPU_indexbuf_build_in_place(elb, ibo);

  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr.use_final_mesh && cache.tris_per_mat) {
    for (int i = 0; i < mr.materials_num; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (cache.tris_per_mat[i] == nullptr) {
        cache.tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      /* Multiply by 3 because these are triangle indices. */
      const int len = mr.corner_tris_num * 3;
      GPU_indexbuf_create_subrange_in_place(cache.tris_per_mat[i], ibo, 0, len);
    }
  }
}

constexpr MeshExtract create_extractor_tris_single_mat()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tris_single_mat_init;
  extractor.init_subdiv = extract_tris_init_subdiv;
  extractor.iter_looptri_bm = extract_tris_single_mat_iter_looptri_bm;
  extractor.iter_corner_tri_mesh = extract_tris_single_mat_iter_corner_tri_mesh;
  extractor.task_reduce = extract_tris_mat_task_reduce;
  extractor.finish = extract_tris_single_mat_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(GPUIndexBufBuilder);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.tris);
  return extractor;
}

/** \} */

const MeshExtract extract_tris = create_extractor_tris();
const MeshExtract extract_tris_single_mat = create_extractor_tris_single_mat();

}  // namespace blender::draw
