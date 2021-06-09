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

#include "MEM_guardedalloc.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Triangles Indices (multi material)
 * \{ */

struct MeshExtract_Tri_Data {
  GPUIndexBufBuilder elb;
  int *tri_mat_start;
  int *tri_mat_end;
};

static void *extract_tris_init(const MeshRenderData *mr,
                               struct MeshBatchCache *UNUSED(cache),
                               void *UNUSED(ibo))
{
  MeshExtract_Tri_Data *data = static_cast<MeshExtract_Tri_Data *>(
      MEM_callocN(sizeof(*data), __func__));

  size_t mat_tri_idx_size = sizeof(int) * mr->mat_len;
  data->tri_mat_start = static_cast<int *>(MEM_callocN(mat_tri_idx_size, __func__));
  data->tri_mat_end = static_cast<int *>(MEM_callocN(mat_tri_idx_size, __func__));

  int *mat_tri_len = data->tri_mat_start;
  /* Count how many triangle for each material. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &iter, mr->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
        int mat = min_ii(efa->mat_nr, mr->mat_len - 1);
        mat_tri_len[mat] += efa->len - 2;
      }
    }
  }
  else {
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      if (!(mr->use_hide && (mp->flag & ME_HIDE))) {
        int mat = min_ii(mp->mat_nr, mr->mat_len - 1);
        mat_tri_len[mat] += mp->totloop - 2;
      }
    }
  }
  /* Accumulate triangle lengths per material to have correct offsets. */
  int ofs = mat_tri_len[0];
  mat_tri_len[0] = 0;
  for (int i = 1; i < mr->mat_len; i++) {
    int tmp = mat_tri_len[i];
    mat_tri_len[i] = ofs;
    ofs += tmp;
  }

  memcpy(data->tri_mat_end, mat_tri_len, mat_tri_idx_size);

  int visible_tri_tot = ofs;
  GPU_indexbuf_init(&data->elb, GPU_PRIM_TRIS, visible_tri_tot, mr->loop_len);

  return data;
}

static void extract_tris_iter_looptri_bm(const MeshRenderData *mr,
                                         BMLoop **elt,
                                         const int UNUSED(elt_index),
                                         void *_data)
{
  MeshExtract_Tri_Data *data = static_cast<MeshExtract_Tri_Data *>(_data);
  const int mat_last = mr->mat_len - 1;

  if (!BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN)) {
    int *mat_tri_ofs = data->tri_mat_end;
    const int mat = min_ii(elt[0]->f->mat_nr, mat_last);
    GPU_indexbuf_set_tri_verts(&data->elb,
                               mat_tri_ofs[mat]++,
                               BM_elem_index_get(elt[0]),
                               BM_elem_index_get(elt[1]),
                               BM_elem_index_get(elt[2]));
  }
}

static void extract_tris_iter_looptri_mesh(const MeshRenderData *mr,
                                           const MLoopTri *mlt,
                                           const int UNUSED(elt_index),
                                           void *_data)
{
  MeshExtract_Tri_Data *data = static_cast<MeshExtract_Tri_Data *>(_data);
  const int mat_last = mr->mat_len - 1;
  const MPoly *mp = &mr->mpoly[mlt->poly];
  if (!(mr->use_hide && (mp->flag & ME_HIDE))) {
    int *mat_tri_ofs = data->tri_mat_end;
    const int mat = min_ii(mp->mat_nr, mat_last);
    GPU_indexbuf_set_tri_verts(
        &data->elb, mat_tri_ofs[mat]++, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
  }
}

static void extract_tris_finish(const MeshRenderData *mr,
                                struct MeshBatchCache *cache,
                                void *buf,
                                void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  MeshExtract_Tri_Data *data = static_cast<MeshExtract_Tri_Data *>(_data);
  GPU_indexbuf_build_in_place(&data->elb, ibo);

  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr->use_final_mesh && cache->final.tris_per_mat) {
    MeshBufferCache *mbc_final = &cache->final;
    for (int i = 0; i < mr->mat_len; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (mbc_final->tris_per_mat[i] == nullptr) {
        mbc_final->tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      /* Multiply by 3 because these are triangle indices. */
      const int mat_start = data->tri_mat_start[i];
      const int mat_end = data->tri_mat_end[i];
      const int start = mat_start * 3;
      const int len = (mat_end - mat_start) * 3;
      GPU_indexbuf_create_subrange_in_place(mbc_final->tris_per_mat[i], ibo, start, len);
    }
  }
  MEM_freeN(data->tri_mat_start);
  MEM_freeN(data->tri_mat_end);
  MEM_freeN(data);
}

constexpr MeshExtract create_extractor_tris()
{
  MeshExtract extractor = {0};
  extractor.init = extract_tris_init;
  extractor.iter_looptri_bm = extract_tris_iter_looptri_bm;
  extractor.iter_looptri_mesh = extract_tris_iter_looptri_mesh;
  extractor.finish = extract_tris_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.tris);
  return extractor;
}

/** \} */

/** \name Extract Triangles Indices (single material)
 * \{ */

static void *extract_tris_single_mat_init(const MeshRenderData *mr,
                                          struct MeshBatchCache *UNUSED(cache),
                                          void *UNUSED(ibo))
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(MEM_mallocN(sizeof(*elb), __func__));
  GPU_indexbuf_init(elb, GPU_PRIM_TRIS, mr->tri_len, mr->loop_len);
  return elb;
}

static void *extract_tris_single_mat_task_init(void *_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  GPUIndexBufBuilder *sub_builder = static_cast<GPUIndexBufBuilder *>(
      MEM_mallocN(sizeof(*sub_builder), __func__));
  GPU_indexbuf_subbuilder_init(elb, sub_builder);
  return sub_builder;
}

static void extract_tris_single_mat_iter_looptri_bm(const MeshRenderData *UNUSED(mr),
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
  const MPoly *mp = &mr->mpoly[mlt->poly];
  if (!(mr->use_hide && (mp->flag & ME_HIDE))) {
    GPU_indexbuf_set_tri_verts(elb, mlt_index, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
  }
  else {
    GPU_indexbuf_set_tri_restart(elb, mlt_index);
  }
}

static void extract_tris_single_mat_task_finish(void *_userdata, void *_task_userdata)
{
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_userdata);
  GPUIndexBufBuilder *sub_builder = static_cast<GPUIndexBufBuilder *>(_task_userdata);
  GPU_indexbuf_subbuilder_finish(elb, sub_builder);
  MEM_freeN(sub_builder);
}

static void extract_tris_single_mat_finish(const MeshRenderData *mr,
                                           struct MeshBatchCache *cache,
                                           void *buf,
                                           void *_data)
{
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPUIndexBufBuilder *elb = static_cast<GPUIndexBufBuilder *>(_data);
  GPU_indexbuf_build_in_place(elb, ibo);

  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr->use_final_mesh && cache->final.tris_per_mat) {
    MeshBufferCache *mbc = &cache->final;
    for (int i = 0; i < mr->mat_len; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (mbc->tris_per_mat[i] == NULL) {
        mbc->tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      /* Multiply by 3 because these are triangle indices. */
      const int len = mr->tri_len * 3;
      GPU_indexbuf_create_subrange_in_place(mbc->tris_per_mat[i], ibo, 0, len);
    }
  }
  MEM_freeN(elb);
}

constexpr MeshExtract create_extractor_tris_single_mat()
{
  MeshExtract extractor = {0};
  extractor.init = extract_tris_single_mat_init;
  extractor.task_init = extract_tris_single_mat_task_init;
  extractor.iter_looptri_bm = extract_tris_single_mat_iter_looptri_bm;
  extractor.iter_looptri_mesh = extract_tris_single_mat_iter_looptri_mesh;
  extractor.task_finish = extract_tris_single_mat_task_finish;
  extractor.finish = extract_tris_single_mat_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.tris);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_tris = blender::draw::create_extractor_tris();
const MeshExtract extract_tris_single_mat = blender::draw::create_extractor_tris_single_mat();
}
