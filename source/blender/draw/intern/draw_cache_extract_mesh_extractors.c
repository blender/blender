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
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "MEM_guardedalloc.h"

#include "atomic_ops.h"

#include "DNA_object_types.h"

#include "BLI_edgehash.h"
#include "BLI_jitter_2d.h"
#include "BLI_kdopbvh.h"
#include "BLI_string.h"

#include "BKE_bvhutils.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h"
#include "BKE_paint.h"

#include "ED_uvedit.h"

#include "GPU_capabilities.h"

#include "draw_cache_extract_mesh_private.h"
#include "draw_cache_impl.h"

void *mesh_extract_buffer_get(const MeshExtract *extractor, MeshBufferCache *mbc)
{
  /* NOTE: POINTER_OFFSET on windows platforms casts internally to `void *`, but on GCC/CLANG to
   * `MeshBufferCache *`. What shows a different usage versus intent. */
  void **buffer_ptr = (void **)POINTER_OFFSET(mbc, extractor->mesh_buffer_offset);
  void *buffer = *buffer_ptr;
  BLI_assert(buffer);
  return buffer;
}

eMRIterType mesh_extract_iter_type(const MeshExtract *ext)
{
  eMRIterType type = 0;
  SET_FLAG_FROM_TEST(type, (ext->iter_looptri_bm || ext->iter_looptri_mesh), MR_ITER_LOOPTRI);
  SET_FLAG_FROM_TEST(type, (ext->iter_poly_bm || ext->iter_poly_mesh), MR_ITER_POLY);
  SET_FLAG_FROM_TEST(type, (ext->iter_ledge_bm || ext->iter_ledge_mesh), MR_ITER_LEDGE);
  SET_FLAG_FROM_TEST(type, (ext->iter_lvert_bm || ext->iter_lvert_mesh), MR_ITER_LVERT);
  return type;
}

/* ---------------------------------------------------------------------- */
/** \name Override extractors
 * Extractors can be overridden. When overridden a specialized version is used. The next functions
 * would check for any needed overrides and usage of the specialized version.
 * \{ */

static const MeshExtract *mesh_extract_override_hq_normals(const MeshExtract *extractor)
{
  if (extractor == &extract_pos_nor) {
    return &extract_pos_nor_hq;
  }
  if (extractor == &extract_lnor) {
    return &extract_lnor_hq;
  }
  if (extractor == &extract_tan) {
    return &extract_tan_hq;
  }
  if (extractor == &extract_fdots_nor) {
    return &extract_fdots_nor_hq;
  }
  return extractor;
}

static const MeshExtract *mesh_extract_override_single_material(const MeshExtract *extractor)
{
  if (extractor == &extract_tris) {
    return &extract_tris_single_mat;
  }
  return extractor;
}

const MeshExtract *mesh_extract_override_get(const MeshExtract *extractor,
                                             const bool do_hq_normals,
                                             const bool do_single_mat)
{
  if (do_hq_normals) {
    extractor = mesh_extract_override_hq_normals(extractor);
  }

  if (do_single_mat) {
    extractor = mesh_extract_override_single_material(extractor);
  }

  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Position and Vertex Normal
 * \{ */

typedef struct PosNorLoop {
  float pos[3];
  GPUPackedNormal nor;
} PosNorLoop;

typedef struct MeshExtract_PosNor_Data {
  PosNorLoop *vbo_data;
  GPUNormal normals[];
} MeshExtract_PosNor_Data;

static void *extract_pos_nor_init(const MeshRenderData *mr,
                                  struct MeshBatchCache *UNUSED(cache),
                                  void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING Adjust #PosNorLoop struct accordingly. */
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "vnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  /* Pack normals per vert, reduce amount of computation. */
  size_t packed_nor_len = sizeof(GPUNormal) * mr->vert_len;
  MeshExtract_PosNor_Data *data = MEM_mallocN(sizeof(*data) + packed_nor_len, __func__);
  data->vbo_data = (PosNorLoop *)GPU_vertbuf_get_data(vbo);

  /* Quicker than doing it for each loop. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMVert *eve;
    int v;
    BM_ITER_MESH_INDEX (eve, &iter, mr->bm, BM_VERTS_OF_MESH, v) {
      data->normals[v].low = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, eve));
    }
  }
  else {
    const MVert *mv = mr->mvert;
    for (int v = 0; v < mr->vert_len; v++, mv++) {
      data->normals[v].low = GPU_normal_convert_i10_s3(mv->no);
    }
  }
  return data;
}

static void extract_pos_nor_iter_poly_bm(const MeshRenderData *mr,
                                         BMFace *f,
                                         const int UNUSED(f_index),
                                         void *_data)
{
  MeshExtract_PosNor_Data *data = _data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    PosNorLoop *vert = &data->vbo_data[l_index];
    copy_v3_v3(vert->pos, bm_vert_co_get(mr, l_iter->v));
    vert->nor = data->normals[BM_elem_index_get(l_iter->v)].low;
    vert->nor.w = BM_elem_flag_test(f, BM_ELEM_HIDDEN) ? -1 : 0;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_pos_nor_iter_poly_mesh(const MeshRenderData *mr,
                                           const MPoly *mp,
                                           const int UNUSED(mp_index),
                                           void *_data)
{
  MeshExtract_PosNor_Data *data = _data;

  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    PosNorLoop *vert = &data->vbo_data[ml_index];
    const MVert *mv = &mr->mvert[ml->v];
    copy_v3_v3(vert->pos, mv->co);
    vert->nor = data->normals[ml->v].low;
    /* Flag for paint mode overlay. */
    if (mp->flag & ME_HIDE || mv->flag & ME_HIDE ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->v_origindex) &&
         (mr->v_origindex[ml->v] == ORIGINDEX_NONE))) {
      vert->nor.w = -1;
    }
    else if (mv->flag & SELECT) {
      vert->nor.w = 1;
    }
    else {
      vert->nor.w = 0;
    }
  }
}

static void extract_pos_nor_iter_ledge_bm(const MeshRenderData *mr,
                                          BMEdge *eed,
                                          const int ledge_index,
                                          void *_data)
{
  MeshExtract_PosNor_Data *data = _data;

  int l_index = mr->loop_len + ledge_index * 2;
  PosNorLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert[0].pos, bm_vert_co_get(mr, eed->v1));
  copy_v3_v3(vert[1].pos, bm_vert_co_get(mr, eed->v2));
  vert[0].nor = data->normals[BM_elem_index_get(eed->v1)].low;
  vert[1].nor = data->normals[BM_elem_index_get(eed->v2)].low;
}

static void extract_pos_nor_iter_ledge_mesh(const MeshRenderData *mr,
                                            const MEdge *med,
                                            const int ledge_index,
                                            void *_data)
{
  MeshExtract_PosNor_Data *data = _data;
  const int ml_index = mr->loop_len + ledge_index * 2;
  PosNorLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert[0].pos, mr->mvert[med->v1].co);
  copy_v3_v3(vert[1].pos, mr->mvert[med->v2].co);
  vert[0].nor = data->normals[med->v1].low;
  vert[1].nor = data->normals[med->v2].low;
}

static void extract_pos_nor_iter_lvert_bm(const MeshRenderData *mr,
                                          BMVert *eve,
                                          const int lvert_index,
                                          void *_data)
{
  MeshExtract_PosNor_Data *data = _data;
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int l_index = offset + lvert_index;
  PosNorLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert->pos, bm_vert_co_get(mr, eve));
  vert->nor = data->normals[BM_elem_index_get(eve)].low;
}

static void extract_pos_nor_iter_lvert_mesh(const MeshRenderData *mr,
                                            const MVert *mv,
                                            const int lvert_index,
                                            void *_data)
{
  MeshExtract_PosNor_Data *data = _data;
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int ml_index = offset + lvert_index;
  const int v_index = mr->lverts[lvert_index];
  PosNorLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert->pos, mv->co);
  vert->nor = data->normals[v_index].low;
}

static void extract_pos_nor_finish(const MeshRenderData *UNUSED(mr),
                                   struct MeshBatchCache *UNUSED(cache),
                                   void *UNUSED(buf),
                                   void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_pos_nor = {
    .init = extract_pos_nor_init,
    .iter_poly_bm = extract_pos_nor_iter_poly_bm,
    .iter_poly_mesh = extract_pos_nor_iter_poly_mesh,
    .iter_ledge_bm = extract_pos_nor_iter_ledge_bm,
    .iter_ledge_mesh = extract_pos_nor_iter_ledge_mesh,
    .iter_lvert_bm = extract_pos_nor_iter_lvert_bm,
    .iter_lvert_mesh = extract_pos_nor_iter_lvert_mesh,
    .finish = extract_pos_nor_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.pos_nor),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Position and High Quality Vertex Normal
 * \{ */

typedef struct PosNorHQLoop {
  float pos[3];
  short nor[4];
} PosNorHQLoop;

typedef struct MeshExtract_PosNorHQ_Data {
  PosNorHQLoop *vbo_data;
  GPUNormal normals[];
} MeshExtract_PosNorHQ_Data;

static void *extract_pos_nor_hq_init(const MeshRenderData *mr,
                                     struct MeshBatchCache *UNUSED(cache),
                                     void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING Adjust #PosNorHQLoop struct accordingly. */
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "vnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  /* Pack normals per vert, reduce amount of computation. */
  size_t packed_nor_len = sizeof(GPUNormal) * mr->vert_len;
  MeshExtract_PosNorHQ_Data *data = MEM_mallocN(sizeof(*data) + packed_nor_len, __func__);
  data->vbo_data = (PosNorHQLoop *)GPU_vertbuf_get_data(vbo);

  /* Quicker than doing it for each loop. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMVert *eve;
    int v;
    BM_ITER_MESH_INDEX (eve, &iter, mr->bm, BM_VERTS_OF_MESH, v) {
      normal_float_to_short_v3(data->normals[v].high, bm_vert_no_get(mr, eve));
    }
  }
  else {
    const MVert *mv = mr->mvert;
    for (int v = 0; v < mr->vert_len; v++, mv++) {
      copy_v3_v3_short(data->normals[v].high, mv->no);
    }
  }
  return data;
}

static void extract_pos_nor_hq_iter_poly_bm(const MeshRenderData *mr,
                                            BMFace *f,
                                            const int UNUSED(f_index),
                                            void *_data)
{
  MeshExtract_PosNorHQ_Data *data = _data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    PosNorHQLoop *vert = &data->vbo_data[l_index];
    copy_v3_v3(vert->pos, bm_vert_co_get(mr, l_iter->v));
    copy_v3_v3_short(vert->nor, data->normals[BM_elem_index_get(l_iter->v)].high);

    BMFace *efa = l_iter->f;
    vert->nor[3] = BM_elem_flag_test(efa, BM_ELEM_HIDDEN) ? -1 : 0;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_pos_nor_hq_iter_poly_mesh(const MeshRenderData *mr,
                                              const MPoly *mp,
                                              const int UNUSED(mp_index),
                                              void *_data)
{
  MeshExtract_PosNorHQ_Data *data = _data;
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    PosNorHQLoop *vert = &data->vbo_data[ml_index];
    const MVert *mv = &mr->mvert[ml->v];
    copy_v3_v3(vert->pos, mv->co);
    copy_v3_v3_short(vert->nor, data->normals[ml->v].high);

    /* Flag for paint mode overlay. */
    if (mp->flag & ME_HIDE || mv->flag & ME_HIDE ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->v_origindex) &&
         (mr->v_origindex[ml->v] == ORIGINDEX_NONE))) {
      vert->nor[3] = -1;
    }
    else if (mv->flag & SELECT) {
      vert->nor[3] = 1;
    }
    else {
      vert->nor[3] = 0;
    }
  }
}

static void extract_pos_nor_hq_iter_ledge_bm(const MeshRenderData *mr,
                                             BMEdge *eed,
                                             const int ledge_index,
                                             void *_data)
{
  MeshExtract_PosNorHQ_Data *data = _data;
  int l_index = mr->loop_len + ledge_index * 2;
  PosNorHQLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert[0].pos, bm_vert_co_get(mr, eed->v1));
  copy_v3_v3(vert[1].pos, bm_vert_co_get(mr, eed->v2));
  copy_v3_v3_short(vert[0].nor, data->normals[BM_elem_index_get(eed->v1)].high);
  vert[0].nor[3] = 0;
  copy_v3_v3_short(vert[1].nor, data->normals[BM_elem_index_get(eed->v2)].high);
  vert[1].nor[3] = 0;
}

static void extract_pos_nor_hq_iter_ledge_mesh(const MeshRenderData *mr,
                                               const MEdge *med,
                                               const int ledge_index,
                                               void *_data)
{
  MeshExtract_PosNorHQ_Data *data = _data;
  const int ml_index = mr->loop_len + ledge_index * 2;
  PosNorHQLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert[0].pos, mr->mvert[med->v1].co);
  copy_v3_v3(vert[1].pos, mr->mvert[med->v2].co);
  copy_v3_v3_short(vert[0].nor, data->normals[med->v1].high);
  vert[0].nor[3] = 0;
  copy_v3_v3_short(vert[1].nor, data->normals[med->v2].high);
  vert[1].nor[3] = 0;
}

static void extract_pos_nor_hq_iter_lvert_bm(const MeshRenderData *mr,
                                             BMVert *eve,
                                             const int lvert_index,
                                             void *_data)
{
  MeshExtract_PosNorHQ_Data *data = _data;
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int l_index = offset + lvert_index;
  PosNorHQLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert->pos, bm_vert_co_get(mr, eve));
  copy_v3_v3_short(vert->nor, data->normals[BM_elem_index_get(eve)].high);
  vert->nor[3] = 0;
}

static void extract_pos_nor_hq_iter_lvert_mesh(const MeshRenderData *mr,
                                               const MVert *mv,
                                               const int lvert_index,
                                               void *_data)
{
  MeshExtract_PosNorHQ_Data *data = _data;
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int ml_index = offset + lvert_index;
  const int v_index = mr->lverts[lvert_index];
  PosNorHQLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert->pos, mv->co);
  copy_v3_v3_short(vert->nor, data->normals[v_index].high);
  vert->nor[3] = 0;
}

static void extract_pos_nor_hq_finish(const MeshRenderData *UNUSED(mr),
                                      struct MeshBatchCache *UNUSED(cache),
                                      void *UNUSED(buf),
                                      void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_pos_nor_hq = {
    .init = extract_pos_nor_hq_init,
    .iter_poly_bm = extract_pos_nor_hq_iter_poly_bm,
    .iter_poly_mesh = extract_pos_nor_hq_iter_poly_mesh,
    .iter_ledge_bm = extract_pos_nor_hq_iter_ledge_bm,
    .iter_ledge_mesh = extract_pos_nor_hq_iter_ledge_mesh,
    .iter_lvert_bm = extract_pos_nor_hq_iter_lvert_bm,
    .iter_lvert_mesh = extract_pos_nor_hq_iter_lvert_mesh,
    .finish = extract_pos_nor_hq_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.pos_nor)};

/** \} */
/* ---------------------------------------------------------------------- */
/** \name Extract HQ Loop Normal
 * \{ */

typedef struct gpuHQNor {
  short x, y, z, w;
} gpuHQNor;

static void *extract_lnor_hq_init(const MeshRenderData *mr,
                                  struct MeshBatchCache *UNUSED(cache),
                                  void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return GPU_vertbuf_get_data(vbo);
}

static void extract_lnor_hq_iter_poly_bm(const MeshRenderData *mr,
                                         BMFace *f,
                                         const int UNUSED(f_index),
                                         void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (mr->loop_normals) {
      normal_float_to_short_v3(&((gpuHQNor *)data)[l_index].x, mr->loop_normals[l_index]);
    }
    else {
      if (BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
        normal_float_to_short_v3(&((gpuHQNor *)data)[l_index].x, bm_vert_no_get(mr, l_iter->v));
      }
      else {
        normal_float_to_short_v3(&((gpuHQNor *)data)[l_index].x, bm_face_no_get(mr, f));
      }
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_lnor_hq_iter_poly_mesh(const MeshRenderData *mr,
                                           const MPoly *mp,
                                           const int mp_index,
                                           void *data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    gpuHQNor *lnor_data = &((gpuHQNor *)data)[ml_index];
    if (mr->loop_normals) {
      normal_float_to_short_v3(&lnor_data->x, mr->loop_normals[ml_index]);
    }
    else if (mp->flag & ME_SMOOTH) {
      copy_v3_v3_short(&lnor_data->x, mr->mvert[ml->v].no);
    }
    else {
      normal_float_to_short_v3(&lnor_data->x, mr->poly_normals[mp_index]);
    }

    /* Flag for paint mode overlay.
     * Only use #MR_EXTRACT_MAPPED in edit mode where it is used to display the edge-normals.
     * In paint mode it will use the un-mapped data to draw the wire-frame. */
    if (mp->flag & ME_HIDE || (mr->edit_bmesh && mr->extract_type == MR_EXTRACT_MAPPED &&
                               (mr->v_origindex) && mr->v_origindex[ml->v] == ORIGINDEX_NONE)) {
      lnor_data->w = -1;
    }
    else if (mp->flag & ME_FACE_SEL) {
      lnor_data->w = 1;
    }
    else {
      lnor_data->w = 0;
    }
  }
}

const MeshExtract extract_lnor_hq = {
    .init = extract_lnor_hq_init,
    .iter_poly_bm = extract_lnor_hq_iter_poly_bm,
    .iter_poly_mesh = extract_lnor_hq_iter_poly_mesh,
    .data_type = MR_DATA_LOOP_NOR,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.lnor),
};

/** \} */
/* ---------------------------------------------------------------------- */
/** \name Extract Loop Normal
 * \{ */

static void *extract_lnor_init(const MeshRenderData *mr,
                               struct MeshBatchCache *UNUSED(cache),
                               void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return GPU_vertbuf_get_data(vbo);
}

static void extract_lnor_iter_poly_bm(const MeshRenderData *mr,
                                      BMFace *f,
                                      const int UNUSED(f_index),
                                      void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (mr->loop_normals) {
      ((GPUPackedNormal *)data)[l_index] = GPU_normal_convert_i10_v3(mr->loop_normals[l_index]);
    }
    else {
      if (BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
        ((GPUPackedNormal *)data)[l_index] = GPU_normal_convert_i10_v3(
            bm_vert_no_get(mr, l_iter->v));
      }
      else {
        ((GPUPackedNormal *)data)[l_index] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, f));
      }
    }
    ((GPUPackedNormal *)data)[l_index].w = BM_elem_flag_test(f, BM_ELEM_HIDDEN) ? -1 : 0;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_lnor_iter_poly_mesh(const MeshRenderData *mr,
                                        const MPoly *mp,
                                        const int mp_index,
                                        void *data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    GPUPackedNormal *lnor_data = &((GPUPackedNormal *)data)[ml_index];
    if (mr->loop_normals) {
      *lnor_data = GPU_normal_convert_i10_v3(mr->loop_normals[ml_index]);
    }
    else if (mp->flag & ME_SMOOTH) {
      *lnor_data = GPU_normal_convert_i10_s3(mr->mvert[ml->v].no);
    }
    else {
      *lnor_data = GPU_normal_convert_i10_v3(mr->poly_normals[mp_index]);
    }

    /* Flag for paint mode overlay.
     * Only use MR_EXTRACT_MAPPED in edit mode where it is used to display the edge-normals.
     * In paint mode it will use the un-mapped data to draw the wire-frame. */
    if (mp->flag & ME_HIDE || (mr->edit_bmesh && mr->extract_type == MR_EXTRACT_MAPPED &&
                               (mr->v_origindex) && mr->v_origindex[ml->v] == ORIGINDEX_NONE)) {
      lnor_data->w = -1;
    }
    else if (mp->flag & ME_FACE_SEL) {
      lnor_data->w = 1;
    }
    else {
      lnor_data->w = 0;
    }
  }
}

const MeshExtract extract_lnor = {
    .init = extract_lnor_init,
    .iter_poly_bm = extract_lnor_iter_poly_bm,
    .iter_poly_mesh = extract_lnor_iter_poly_mesh,
    .data_type = MR_DATA_LOOP_NOR,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.lnor),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract UV  layers
 * \{ */

static void *extract_uv_init(const MeshRenderData *mr, struct MeshBatchCache *cache, void *buf)
{
  GPUVertBuf *vbo = buf;
  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  uint32_t uv_layers = cache->cd_used.uv;
  /* HACK to fix T68857 */
  if (mr->extract_type == MR_EXTRACT_BMESH && cache->cd_used.edit_uv == 1) {
    int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
    if (layer != -1) {
      uv_layers |= (1 << layer);
    }
  }

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);

      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* UV layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "u%s", attr_safe_name);
      GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      /* Auto layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
      GPU_vertformat_alias_add(&format, attr_name);
      /* Active render layer name. */
      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "u");
      }
      /* Active display layer name. */
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "au");
        /* Alias to `pos` for edit uvs. */
        GPU_vertformat_alias_add(&format, "pos");
      }
      /* Stencil mask uv layer name. */
      if (i == CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "mu");
      }
    }
  }

  int v_len = mr->loop_len;
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, v_len);

  float(*uv_data)[2] = (float(*)[2])GPU_vertbuf_get_data(vbo);
  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      if (mr->extract_type == MR_EXTRACT_BMESH) {
        int cd_ofs = CustomData_get_n_offset(cd_ldata, CD_MLOOPUV, i);
        BMIter f_iter;
        BMFace *efa;
        BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
          BMLoop *l_iter, *l_first;
          l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
          do {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_ofs);
            memcpy(uv_data, luv->uv, sizeof(*uv_data));
            uv_data++;
          } while ((l_iter = l_iter->next) != l_first);
        }
      }
      else {
        MLoopUV *layer_data = CustomData_get_layer_n(cd_ldata, CD_MLOOPUV, i);
        for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, uv_data++, layer_data++) {
          memcpy(uv_data, layer_data->uv, sizeof(*uv_data));
        }
      }
    }
  }

  return NULL;
}

const MeshExtract extract_uv = {
    .init = extract_uv_init,
    .data_type = 0,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.uv),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Tangent layers
 * \{ */

static void extract_tan_ex_init(const MeshRenderData *mr,
                                struct MeshBatchCache *cache,
                                GPUVertBuf *vbo,
                                const bool do_hq)
{
  GPUVertCompType comp_type = do_hq ? GPU_COMP_I16 : GPU_COMP_I10;
  GPUVertFetchMode fetch_mode = GPU_FETCH_INT_TO_FLOAT_UNIT;

  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  uint32_t tan_layers = cache->cd_used.tan;
  float(*orco)[3] = CustomData_get_layer(cd_vdata, CD_ORCO);
  bool orco_allocated = false;
  const bool use_orco_tan = cache->cd_used.tan_orco != 0;

  int tan_len = 0;
  char tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME];

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (tan_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* Tangent layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "t%s", attr_safe_name);
      GPU_vertformat_attr_add(&format, attr_name, comp_type, 4, fetch_mode);
      /* Active render layer name. */
      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "t");
      }
      /* Active display layer name. */
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "at");
      }

      BLI_strncpy(tangent_names[tan_len++], layer_name, MAX_CUSTOMDATA_LAYER_NAME);
    }
  }
  if (use_orco_tan && orco == NULL) {
    /* If `orco` is not available compute it ourselves */
    orco_allocated = true;
    orco = MEM_mallocN(sizeof(*orco) * mr->vert_len, __func__);

    if (mr->extract_type == MR_EXTRACT_BMESH) {
      BMesh *bm = mr->bm;
      for (int v = 0; v < mr->vert_len; v++) {
        const BMVert *eve = BM_vert_at_index(bm, v);
        /* Exceptional case where #bm_vert_co_get can be avoided, as we want the original coords.
         * not the distorted ones. */
        copy_v3_v3(orco[v], eve->co);
      }
    }
    else {
      const MVert *mv = mr->mvert;
      for (int v = 0; v < mr->vert_len; v++, mv++) {
        copy_v3_v3(orco[v], mv->co);
      }
    }
    BKE_mesh_orco_verts_transform(mr->me, orco, mr->vert_len, 0);
  }

  /* Start Fresh */
  CustomData loop_data;
  CustomData_reset(&loop_data);
  if (tan_len != 0 || use_orco_tan) {
    short tangent_mask = 0;
    bool calc_active_tangent = false;
    if (mr->extract_type == MR_EXTRACT_BMESH) {
      BKE_editmesh_loop_tangent_calc(mr->edit_bmesh,
                                     calc_active_tangent,
                                     tangent_names,
                                     tan_len,
                                     mr->poly_normals,
                                     mr->loop_normals,
                                     orco,
                                     &loop_data,
                                     mr->loop_len,
                                     &tangent_mask);
    }
    else {
      BKE_mesh_calc_loop_tangent_ex(mr->mvert,
                                    mr->mpoly,
                                    mr->poly_len,
                                    mr->mloop,
                                    mr->mlooptri,
                                    mr->tri_len,
                                    cd_ldata,
                                    calc_active_tangent,
                                    tangent_names,
                                    tan_len,
                                    mr->poly_normals,
                                    mr->loop_normals,
                                    orco,
                                    &loop_data,
                                    mr->loop_len,
                                    &tangent_mask);
    }
  }

  if (use_orco_tan) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
    const char *layer_name = CustomData_get_layer_name(&loop_data, CD_TANGENT, 0);
    GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
    BLI_snprintf(attr_name, sizeof(*attr_name), "t%s", attr_safe_name);
    GPU_vertformat_attr_add(&format, attr_name, comp_type, 4, fetch_mode);
    GPU_vertformat_alias_add(&format, "t");
    GPU_vertformat_alias_add(&format, "at");
  }

  if (orco_allocated) {
    MEM_SAFE_FREE(orco);
  }

  int v_len = mr->loop_len;
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, v_len);

  if (do_hq) {
    short(*tan_data)[4] = (short(*)[4])GPU_vertbuf_get_data(vbo);
    for (int i = 0; i < tan_len; i++) {
      const char *name = tangent_names[i];
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_named(
          &loop_data, CD_TANGENT, name);
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++) {
        normal_float_to_short_v3(*tan_data, layer_data[ml_index]);
        (*tan_data)[3] = (layer_data[ml_index][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        tan_data++;
      }
    }
    if (use_orco_tan) {
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_n(&loop_data, CD_TANGENT, 0);
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++) {
        normal_float_to_short_v3(*tan_data, layer_data[ml_index]);
        (*tan_data)[3] = (layer_data[ml_index][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        tan_data++;
      }
    }
  }
  else {
    GPUPackedNormal *tan_data = (GPUPackedNormal *)GPU_vertbuf_get_data(vbo);
    for (int i = 0; i < tan_len; i++) {
      const char *name = tangent_names[i];
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_named(
          &loop_data, CD_TANGENT, name);
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++) {
        *tan_data = GPU_normal_convert_i10_v3(layer_data[ml_index]);
        tan_data->w = (layer_data[ml_index][3] > 0.0f) ? 1 : -2;
        tan_data++;
      }
    }
    if (use_orco_tan) {
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_n(&loop_data, CD_TANGENT, 0);
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++) {
        *tan_data = GPU_normal_convert_i10_v3(layer_data[ml_index]);
        tan_data->w = (layer_data[ml_index][3] > 0.0f) ? 1 : -2;
        tan_data++;
      }
    }
  }

  CustomData_free(&loop_data, mr->loop_len);
}

static void *extract_tan_init(const MeshRenderData *mr, struct MeshBatchCache *cache, void *buf)
{
  extract_tan_ex_init(mr, cache, buf, false);
  return NULL;
}

const MeshExtract extract_tan = {
    .init = extract_tan_init,
    .data_type = MR_DATA_POLY_NOR | MR_DATA_TAN_LOOP_NOR | MR_DATA_LOOPTRI,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.tan),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract HQ Tangent layers
 * \{ */

static void *extract_tan_hq_init(const MeshRenderData *mr, struct MeshBatchCache *cache, void *buf)
{
  extract_tan_ex_init(mr, cache, buf, true);
  return NULL;
}

const MeshExtract extract_tan_hq = {
    .init = extract_tan_hq_init,
    .data_type = MR_DATA_POLY_NOR | MR_DATA_TAN_LOOP_NOR | MR_DATA_LOOPTRI,
    .use_threading = false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Sculpt Data
 * \{ */

static void *extract_sculpt_data_init(const MeshRenderData *mr,
                                      struct MeshBatchCache *UNUSED(cache),
                                      void *buf)
{
  GPUVertBuf *vbo = buf;
  GPUVertFormat format = {0};

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  CustomData *cd_pdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->pdata : &mr->me->pdata;

  float *cd_mask = CustomData_get_layer(cd_vdata, CD_PAINT_MASK);
  int *cd_face_set = CustomData_get_layer(cd_pdata, CD_SCULPT_FACE_SETS);

  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "fset", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_attr_add(&format, "msk", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  typedef struct gpuSculptData {
    uint8_t face_set_color[4];
    float mask;
  } gpuSculptData;

  gpuSculptData *vbo_data = (gpuSculptData *)GPU_vertbuf_get_data(vbo);
  MLoop *loops = CustomData_get_layer(cd_ldata, CD_MLOOP);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    int cd_mask_ofs = CustomData_get_offset(cd_vdata, CD_PAINT_MASK);
    int cd_face_set_ofs = CustomData_get_offset(cd_pdata, CD_SCULPT_FACE_SETS);
    BMIter f_iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        float v_mask = 0.0f;
        if (cd_mask) {
          v_mask = BM_ELEM_CD_GET_FLOAT(l_iter->v, cd_mask_ofs);
        }
        vbo_data->mask = v_mask;
        uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        if (cd_face_set) {
          const int face_set_id = BM_ELEM_CD_GET_INT(l_iter->f, cd_face_set_ofs);
          if (face_set_id != mr->me->face_sets_color_default) {
            BKE_paint_face_set_overlay_color_get(
                face_set_id, mr->me->face_sets_color_seed, face_set_color);
          }
        }
        copy_v3_v3_uchar(vbo_data->face_set_color, face_set_color);
        vbo_data++;
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    int mp_loop = 0;
    for (int mp_index = 0; mp_index < mr->poly_len; mp_index++) {
      const MPoly *p = &mr->mpoly[mp_index];
      for (int l = 0; l < p->totloop; l++) {
        float v_mask = 0.0f;
        if (cd_mask) {
          v_mask = cd_mask[loops[mp_loop].v];
        }
        vbo_data->mask = v_mask;

        uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        if (cd_face_set) {
          const int face_set_id = cd_face_set[mp_index];
          /* Skip for the default color Face Set to render it white. */
          if (face_set_id != mr->me->face_sets_color_default) {
            BKE_paint_face_set_overlay_color_get(
                face_set_id, mr->me->face_sets_color_seed, face_set_color);
          }
        }
        copy_v3_v3_uchar(vbo_data->face_set_color, face_set_color);
        mp_loop++;
        vbo_data++;
      }
    }
  }

  return NULL;
}

const MeshExtract extract_sculpt_data = {
    .init = extract_sculpt_data_init,
    .data_type = 0,
    /* TODO: enable threading. */
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.sculpt_data)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract VCol
 * \{ */

static void *extract_vcol_init(const MeshRenderData *mr, struct MeshBatchCache *cache, void *buf)
{
  GPUVertBuf *vbo = buf;
  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  uint32_t vcol_layers = cache->cd_used.vcol;
  uint32_t svcol_layers = cache->cd_used.sculpt_vcol;

  for (int i = 0; i < MAX_MCOL; i++) {
    if (vcol_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

      BLI_snprintf(attr_name, sizeof(attr_name), "c%s", attr_safe_name);
      GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPCOL)) {
        GPU_vertformat_alias_add(&format, "c");
      }
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL)) {
        GPU_vertformat_alias_add(&format, "ac");
      }

      /* Gather number of auto layers. */
      /* We only do `vcols` that are not overridden by `uvs` and sculpt vertex colors. */
      if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, layer_name) == -1 &&
          CustomData_get_named_layer_index(cd_vdata, CD_PROP_COLOR, layer_name) == -1) {
        BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
        GPU_vertformat_alias_add(&format, attr_name);
      }
    }
  }

  /* Sculpt Vertex Colors */
  if (U.experimental.use_sculpt_vertex_colors) {
    for (int i = 0; i < 8; i++) {
      if (svcol_layers & (1 << i)) {
        char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
        const char *layer_name = CustomData_get_layer_name(cd_vdata, CD_PROP_COLOR, i);
        GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

        BLI_snprintf(attr_name, sizeof(attr_name), "c%s", attr_safe_name);
        GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

        if (i == CustomData_get_render_layer(cd_vdata, CD_PROP_COLOR)) {
          GPU_vertformat_alias_add(&format, "c");
        }
        if (i == CustomData_get_active_layer(cd_vdata, CD_PROP_COLOR)) {
          GPU_vertformat_alias_add(&format, "ac");
        }
        /* Gather number of auto layers. */
        /* We only do `vcols` that are not overridden by `uvs`. */
        if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, layer_name) == -1) {
          BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
          GPU_vertformat_alias_add(&format, attr_name);
        }
      }
    }
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  typedef struct gpuMeshVcol {
    ushort r, g, b, a;
  } gpuMeshVcol;

  gpuMeshVcol *vcol_data = (gpuMeshVcol *)GPU_vertbuf_get_data(vbo);
  MLoop *loops = CustomData_get_layer(cd_ldata, CD_MLOOP);

  for (int i = 0; i < MAX_MCOL; i++) {
    if (vcol_layers & (1 << i)) {
      if (mr->extract_type == MR_EXTRACT_BMESH) {
        int cd_ofs = CustomData_get_n_offset(cd_ldata, CD_MLOOPCOL, i);
        BMIter f_iter;
        BMFace *efa;
        BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
          BMLoop *l_iter, *l_first;
          l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
          do {
            const MLoopCol *mloopcol = BM_ELEM_CD_GET_VOID_P(l_iter, cd_ofs);
            vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
            vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
            vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
            vcol_data->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
            vcol_data++;
          } while ((l_iter = l_iter->next) != l_first);
        }
      }
      else {
        const MLoopCol *mloopcol = (MLoopCol *)CustomData_get_layer_n(cd_ldata, CD_MLOOPCOL, i);
        for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, mloopcol++, vcol_data++) {
          vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
          vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
          vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
          vcol_data->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
        }
      }
    }

    if (svcol_layers & (1 << i) && U.experimental.use_sculpt_vertex_colors) {
      if (mr->extract_type == MR_EXTRACT_BMESH) {
        int cd_ofs = CustomData_get_n_offset(cd_vdata, CD_PROP_COLOR, i);
        BMIter f_iter;
        BMFace *efa;
        BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
          BMLoop *l_iter, *l_first;
          l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
          do {
            const MPropCol *prop_col = BM_ELEM_CD_GET_VOID_P(l_iter->v, cd_ofs);
            vcol_data->r = unit_float_to_ushort_clamp(prop_col->color[0]);
            vcol_data->g = unit_float_to_ushort_clamp(prop_col->color[1]);
            vcol_data->b = unit_float_to_ushort_clamp(prop_col->color[2]);
            vcol_data->a = unit_float_to_ushort_clamp(prop_col->color[3]);
            vcol_data++;
          } while ((l_iter = l_iter->next) != l_first);
        }
      }
      else {
        MPropCol *vcol = CustomData_get_layer_n(cd_vdata, CD_PROP_COLOR, i);
        for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vcol_data++) {
          vcol_data->r = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[0]);
          vcol_data->g = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[1]);
          vcol_data->b = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[2]);
          vcol_data->a = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[3]);
        }
      }
    }
  }
  return NULL;
}

const MeshExtract extract_vcol = {
    .init = extract_vcol_init,
    .data_type = 0,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.vcol),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Orco
 * \{ */

typedef struct MeshExtract_Orco_Data {
  float (*vbo_data)[4];
  float (*orco)[3];
} MeshExtract_Orco_Data;

static void *extract_orco_init(const MeshRenderData *mr,
                               struct MeshBatchCache *UNUSED(cache),
                               void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
     * attributes. This is a substantial waste of video-ram and should be done another way.
     * Unfortunately, at the time of writing, I did not found any other "non disruptive"
     * alternative. */
    GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  CustomData *cd_vdata = &mr->me->vdata;

  MeshExtract_Orco_Data *data = MEM_mallocN(sizeof(*data), __func__);
  data->vbo_data = (float(*)[4])GPU_vertbuf_get_data(vbo);
  data->orco = CustomData_get_layer(cd_vdata, CD_ORCO);
  /* Make sure `orco` layer was requested only if needed! */
  BLI_assert(data->orco);
  return data;
}

static void extract_orco_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                      BMFace *f,
                                      const int UNUSED(f_index),
                                      void *data)
{
  MeshExtract_Orco_Data *orco_data = (MeshExtract_Orco_Data *)data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    float *loop_orco = orco_data->vbo_data[l_index];
    copy_v3_v3(loop_orco, orco_data->orco[BM_elem_index_get(l_iter->v)]);
    loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_orco_iter_poly_mesh(const MeshRenderData *mr,
                                        const MPoly *mp,
                                        const int UNUSED(mp_index),
                                        void *data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    MeshExtract_Orco_Data *orco_data = (MeshExtract_Orco_Data *)data;
    float *loop_orco = orco_data->vbo_data[ml_index];
    copy_v3_v3(loop_orco, orco_data->orco[ml->v]);
    loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
  }
}

static void extract_orco_finish(const MeshRenderData *UNUSED(mr),
                                struct MeshBatchCache *UNUSED(cache),
                                void *UNUSED(buf),
                                void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_orco = {
    .init = extract_orco_init,
    .iter_poly_bm = extract_orco_iter_poly_bm,
    .iter_poly_mesh = extract_orco_iter_poly_mesh,
    .finish = extract_orco_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.orco),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edge Factor
 * Defines how much an edge is visible.
 * \{ */

typedef struct MeshExtract_EdgeFac_Data {
  uchar *vbo_data;
  bool use_edge_render;
  /* Number of loop per edge. */
  uchar edge_loop_count[0];
} MeshExtract_EdgeFac_Data;

static float loop_edge_factor_get(const float f_no[3],
                                  const float v_co[3],
                                  const float v_no[3],
                                  const float v_next_co[3])
{
  float enor[3], evec[3];
  sub_v3_v3v3(evec, v_next_co, v_co);
  cross_v3_v3v3(enor, v_no, evec);
  normalize_v3(enor);
  float d = fabsf(dot_v3v3(enor, f_no));
  /* Re-scale to the slider range. */
  d *= (1.0f / 0.065f);
  CLAMP(d, 0.0f, 1.0f);
  return d;
}

static void *extract_edge_fac_init(const MeshRenderData *mr,
                                   struct MeshBatchCache *UNUSED(cache),
                                   void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_EdgeFac_Data *data;

  if (mr->extract_type == MR_EXTRACT_MESH) {
    size_t edge_loop_count_size = sizeof(uint32_t) * mr->edge_len;
    data = MEM_callocN(sizeof(*data) + edge_loop_count_size, __func__);

    /* HACK(fclem) Detecting the need for edge render.
     * We could have a flag in the mesh instead or check the modifier stack. */
    const MEdge *med = mr->medge;
    for (int e_index = 0; e_index < mr->edge_len; e_index++, med++) {
      if ((med->flag & ME_EDGERENDER) == 0) {
        data->use_edge_render = true;
        break;
      }
    }
  }
  else {
    data = MEM_callocN(sizeof(*data), __func__);
    /* HACK to bypass non-manifold check in mesh_edge_fac_finish(). */
    data->use_edge_render = true;
  }

  data->vbo_data = GPU_vertbuf_get_data(vbo);
  return data;
}

static void extract_edge_fac_iter_poly_bm(const MeshRenderData *mr,
                                          BMFace *f,
                                          const int UNUSED(f_index),
                                          void *_data)
{
  MeshExtract_EdgeFac_Data *data = _data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    if (BM_edge_is_manifold(l_iter->e)) {
      float ratio = loop_edge_factor_get(bm_face_no_get(mr, f),
                                         bm_vert_co_get(mr, l_iter->v),
                                         bm_vert_no_get(mr, l_iter->v),
                                         bm_vert_co_get(mr, l_iter->next->v));
      data->vbo_data[l_index] = ratio * 253 + 1;
    }
    else {
      data->vbo_data[l_index] = 255;
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_fac_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int mp_index,
                                            void *_data)
{
  MeshExtract_EdgeFac_Data *data = (MeshExtract_EdgeFac_Data *)_data;

  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    if (data->use_edge_render) {
      const MEdge *med = &mr->medge[ml->e];
      data->vbo_data[ml_index] = (med->flag & ME_EDGERENDER) ? 255 : 0;
    }
    else {

      /* Count loop per edge to detect non-manifold. */
      if (data->edge_loop_count[ml->e] < 3) {
        data->edge_loop_count[ml->e]++;
      }
      if (data->edge_loop_count[ml->e] == 2) {
        /* Manifold */
        const int ml_index_last = mp->totloop + mp->loopstart - 1;
        const int ml_index_other = (ml_index == ml_index_last) ? mp->loopstart : (ml_index + 1);
        const MLoop *ml_next = &mr->mloop[ml_index_other];
        const MVert *v1 = &mr->mvert[ml->v];
        const MVert *v2 = &mr->mvert[ml_next->v];
        float vnor_f[3];
        normal_short_to_float_v3(vnor_f, v1->no);
        float ratio = loop_edge_factor_get(mr->poly_normals[mp_index], v1->co, vnor_f, v2->co);
        data->vbo_data[ml_index] = ratio * 253 + 1;
      }
      else {
        /* Non-manifold */
        data->vbo_data[ml_index] = 255;
      }
    }
  }
}

static void extract_edge_fac_iter_ledge_bm(const MeshRenderData *mr,
                                           BMEdge *UNUSED(eed),
                                           const int ledge_index,
                                           void *_data)
{
  MeshExtract_EdgeFac_Data *data = _data;
  data->vbo_data[mr->loop_len + (ledge_index * 2) + 0] = 255;
  data->vbo_data[mr->loop_len + (ledge_index * 2) + 1] = 255;
}

static void extract_edge_fac_iter_ledge_mesh(const MeshRenderData *mr,
                                             const MEdge *UNUSED(med),
                                             const int ledge_index,
                                             void *_data)
{
  MeshExtract_EdgeFac_Data *data = _data;

  data->vbo_data[mr->loop_len + ledge_index * 2 + 0] = 255;
  data->vbo_data[mr->loop_len + ledge_index * 2 + 1] = 255;
}

static void extract_edge_fac_finish(const MeshRenderData *mr,
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *buf,
                                    void *_data)
{
  GPUVertBuf *vbo = buf;
  MeshExtract_EdgeFac_Data *data = _data;

  if (GPU_crappy_amd_driver()) {
    /* Some AMD drivers strangely crash with VBO's with a one byte format.
     * To workaround we reinitialize the VBO with another format and convert
     * all bytes to floats. */
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    /* We keep the data reference in data->vbo_data. */
    data->vbo_data = GPU_vertbuf_steal_data(vbo);
    GPU_vertbuf_clear(vbo);

    int buf_len = mr->loop_len + mr->loop_loose_len;
    GPU_vertbuf_init_with_format(vbo, &format);
    GPU_vertbuf_data_alloc(vbo, buf_len);

    float *fdata = (float *)GPU_vertbuf_get_data(vbo);
    for (int ml_index = 0; ml_index < buf_len; ml_index++, fdata++) {
      *fdata = data->vbo_data[ml_index] / 255.0f;
    }
    /* Free old byte data. */
    MEM_freeN(data->vbo_data);
  }
  MEM_freeN(data);
}

const MeshExtract extract_edge_fac = {
    .init = extract_edge_fac_init,
    .iter_poly_bm = extract_edge_fac_iter_poly_bm,
    .iter_poly_mesh = extract_edge_fac_iter_poly_mesh,
    .iter_ledge_bm = extract_edge_fac_iter_ledge_bm,
    .iter_ledge_mesh = extract_edge_fac_iter_ledge_mesh,
    .finish = extract_edge_fac_finish,
    .data_type = MR_DATA_POLY_NOR,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.edge_fac)};

/** \} */
/* ---------------------------------------------------------------------- */
/** \name Extract Vertex Weight
 * \{ */

typedef struct MeshExtract_Weight_Data {
  float *vbo_data;
  const DRW_MeshWeightState *wstate;
  const MDeformVert *dvert; /* For #Mesh. */
  int cd_ofs;               /* For #BMesh. */
} MeshExtract_Weight_Data;

static float evaluate_vertex_weight(const MDeformVert *dvert, const DRW_MeshWeightState *wstate)
{
  /* Error state. */
  if ((wstate->defgroup_active < 0) && (wstate->defgroup_len > 0)) {
    return -2.0f;
  }
  if (dvert == NULL) {
    return (wstate->alert_mode != OB_DRAW_GROUPUSER_NONE) ? -1.0f : 0.0f;
  }

  float input = 0.0f;
  if (wstate->flags & DRW_MESH_WEIGHT_STATE_MULTIPAINT) {
    /* Multi-Paint feature */
    bool is_normalized = (wstate->flags & (DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE |
                                           DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE));
    input = BKE_defvert_multipaint_collective_weight(dvert,
                                                     wstate->defgroup_len,
                                                     wstate->defgroup_sel,
                                                     wstate->defgroup_sel_count,
                                                     is_normalized);
    /* make it black if the selected groups have no weight on a vertex */
    if (input == 0.0f) {
      return -1.0f;
    }
  }
  else {
    /* default, non tricky behavior */
    input = BKE_defvert_find_weight(dvert, wstate->defgroup_active);

    if (input == 0.0f) {
      switch (wstate->alert_mode) {
        case OB_DRAW_GROUPUSER_ACTIVE:
          return -1.0f;
          break;
        case OB_DRAW_GROUPUSER_ALL:
          if (BKE_defvert_is_weight_zero(dvert, wstate->defgroup_len)) {
            return -1.0f;
          }
          break;
      }
    }
  }

  /* Lock-Relative: display the fraction of current weight vs total unlocked weight. */
  if (wstate->flags & DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE) {
    input = BKE_defvert_lock_relative_weight(
        input, dvert, wstate->defgroup_len, wstate->defgroup_locked, wstate->defgroup_unlocked);
  }

  CLAMP(input, 0.0f, 1.0f);
  return input;
}

static void *extract_weights_init(const MeshRenderData *mr,
                                  struct MeshBatchCache *cache,
                                  void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_Weight_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (float *)GPU_vertbuf_get_data(vbo);
  data->wstate = &cache->weight_state;

  if (data->wstate->defgroup_active == -1) {
    /* Nothing to show. */
    data->dvert = NULL;
    data->cd_ofs = -1;
  }
  else if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->dvert = NULL;
    data->cd_ofs = CustomData_get_offset(&mr->bm->vdata, CD_MDEFORMVERT);
  }
  else {
    data->dvert = CustomData_get_layer(&mr->me->vdata, CD_MDEFORMVERT);
    data->cd_ofs = -1;
  }
  return data;
}

static void extract_weights_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                         BMFace *f,
                                         const int UNUSED(f_index),
                                         void *_data)
{
  MeshExtract_Weight_Data *data = _data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (data->cd_ofs != -1) {
      const MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(l_iter->v, data->cd_ofs);
      data->vbo_data[l_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
    else {
      data->vbo_data[l_index] = evaluate_vertex_weight(NULL, data->wstate);
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_weights_iter_poly_mesh(const MeshRenderData *mr,
                                           const MPoly *mp,
                                           const int UNUSED(mp_index),
                                           void *_data)
{
  MeshExtract_Weight_Data *data = _data;
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    if (data->dvert != NULL) {
      const MDeformVert *dvert = &data->dvert[ml->v];
      data->vbo_data[ml_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
    else {
      const MDeformVert *dvert = NULL;
      data->vbo_data[ml_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
  }
}

static void extract_weights_finish(const MeshRenderData *UNUSED(mr),
                                   struct MeshBatchCache *UNUSED(cache),
                                   void *UNUSED(buf),
                                   void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_weights = {
    .init = extract_weights_init,
    .iter_poly_bm = extract_weights_iter_poly_bm,
    .iter_poly_mesh = extract_weights_iter_poly_mesh,
    .finish = extract_weights_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.weights),
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit Mode Data / Flags
 * \{ */

typedef struct EditLoopData {
  uchar v_flag;
  uchar e_flag;
  uchar crease;
  uchar bweight;
} EditLoopData;

static void mesh_render_data_face_flag(const MeshRenderData *mr,
                                       BMFace *efa,
                                       const int cd_ofs,
                                       EditLoopData *eattr)
{
  if (efa == mr->efa_act) {
    eattr->v_flag |= VFLAG_FACE_ACTIVE;
  }
  if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    eattr->v_flag |= VFLAG_FACE_SELECTED;
  }

  if (efa == mr->efa_act_uv) {
    eattr->v_flag |= VFLAG_FACE_UV_ACTIVE;
  }
  if ((cd_ofs != -1) && uvedit_face_select_test_ex(mr->toolsettings, (BMFace *)efa, cd_ofs)) {
    eattr->v_flag |= VFLAG_FACE_UV_SELECT;
  }

#ifdef WITH_FREESTYLE
  if (mr->freestyle_face_ofs != -1) {
    const FreestyleFace *ffa = BM_ELEM_CD_GET_VOID_P(efa, mr->freestyle_face_ofs);
    if (ffa->flag & FREESTYLE_FACE_MARK) {
      eattr->v_flag |= VFLAG_FACE_FREESTYLE;
    }
  }
#endif
}

static void mesh_render_data_edge_flag(const MeshRenderData *mr, BMEdge *eed, EditLoopData *eattr)
{
  const ToolSettings *ts = mr->toolsettings;
  const bool is_vertex_select_mode = (ts != NULL) && (ts->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool is_face_only_select_mode = (ts != NULL) && (ts->selectmode == SCE_SELECT_FACE);

  if (eed == mr->eed_act) {
    eattr->e_flag |= VFLAG_EDGE_ACTIVE;
  }
  if (!is_vertex_select_mode && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_EDGE_SELECTED;
  }
  if (is_vertex_select_mode && BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) &&
      BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_EDGE_SELECTED;
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
  if (BM_elem_flag_test(eed, BM_ELEM_SEAM)) {
    eattr->e_flag |= VFLAG_EDGE_SEAM;
  }
  if (!BM_elem_flag_test(eed, BM_ELEM_SMOOTH)) {
    eattr->e_flag |= VFLAG_EDGE_SHARP;
  }

  /* Use active edge color for active face edges because
   * specular highlights make it hard to see T55456#510873.
   *
   * This isn't ideal since it can't be used when mixing edge/face modes
   * but it's still better than not being able to see the active face. */
  if (is_face_only_select_mode) {
    if (mr->efa_act != NULL) {
      if (BM_edge_in_face(eed, mr->efa_act)) {
        eattr->e_flag |= VFLAG_EDGE_ACTIVE;
      }
    }
  }

  /* Use a byte for value range */
  if (mr->crease_ofs != -1) {
    float crease = BM_ELEM_CD_GET_FLOAT(eed, mr->crease_ofs);
    if (crease > 0) {
      eattr->crease = (uchar)(crease * 255.0f);
    }
  }
  /* Use a byte for value range */
  if (mr->bweight_ofs != -1) {
    float bweight = BM_ELEM_CD_GET_FLOAT(eed, mr->bweight_ofs);
    if (bweight > 0) {
      eattr->bweight = (uchar)(bweight * 255.0f);
    }
  }
#ifdef WITH_FREESTYLE
  if (mr->freestyle_edge_ofs != -1) {
    const FreestyleEdge *fed = BM_ELEM_CD_GET_VOID_P(eed, mr->freestyle_edge_ofs);
    if (fed->flag & FREESTYLE_EDGE_MARK) {
      eattr->e_flag |= VFLAG_EDGE_FREESTYLE;
    }
  }
#endif
}

static void mesh_render_data_loop_flag(const MeshRenderData *mr,
                                       BMLoop *l,
                                       const int cd_ofs,
                                       EditLoopData *eattr)
{
  if (cd_ofs == -1) {
    return;
  }
  MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_ofs);
  if (luv != NULL && (luv->flag & MLOOPUV_PINNED)) {
    eattr->v_flag |= VFLAG_VERT_UV_PINNED;
  }
  if (uvedit_uv_select_test_ex(mr->toolsettings, l, cd_ofs)) {
    eattr->v_flag |= VFLAG_VERT_UV_SELECT;
  }
}

static void mesh_render_data_loop_edge_flag(const MeshRenderData *mr,
                                            BMLoop *l,
                                            const int cd_ofs,
                                            EditLoopData *eattr)
{
  if (cd_ofs == -1) {
    return;
  }
  if (uvedit_edge_select_test_ex(mr->toolsettings, l, cd_ofs)) {
    eattr->v_flag |= VFLAG_EDGE_UV_SELECT;
    eattr->v_flag |= VFLAG_VERT_UV_SELECT;
  }
}

static void mesh_render_data_vert_flag(const MeshRenderData *mr, BMVert *eve, EditLoopData *eattr)
{
  if (eve == mr->eve_act) {
    eattr->e_flag |= VFLAG_VERT_ACTIVE;
  }
  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
}

static void *extract_edit_data_init(const MeshRenderData *mr,
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING: Adjust #EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);
  return GPU_vertbuf_get_data(vbo);
}

static void extract_edit_data_iter_poly_bm(const MeshRenderData *mr,
                                           BMFace *f,
                                           const int UNUSED(f_index),
                                           void *_data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    EditLoopData *data = (EditLoopData *)_data + l_index;
    memset(data, 0x0, sizeof(*data));
    mesh_render_data_face_flag(mr, f, -1, data);
    mesh_render_data_edge_flag(mr, l_iter->e, data);
    mesh_render_data_vert_flag(mr, l_iter->v, data);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edit_data_iter_poly_mesh(const MeshRenderData *mr,
                                             const MPoly *mp,
                                             const int mp_index,
                                             void *_data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    EditLoopData *data = (EditLoopData *)_data + ml_index;
    memset(data, 0x0, sizeof(*data));
    BMFace *efa = bm_original_face_get(mr, mp_index);
    BMEdge *eed = bm_original_edge_get(mr, ml->e);
    BMVert *eve = bm_original_vert_get(mr, ml->v);
    if (efa) {
      mesh_render_data_face_flag(mr, efa, -1, data);
    }
    if (eed) {
      mesh_render_data_edge_flag(mr, eed, data);
    }
    if (eve) {
      mesh_render_data_vert_flag(mr, eve, data);
    }
  }
}

static void extract_edit_data_iter_ledge_bm(const MeshRenderData *mr,
                                            BMEdge *eed,
                                            const int ledge_index,
                                            void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + mr->loop_len + (ledge_index * 2);
  memset(data, 0x0, sizeof(*data) * 2);
  mesh_render_data_edge_flag(mr, eed, &data[0]);
  data[1] = data[0];
  mesh_render_data_vert_flag(mr, eed->v1, &data[0]);
  mesh_render_data_vert_flag(mr, eed->v2, &data[1]);
}

static void extract_edit_data_iter_ledge_mesh(const MeshRenderData *mr,
                                              const MEdge *med,
                                              const int ledge_index,
                                              void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + mr->loop_len + ledge_index * 2;
  memset(data, 0x0, sizeof(*data) * 2);
  const int e_index = mr->ledges[ledge_index];
  BMEdge *eed = bm_original_edge_get(mr, e_index);
  BMVert *eve1 = bm_original_vert_get(mr, med->v1);
  BMVert *eve2 = bm_original_vert_get(mr, med->v2);
  if (eed) {
    mesh_render_data_edge_flag(mr, eed, &data[0]);
    data[1] = data[0];
  }
  if (eve1) {
    mesh_render_data_vert_flag(mr, eve1, &data[0]);
  }
  if (eve2) {
    mesh_render_data_vert_flag(mr, eve2, &data[1]);
  }
}

static void extract_edit_data_iter_lvert_bm(const MeshRenderData *mr,
                                            BMVert *eve,
                                            const int lvert_index,
                                            void *_data)
{
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);
  EditLoopData *data = (EditLoopData *)_data + offset + lvert_index;
  memset(data, 0x0, sizeof(*data));
  mesh_render_data_vert_flag(mr, eve, data);
}

static void extract_edit_data_iter_lvert_mesh(const MeshRenderData *mr,
                                              const MVert *UNUSED(mv),
                                              const int lvert_index,
                                              void *_data)
{
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  EditLoopData *data = (EditLoopData *)_data + offset + lvert_index;
  memset(data, 0x0, sizeof(*data));
  const int v_index = mr->lverts[lvert_index];
  BMVert *eve = bm_original_vert_get(mr, v_index);
  if (eve) {
    mesh_render_data_vert_flag(mr, eve, data);
  }
}

const MeshExtract extract_edit_data = {
    .init = extract_edit_data_init,
    .iter_poly_bm = extract_edit_data_iter_poly_bm,
    .iter_poly_mesh = extract_edit_data_iter_poly_mesh,
    .iter_ledge_bm = extract_edit_data_iter_ledge_bm,
    .iter_ledge_mesh = extract_edit_data_iter_ledge_mesh,
    .iter_lvert_bm = extract_edit_data_iter_lvert_bm,
    .iter_lvert_mesh = extract_edit_data_iter_lvert_mesh,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.edit_data)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Data / Flags
 * \{ */

typedef struct MeshExtract_EditUVData_Data {
  EditLoopData *vbo_data;
  int cd_ofs;
} MeshExtract_EditUVData_Data;

static void *extract_edituv_data_init(const MeshRenderData *mr,
                                      struct MeshBatchCache *UNUSED(cache),
                                      void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING: Adjust #EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;

  MeshExtract_EditUVData_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (EditLoopData *)GPU_vertbuf_get_data(vbo);
  data->cd_ofs = CustomData_get_offset(cd_ldata, CD_MLOOPUV);
  return data;
}

static void extract_edituv_data_iter_poly_bm(const MeshRenderData *mr,
                                             BMFace *f,
                                             const int UNUSED(f_index),
                                             void *_data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    MeshExtract_EditUVData_Data *data = _data;
    EditLoopData *eldata = &data->vbo_data[l_index];
    memset(eldata, 0x0, sizeof(*eldata));
    mesh_render_data_loop_flag(mr, l_iter, data->cd_ofs, eldata);
    mesh_render_data_face_flag(mr, f, data->cd_ofs, eldata);
    mesh_render_data_loop_edge_flag(mr, l_iter, data->cd_ofs, eldata);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edituv_data_iter_poly_mesh(const MeshRenderData *mr,
                                               const MPoly *mp,
                                               const int mp_index,
                                               void *_data)
{
  MeshExtract_EditUVData_Data *data = _data;
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    EditLoopData *eldata = &data->vbo_data[ml_index];
    memset(eldata, 0x0, sizeof(*eldata));
    BMFace *efa = bm_original_face_get(mr, mp_index);
    if (efa) {
      BMEdge *eed = bm_original_edge_get(mr, ml->e);
      BMVert *eve = bm_original_vert_get(mr, ml->v);
      if (eed && eve) {
        /* Loop on an edge endpoint. */
        BMLoop *l = BM_face_edge_share_loop(efa, eed);
        mesh_render_data_loop_flag(mr, l, data->cd_ofs, eldata);
        mesh_render_data_loop_edge_flag(mr, l, data->cd_ofs, eldata);
      }
      else {
        if (eed == NULL) {
          /* Find if the loop's vert is not part of an edit edge.
           * For this, we check if the previous loop was on an edge. */
          const int ml_index_last = mp->loopstart + mp->totloop - 1;
          const int l_prev = (ml_index == mp->loopstart) ? ml_index_last : (ml_index - 1);
          const MLoop *ml_prev = &mr->mloop[l_prev];
          eed = bm_original_edge_get(mr, ml_prev->e);
        }
        if (eed) {
          /* Mapped points on an edge between two edit verts. */
          BMLoop *l = BM_face_edge_share_loop(efa, eed);
          mesh_render_data_loop_edge_flag(mr, l, data->cd_ofs, eldata);
        }
      }
    }
  }
}

static void extract_edituv_data_finish(const MeshRenderData *UNUSED(mr),
                                       struct MeshBatchCache *UNUSED(cache),
                                       void *UNUSED(buf),
                                       void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_edituv_data = {
    .init = extract_edituv_data_init,
    .iter_poly_bm = extract_edituv_data_iter_poly_bm,
    .iter_poly_mesh = extract_edituv_data_iter_poly_mesh,
    .finish = extract_edituv_data_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.edituv_data)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV area stretch
 * \{ */

static void *extract_edituv_stretch_area_init(const MeshRenderData *mr,
                                              struct MeshBatchCache *UNUSED(cache),
                                              void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "ratio", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return NULL;
}

BLI_INLINE float area_ratio_get(float area, float uvarea)
{
  if (area >= FLT_EPSILON && uvarea >= FLT_EPSILON) {
    /* Tag inversion by using the sign. */
    return (area > uvarea) ? (uvarea / area) : -(area / uvarea);
  }
  return 0.0f;
}

BLI_INLINE float area_ratio_to_stretch(float ratio, float tot_ratio, float inv_tot_ratio)
{
  ratio *= (ratio > 0.0f) ? tot_ratio : -inv_tot_ratio;
  return (ratio > 1.0f) ? (1.0f / ratio) : ratio;
}

static void extract_edituv_stretch_area_finish(const MeshRenderData *mr,
                                               struct MeshBatchCache *cache,
                                               void *buf,
                                               void *UNUSED(data))
{
  GPUVertBuf *vbo = buf;
  float tot_area = 0.0f, tot_uv_area = 0.0f;
  float *area_ratio = MEM_mallocN(sizeof(float) * mr->poly_len, __func__);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    CustomData *cd_ldata = &mr->bm->ldata;
    int uv_ofs = CustomData_get_offset(cd_ldata, CD_MLOOPUV);

    BMFace *efa;
    BMIter f_iter;
    int f;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      float area = BM_face_calc_area(efa);
      float uvarea = BM_face_calc_area_uv(efa, uv_ofs);
      tot_area += area;
      tot_uv_area += uvarea;
      area_ratio[f] = area_ratio_get(area, uvarea);
    }
  }
  else {
    BLI_assert(ELEM(mr->extract_type, MR_EXTRACT_MAPPED, MR_EXTRACT_MESH));
    const MLoopUV *uv_data = CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      float area = BKE_mesh_calc_poly_area(mp, &mr->mloop[mp->loopstart], mr->mvert);
      float uvarea = BKE_mesh_calc_poly_uv_area(mp, uv_data);
      tot_area += area;
      tot_uv_area += uvarea;
      area_ratio[mp_index] = area_ratio_get(area, uvarea);
    }
  }

  cache->tot_area = tot_area;
  cache->tot_uv_area = tot_uv_area;

  /* Convert in place to avoid an extra allocation */
  uint16_t *poly_stretch = (uint16_t *)area_ratio;
  for (int mp_index = 0; mp_index < mr->poly_len; mp_index++) {
    poly_stretch[mp_index] = area_ratio[mp_index] * SHRT_MAX;
  }

  /* Copy face data for each loop. */
  uint16_t *loop_stretch = (uint16_t *)GPU_vertbuf_get_data(vbo);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMFace *efa;
    BMIter f_iter;
    int f, l_index = 0;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      for (int i = 0; i < efa->len; i++, l_index++) {
        loop_stretch[l_index] = poly_stretch[f];
      }
    }
  }
  else {
    BLI_assert(ELEM(mr->extract_type, MR_EXTRACT_MAPPED, MR_EXTRACT_MESH));
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0, l_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      for (int i = 0; i < mp->totloop; i++, l_index++) {
        loop_stretch[l_index] = poly_stretch[mp_index];
      }
    }
  }

  MEM_freeN(area_ratio);
}

const MeshExtract extract_edituv_stretch_area = {
    .init = extract_edituv_stretch_area_init,
    .finish = extract_edituv_stretch_area_finish,
    .data_type = 0,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.edituv_stretch_area)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV angle stretch
 * \{ */

typedef struct UVStretchAngle {
  int16_t angle;
  int16_t uv_angles[2];
} UVStretchAngle;

typedef struct MeshExtract_StretchAngle_Data {
  UVStretchAngle *vbo_data;
  MLoopUV *luv;
  float auv[2][2], last_auv[2];
  float av[2][3], last_av[3];
  int cd_ofs;
} MeshExtract_StretchAngle_Data;

static void compute_normalize_edge_vectors(float auv[2][2],
                                           float av[2][3],
                                           const float uv[2],
                                           const float uv_prev[2],
                                           const float co[3],
                                           const float co_prev[3])
{
  /* Move previous edge. */
  copy_v2_v2(auv[0], auv[1]);
  copy_v3_v3(av[0], av[1]);
  /* 2d edge */
  sub_v2_v2v2(auv[1], uv_prev, uv);
  normalize_v2(auv[1]);
  /* 3d edge */
  sub_v3_v3v3(av[1], co_prev, co);
  normalize_v3(av[1]);
}

static short v2_to_short_angle(const float v[2])
{
  return atan2f(v[1], v[0]) * (float)M_1_PI * SHRT_MAX;
}

static void edituv_get_edituv_stretch_angle(float auv[2][2],
                                            const float av[2][3],
                                            UVStretchAngle *r_stretch)
{
  /* Send UV's to the shader and let it compute the aspect corrected angle. */
  r_stretch->uv_angles[0] = v2_to_short_angle(auv[0]);
  r_stretch->uv_angles[1] = v2_to_short_angle(auv[1]);
  /* Compute 3D angle here. */
  r_stretch->angle = angle_normalized_v3v3(av[0], av[1]) * (float)M_1_PI * SHRT_MAX;

#if 0 /* here for reference, this is done in shader now. */
  float uvang = angle_normalized_v2v2(auv0, auv1);
  float ang = angle_normalized_v3v3(av0, av1);
  float stretch = fabsf(uvang - ang) / (float)M_PI;
  return 1.0f - pow2f(1.0f - stretch);
#endif
}

static void *extract_edituv_stretch_angle_init(const MeshRenderData *mr,
                                               struct MeshBatchCache *UNUSED(cache),
                                               void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* Waning: adjust #UVStretchAngle struct accordingly. */
    GPU_vertformat_attr_add(&format, "angle", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_attr_add(&format, "uv_angles", GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  MeshExtract_StretchAngle_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (UVStretchAngle *)GPU_vertbuf_get_data(vbo);

  /* Special iterator needed to save about half of the computing cost. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  }
  else {
    BLI_assert(ELEM(mr->extract_type, MR_EXTRACT_MAPPED, MR_EXTRACT_MESH));
    data->luv = CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
  }
  return data;
}

static void extract_edituv_stretch_angle_iter_poly_bm(const MeshRenderData *mr,
                                                      BMFace *f,
                                                      const int UNUSED(f_index),
                                                      void *_data)
{
  MeshExtract_StretchAngle_Data *data = _data;
  float(*auv)[2] = data->auv, *last_auv = data->last_auv;
  float(*av)[3] = data->av, *last_av = data->last_av;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    const MLoopUV *luv, *luv_next;
    BMLoop *l_next = l_iter->next;
    if (l_iter == BM_FACE_FIRST_LOOP(f)) {
      /* First loop in face. */
      BMLoop *l_tmp = l_iter->prev;
      BMLoop *l_next_tmp = l_iter;
      luv = BM_ELEM_CD_GET_VOID_P(l_tmp, data->cd_ofs);
      luv_next = BM_ELEM_CD_GET_VOID_P(l_next_tmp, data->cd_ofs);
      compute_normalize_edge_vectors(auv,
                                     av,
                                     luv->uv,
                                     luv_next->uv,
                                     bm_vert_co_get(mr, l_tmp->v),
                                     bm_vert_co_get(mr, l_next_tmp->v));
      /* Save last edge. */
      copy_v2_v2(last_auv, auv[1]);
      copy_v3_v3(last_av, av[1]);
    }
    if (l_next == BM_FACE_FIRST_LOOP(f)) {
      /* Move previous edge. */
      copy_v2_v2(auv[0], auv[1]);
      copy_v3_v3(av[0], av[1]);
      /* Copy already calculated last edge. */
      copy_v2_v2(auv[1], last_auv);
      copy_v3_v3(av[1], last_av);
    }
    else {
      luv = BM_ELEM_CD_GET_VOID_P(l_iter, data->cd_ofs);
      luv_next = BM_ELEM_CD_GET_VOID_P(l_next, data->cd_ofs);
      compute_normalize_edge_vectors(auv,
                                     av,
                                     luv->uv,
                                     luv_next->uv,
                                     bm_vert_co_get(mr, l_iter->v),
                                     bm_vert_co_get(mr, l_next->v));
    }
    edituv_get_edituv_stretch_angle(auv, av, &data->vbo_data[l_index]);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edituv_stretch_angle_iter_poly_mesh(const MeshRenderData *mr,
                                                        const MPoly *mp,
                                                        const int UNUSED(mp_index),
                                                        void *_data)
{
  MeshExtract_StretchAngle_Data *data = _data;

  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    float(*auv)[2] = data->auv, *last_auv = data->last_auv;
    float(*av)[3] = data->av, *last_av = data->last_av;
    int l_next = ml_index + 1;
    const MVert *v, *v_next;
    if (ml_index == mp->loopstart) {
      /* First loop in face. */
      const int ml_index_last = ml_index_end - 1;
      const int l_next_tmp = mp->loopstart;
      v = &mr->mvert[mr->mloop[ml_index_last].v];
      v_next = &mr->mvert[mr->mloop[l_next_tmp].v];
      compute_normalize_edge_vectors(
          auv, av, data->luv[ml_index_last].uv, data->luv[l_next_tmp].uv, v->co, v_next->co);
      /* Save last edge. */
      copy_v2_v2(last_auv, auv[1]);
      copy_v3_v3(last_av, av[1]);
    }
    if (l_next == ml_index_end) {
      l_next = mp->loopstart;
      /* Move previous edge. */
      copy_v2_v2(auv[0], auv[1]);
      copy_v3_v3(av[0], av[1]);
      /* Copy already calculated last edge. */
      copy_v2_v2(auv[1], last_auv);
      copy_v3_v3(av[1], last_av);
    }
    else {
      v = &mr->mvert[mr->mloop[ml_index].v];
      v_next = &mr->mvert[mr->mloop[l_next].v];
      compute_normalize_edge_vectors(
          auv, av, data->luv[ml_index].uv, data->luv[l_next].uv, v->co, v_next->co);
    }
    edituv_get_edituv_stretch_angle(auv, av, &data->vbo_data[ml_index]);
  }
}

static void extract_edituv_stretch_angle_finish(const MeshRenderData *UNUSED(mr),
                                                struct MeshBatchCache *UNUSED(cache),
                                                void *UNUSED(buf),
                                                void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_edituv_stretch_angle = {
    .init = extract_edituv_stretch_angle_init,
    .iter_poly_bm = extract_edituv_stretch_angle_iter_poly_bm,
    .iter_poly_mesh = extract_edituv_stretch_angle_iter_poly_mesh,
    .finish = extract_edituv_stretch_angle_finish,
    .data_type = 0,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.edituv_stretch_angle)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit Mesh Analysis Colors
 * \{ */

static void *extract_mesh_analysis_init(const MeshRenderData *mr,
                                        struct MeshBatchCache *UNUSED(cache),
                                        void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return NULL;
}

static void axis_from_enum_v3(float v[3], const char axis)
{
  zero_v3(v);
  if (axis < 3) {
    v[axis] = 1.0f;
  }
  else {
    v[axis - 3] = -1.0f;
  }
}

BLI_INLINE float overhang_remap(float fac, float min, float max, float minmax_irange)
{
  if (fac < min) {
    fac = 1.0f;
  }
  else if (fac > max) {
    fac = -1.0f;
  }
  else {
    fac = (fac - min) * minmax_irange;
    fac = 1.0f - fac;
    CLAMP(fac, 0.0f, 1.0f);
  }
  return fac;
}

static void statvis_calc_overhang(const MeshRenderData *mr, float *r_overhang)
{
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->overhang_min / (float)M_PI;
  const float max = statvis->overhang_max / (float)M_PI;
  const char axis = statvis->overhang_axis;
  BMEditMesh *em = mr->edit_bmesh;
  BMIter iter;
  BMesh *bm = em->bm;
  BMFace *f;
  float dir[3];
  const float minmax_irange = 1.0f / (max - min);

  BLI_assert(min <= max);

  axis_from_enum_v3(dir, axis);

  /* now convert into global space */
  mul_transposed_mat3_m4_v3(mr->obmat, dir);
  normalize_v3(dir);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    int l_index = 0;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      float fac = angle_normalized_v3v3(bm_face_no_get(mr, f), dir) / (float)M_PI;
      fac = overhang_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l_index++) {
        r_overhang[l_index] = fac;
      }
    }
  }
  else {
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0, l_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      float fac = angle_normalized_v3v3(mr->poly_normals[mp_index], dir) / (float)M_PI;
      fac = overhang_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < mp->totloop; i++, l_index++) {
        r_overhang[l_index] = fac;
      }
    }
  }
}

/**
 * Needed so we can use jitter values for face interpolation.
 */
static void uv_from_jitter_v2(float uv[2])
{
  uv[0] += 0.5f;
  uv[1] += 0.5f;
  if (uv[0] + uv[1] > 1.0f) {
    uv[0] = 1.0f - uv[0];
    uv[1] = 1.0f - uv[1];
  }

  clamp_v2(uv, 0.0f, 1.0f);
}

BLI_INLINE float thickness_remap(float fac, float min, float max, float minmax_irange)
{
  /* important not '<=' */
  if (fac < max) {
    fac = (fac - min) * minmax_irange;
    fac = 1.0f - fac;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_thickness(const MeshRenderData *mr, float *r_thickness)
{
  const float eps_offset = 0.00002f; /* values <= 0.00001 give errors */
  /* cheating to avoid another allocation */
  float *face_dists = r_thickness + (mr->loop_len - mr->poly_len);
  BMEditMesh *em = mr->edit_bmesh;
  const float scale = 1.0f / mat4_to_scale(mr->obmat);
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->thickness_min * scale;
  const float max = statvis->thickness_max * scale;
  const float minmax_irange = 1.0f / (max - min);
  const int samples = statvis->thickness_samples;
  float jit_ofs[32][2];
  BLI_assert(samples <= 32);
  BLI_assert(min <= max);

  copy_vn_fl(face_dists, mr->poly_len, max);

  BLI_jitter_init(jit_ofs, samples);
  for (int j = 0; j < samples; j++) {
    uv_from_jitter_v2(jit_ofs[j]);
  }

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMesh *bm = em->bm;
    BM_mesh_elem_index_ensure(bm, BM_FACE);

    struct BMBVHTree *bmtree = BKE_bmbvh_new_from_editmesh(em, 0, NULL, false);
    struct BMLoop *(*looptris)[3] = em->looptris;
    for (int i = 0; i < mr->tri_len; i++) {
      BMLoop **ltri = looptris[i];
      const int index = BM_elem_index_get(ltri[0]->f);
      const float *cos[3] = {
          bm_vert_co_get(mr, ltri[0]->v),
          bm_vert_co_get(mr, ltri[1]->v),
          bm_vert_co_get(mr, ltri[2]->v),
      };
      float ray_co[3];
      float ray_no[3];

      normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

      for (int j = 0; j < samples; j++) {
        float dist = face_dists[index];
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        BMFace *f_hit = BKE_bmbvh_ray_cast(bmtree, ray_co, ray_no, 0.0f, &dist, NULL, NULL);
        if (f_hit && dist < face_dists[index]) {
          float angle_fac = fabsf(
              dot_v3v3(bm_face_no_get(mr, ltri[0]->f), bm_face_no_get(mr, f_hit)));
          angle_fac = 1.0f - angle_fac;
          angle_fac = angle_fac * angle_fac * angle_fac;
          angle_fac = 1.0f - angle_fac;
          dist /= angle_fac;
          if (dist < face_dists[index]) {
            face_dists[index] = dist;
          }
        }
      }
    }
    BKE_bmbvh_free(bmtree);

    BMIter iter;
    BMFace *f;
    int l_index = 0;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      float fac = face_dists[BM_elem_index_get(f)];
      fac = thickness_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l_index++) {
        r_thickness[l_index] = fac;
      }
    }
  }
  else {
    BVHTreeFromMesh treeData = {NULL};

    BVHTree *tree = BKE_bvhtree_from_mesh_get(&treeData, mr->me, BVHTREE_FROM_LOOPTRI, 4);
    const MLoopTri *mlooptri = mr->mlooptri;
    for (int i = 0; i < mr->tri_len; i++, mlooptri++) {
      const int index = mlooptri->poly;
      const float *cos[3] = {mr->mvert[mr->mloop[mlooptri->tri[0]].v].co,
                             mr->mvert[mr->mloop[mlooptri->tri[1]].v].co,
                             mr->mvert[mr->mloop[mlooptri->tri[2]].v].co};
      float ray_co[3];
      float ray_no[3];

      normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

      for (int j = 0; j < samples; j++) {
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        BVHTreeRayHit hit;
        hit.index = -1;
        hit.dist = face_dists[index];
        if ((BLI_bvhtree_ray_cast(
                 tree, ray_co, ray_no, 0.0f, &hit, treeData.raycast_callback, &treeData) != -1) &&
            hit.dist < face_dists[index]) {
          float angle_fac = fabsf(dot_v3v3(mr->poly_normals[index], hit.no));
          angle_fac = 1.0f - angle_fac;
          angle_fac = angle_fac * angle_fac * angle_fac;
          angle_fac = 1.0f - angle_fac;
          hit.dist /= angle_fac;
          if (hit.dist < face_dists[index]) {
            face_dists[index] = hit.dist;
          }
        }
      }
    }

    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0, l_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      float fac = face_dists[mp_index];
      fac = thickness_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < mp->totloop; i++, l_index++) {
        r_thickness[l_index] = fac;
      }
    }
  }
}

struct BVHTree_OverlapData {
  const Mesh *me;
  const MLoopTri *mlooptri;
  float epsilon;
};

static bool bvh_overlap_cb(void *userdata, int index_a, int index_b, int UNUSED(thread))
{
  struct BVHTree_OverlapData *data = userdata;
  const Mesh *me = data->me;

  const MLoopTri *tri_a = &data->mlooptri[index_a];
  const MLoopTri *tri_b = &data->mlooptri[index_b];

  if (UNLIKELY(tri_a->poly == tri_b->poly)) {
    return false;
  }

  const float *tri_a_co[3] = {me->mvert[me->mloop[tri_a->tri[0]].v].co,
                              me->mvert[me->mloop[tri_a->tri[1]].v].co,
                              me->mvert[me->mloop[tri_a->tri[2]].v].co};
  const float *tri_b_co[3] = {me->mvert[me->mloop[tri_b->tri[0]].v].co,
                              me->mvert[me->mloop[tri_b->tri[1]].v].co,
                              me->mvert[me->mloop[tri_b->tri[2]].v].co};
  float ix_pair[2][3];
  int verts_shared = 0;

  verts_shared = (ELEM(tri_a_co[0], UNPACK3(tri_b_co)) + ELEM(tri_a_co[1], UNPACK3(tri_b_co)) +
                  ELEM(tri_a_co[2], UNPACK3(tri_b_co)));

  /* if 2 points are shared, bail out */
  if (verts_shared >= 2) {
    return false;
  }

  return (isect_tri_tri_v3(UNPACK3(tri_a_co), UNPACK3(tri_b_co), ix_pair[0], ix_pair[1]) &&
          /* if we share a vertex, check the intersection isn't a 'point' */
          ((verts_shared == 0) || (len_squared_v3v3(ix_pair[0], ix_pair[1]) > data->epsilon)));
}

static void statvis_calc_intersect(const MeshRenderData *mr, float *r_intersect)
{
  BMEditMesh *em = mr->edit_bmesh;

  for (int l_index = 0; l_index < mr->loop_len; l_index++) {
    r_intersect[l_index] = -1.0f;
  }

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    uint overlap_len;
    BMesh *bm = em->bm;

    BM_mesh_elem_index_ensure(bm, BM_FACE);

    struct BMBVHTree *bmtree = BKE_bmbvh_new_from_editmesh(em, 0, NULL, false);
    BVHTreeOverlap *overlap = BKE_bmbvh_overlap_self(bmtree, &overlap_len);

    if (overlap) {
      for (int i = 0; i < overlap_len; i++) {
        BMFace *f_hit_pair[2] = {
            em->looptris[overlap[i].indexA][0]->f,
            em->looptris[overlap[i].indexB][0]->f,
        };
        for (int j = 0; j < 2; j++) {
          BMFace *f_hit = f_hit_pair[j];
          BMLoop *l_first = BM_FACE_FIRST_LOOP(f_hit);
          int l_index = BM_elem_index_get(l_first);
          for (int k = 0; k < f_hit->len; k++, l_index++) {
            r_intersect[l_index] = 1.0f;
          }
        }
      }
      MEM_freeN(overlap);
    }

    BKE_bmbvh_free(bmtree);
  }
  else {
    uint overlap_len;
    BVHTreeFromMesh treeData = {NULL};

    BVHTree *tree = BKE_bvhtree_from_mesh_get(&treeData, mr->me, BVHTREE_FROM_LOOPTRI, 4);

    struct BVHTree_OverlapData data = {
        .me = mr->me, .mlooptri = mr->mlooptri, .epsilon = BLI_bvhtree_get_epsilon(tree)};

    BVHTreeOverlap *overlap = BLI_bvhtree_overlap(tree, tree, &overlap_len, bvh_overlap_cb, &data);
    if (overlap) {
      for (int i = 0; i < overlap_len; i++) {
        const MPoly *f_hit_pair[2] = {
            &mr->mpoly[mr->mlooptri[overlap[i].indexA].poly],
            &mr->mpoly[mr->mlooptri[overlap[i].indexB].poly],
        };
        for (int j = 0; j < 2; j++) {
          const MPoly *f_hit = f_hit_pair[j];
          int l_index = f_hit->loopstart;
          for (int k = 0; k < f_hit->totloop; k++, l_index++) {
            r_intersect[l_index] = 1.0f;
          }
        }
      }
      MEM_freeN(overlap);
    }
  }
}

BLI_INLINE float distort_remap(float fac, float min, float UNUSED(max), float minmax_irange)
{
  if (fac >= min) {
    fac = (fac - min) * minmax_irange;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    /* fallback */
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_distort(const MeshRenderData *mr, float *r_distort)
{
  BMEditMesh *em = mr->edit_bmesh;
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->distort_min;
  const float max = statvis->distort_max;
  const float minmax_irange = 1.0f / (max - min);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMesh *bm = em->bm;
    BMFace *f;

    if (mr->bm_vert_coords != NULL) {
      BKE_editmesh_cache_ensure_poly_normals(em, mr->edit_data);

      /* Most likely this is already valid, ensure just in case.
       * Needed for #BM_loop_calc_face_normal_safe_vcos. */
      BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    }

    int l_index = 0;
    int f_index = 0;
    BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, f_index) {
      float fac = -1.0f;

      if (f->len > 3) {
        BMLoop *l_iter, *l_first;

        fac = 0.0f;
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          const float *no_face;
          float no_corner[3];
          if (mr->bm_vert_coords != NULL) {
            no_face = mr->bm_poly_normals[f_index];
            BM_loop_calc_face_normal_safe_vcos(l_iter, no_face, mr->bm_vert_coords, no_corner);
          }
          else {
            no_face = f->no;
            BM_loop_calc_face_normal_safe(l_iter, no_corner);
          }

          /* simple way to detect (what is most likely) concave */
          if (dot_v3v3(no_face, no_corner) < 0.0f) {
            negate_v3(no_corner);
          }
          fac = max_ff(fac, angle_normalized_v3v3(no_face, no_corner));

        } while ((l_iter = l_iter->next) != l_first);
        fac *= 2.0f;
      }

      fac = distort_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l_index++) {
        r_distort[l_index] = fac;
      }
    }
  }
  else {
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0, l_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      float fac = -1.0f;

      if (mp->totloop > 3) {
        float *f_no = mr->poly_normals[mp_index];
        fac = 0.0f;

        for (int i = 1; i <= mp->totloop; i++) {
          const MLoop *l_prev = &mr->mloop[mp->loopstart + (i - 1) % mp->totloop];
          const MLoop *l_curr = &mr->mloop[mp->loopstart + (i + 0) % mp->totloop];
          const MLoop *l_next = &mr->mloop[mp->loopstart + (i + 1) % mp->totloop];
          float no_corner[3];
          normal_tri_v3(no_corner,
                        mr->mvert[l_prev->v].co,
                        mr->mvert[l_curr->v].co,
                        mr->mvert[l_next->v].co);
          /* simple way to detect (what is most likely) concave */
          if (dot_v3v3(f_no, no_corner) < 0.0f) {
            negate_v3(no_corner);
          }
          fac = max_ff(fac, angle_normalized_v3v3(f_no, no_corner));
        }
        fac *= 2.0f;
      }

      fac = distort_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < mp->totloop; i++, l_index++) {
        r_distort[l_index] = fac;
      }
    }
  }
}

BLI_INLINE float sharp_remap(float fac, float min, float UNUSED(max), float minmax_irange)
{
  /* important not '>=' */
  if (fac > min) {
    fac = (fac - min) * minmax_irange;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    /* fallback */
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_sharp(const MeshRenderData *mr, float *r_sharp)
{
  BMEditMesh *em = mr->edit_bmesh;
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->sharp_min;
  const float max = statvis->sharp_max;
  const float minmax_irange = 1.0f / (max - min);

  /* Can we avoid this extra allocation? */
  float *vert_angles = MEM_mallocN(sizeof(float) * mr->vert_len, __func__);
  copy_vn_fl(vert_angles, mr->vert_len, -M_PI);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMesh *bm = em->bm;
    BMFace *efa;
    BMEdge *e;
    /* first assign float values to verts */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      float angle = BM_edge_calc_face_angle_signed(e);
      float *col1 = &vert_angles[BM_elem_index_get(e->v1)];
      float *col2 = &vert_angles[BM_elem_index_get(e->v2)];
      *col1 = max_ff(*col1, angle);
      *col2 = max_ff(*col2, angle);
    }
    /* Copy vert value to loops. */
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        int l_index = BM_elem_index_get(l_iter);
        int v_index = BM_elem_index_get(l_iter->v);
        r_sharp[l_index] = sharp_remap(vert_angles[v_index], min, max, minmax_irange);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    /* first assign float values to verts */
    const MPoly *mp = mr->mpoly;

    EdgeHash *eh = BLI_edgehash_new_ex(__func__, mr->edge_len);

    for (int mp_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      for (int i = 0; i < mp->totloop; i++) {
        const MLoop *l_curr = &mr->mloop[mp->loopstart + (i + 0) % mp->totloop];
        const MLoop *l_next = &mr->mloop[mp->loopstart + (i + 1) % mp->totloop];
        const MVert *v_curr = &mr->mvert[l_curr->v];
        const MVert *v_next = &mr->mvert[l_next->v];
        float angle;
        void **pval;
        bool value_is_init = BLI_edgehash_ensure_p(eh, l_curr->v, l_next->v, &pval);
        if (!value_is_init) {
          *pval = mr->poly_normals[mp_index];
          /* non-manifold edge, yet... */
          continue;
        }
        if (*pval != NULL) {
          const float *f1_no = mr->poly_normals[mp_index];
          const float *f2_no = *pval;
          angle = angle_normalized_v3v3(f1_no, f2_no);
          angle = is_edge_convex_v3(v_curr->co, v_next->co, f1_no, f2_no) ? angle : -angle;
          /* Tag as manifold. */
          *pval = NULL;
        }
        else {
          /* non-manifold edge */
          angle = DEG2RADF(90.0f);
        }
        float *col1 = &vert_angles[l_curr->v];
        float *col2 = &vert_angles[l_next->v];
        *col1 = max_ff(*col1, angle);
        *col2 = max_ff(*col2, angle);
      }
    }
    /* Remaining non manifold edges. */
    EdgeHashIterator *ehi = BLI_edgehashIterator_new(eh);
    for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
      if (BLI_edgehashIterator_getValue(ehi) != NULL) {
        uint v1, v2;
        const float angle = DEG2RADF(90.0f);
        BLI_edgehashIterator_getKey(ehi, &v1, &v2);
        float *col1 = &vert_angles[v1];
        float *col2 = &vert_angles[v2];
        *col1 = max_ff(*col1, angle);
        *col2 = max_ff(*col2, angle);
      }
    }
    BLI_edgehashIterator_free(ehi);
    BLI_edgehash_free(eh, NULL);

    const MLoop *ml = mr->mloop;
    for (int l_index = 0; l_index < mr->loop_len; l_index++, ml++) {
      r_sharp[l_index] = sharp_remap(vert_angles[ml->v], min, max, minmax_irange);
    }
  }

  MEM_freeN(vert_angles);
}

static void extract_analysis_iter_finish_mesh(const MeshRenderData *mr,
                                              struct MeshBatchCache *UNUSED(cache),
                                              void *buf,
                                              void *UNUSED(data))
{
  GPUVertBuf *vbo = buf;
  BLI_assert(mr->edit_bmesh);

  float *l_weight = (float *)GPU_vertbuf_get_data(vbo);

  switch (mr->toolsettings->statvis.type) {
    case SCE_STATVIS_OVERHANG:
      statvis_calc_overhang(mr, l_weight);
      break;
    case SCE_STATVIS_THICKNESS:
      statvis_calc_thickness(mr, l_weight);
      break;
    case SCE_STATVIS_INTERSECT:
      statvis_calc_intersect(mr, l_weight);
      break;
    case SCE_STATVIS_DISTORT:
      statvis_calc_distort(mr, l_weight);
      break;
    case SCE_STATVIS_SHARP:
      statvis_calc_sharp(mr, l_weight);
      break;
  }
}

const MeshExtract extract_mesh_analysis = {
    .init = extract_mesh_analysis_init,
    .finish = extract_analysis_iter_finish_mesh,
    /* This is not needed for all visualization types.
     * * Maybe split into different extract. */
    .data_type = MR_DATA_POLY_NOR | MR_DATA_LOOPTRI,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.mesh_analysis)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots positions
 * \{ */

static void *extract_fdots_pos_init(const MeshRenderData *mr,
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);
  return GPU_vertbuf_get_data(vbo);
}

static void extract_fdots_pos_iter_poly_bm(const MeshRenderData *mr,
                                           BMFace *f,
                                           const int f_index,
                                           void *data)
{
  float(*center)[3] = data;

  float *co = center[f_index];
  zero_v3(co);

  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    add_v3_v3(co, bm_vert_co_get(mr, l_iter->v));
  } while ((l_iter = l_iter->next) != l_first);
  mul_v3_fl(co, 1.0f / (float)f->len);
}

static void extract_fdots_pos_iter_poly_mesh(const MeshRenderData *mr,
                                             const MPoly *mp,
                                             const int mp_index,
                                             void *data)
{
  float(*center)[3] = (float(*)[3])data;
  float *co = center[mp_index];
  zero_v3(co);

  const MVert *mvert = mr->mvert;
  const MLoop *mloop = mr->mloop;

  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    if (mr->use_subsurf_fdots) {
      const MVert *mv = &mr->mvert[ml->v];
      if (mv->flag & ME_VERT_FACEDOT) {
        copy_v3_v3(center[mp_index], mv->co);
        break;
      }
    }
    else {
      const MVert *mv = &mvert[ml->v];
      add_v3_v3(center[mp_index], mv->co);
    }
  }

  if (!mr->use_subsurf_fdots) {
    mul_v3_fl(co, 1.0f / (float)mp->totloop);
  }
}

const MeshExtract extract_fdots_pos = {
    .init = extract_fdots_pos_init,
    .iter_poly_bm = extract_fdots_pos_iter_poly_bm,
    .iter_poly_mesh = extract_fdots_pos_iter_poly_mesh,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdots_pos)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots Normal and edit flag
 * \{ */
#define NOR_AND_FLAG_DEFAULT 0
#define NOR_AND_FLAG_SELECT 1
#define NOR_AND_FLAG_ACTIVE -1
#define NOR_AND_FLAG_HIDDEN -2

static void *extract_fdots_nor_init(const MeshRenderData *mr,
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  return NULL;
}

static void extract_fdots_nor_finish(const MeshRenderData *mr,
                                     struct MeshBatchCache *UNUSED(cache),
                                     void *buf,
                                     void *UNUSED(data))
{
  GPUVertBuf *vbo = buf;
  static float invalid_normal[3] = {0.0f, 0.0f, 0.0f};
  GPUPackedNormal *nor = (GPUPackedNormal *)GPU_vertbuf_get_data(vbo);
  BMFace *efa;

  /* Quicker than doing it for each loop. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    for (int f = 0; f < mr->poly_len; f++) {
      efa = BM_face_at_index(mr->bm, f);
      const bool is_face_hidden = BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                             mr->p_origindex[f] == ORIGINDEX_NONE)) {
        nor[f] = GPU_normal_convert_i10_v3(invalid_normal);
        nor[f].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        nor[f] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f].w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                        ((efa == mr->efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                        NOR_AND_FLAG_DEFAULT);
      }
    }
  }
  else {
    for (int f = 0; f < mr->poly_len; f++) {
      efa = bm_original_face_get(mr, f);
      const bool is_face_hidden = efa && BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                             mr->p_origindex[f] == ORIGINDEX_NONE)) {
        nor[f] = GPU_normal_convert_i10_v3(invalid_normal);
        nor[f].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        nor[f] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f].w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                        ((efa == mr->efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                        NOR_AND_FLAG_DEFAULT);
      }
    }
  }
}

const MeshExtract extract_fdots_nor = {
    .init = extract_fdots_nor_init,
    .finish = extract_fdots_nor_finish,
    .data_type = MR_DATA_POLY_NOR,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdots_nor)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots High Quality Normal and edit flag
 * \{ */
static void *extract_fdots_nor_hq_init(const MeshRenderData *mr,
                                       struct MeshBatchCache *UNUSED(cache),
                                       void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  return NULL;
}

static void extract_fdots_nor_hq_finish(const MeshRenderData *mr,
                                        struct MeshBatchCache *UNUSED(cache),
                                        void *buf,
                                        void *UNUSED(data))
{
  GPUVertBuf *vbo = buf;
  static float invalid_normal[3] = {0.0f, 0.0f, 0.0f};
  short *nor = (short *)GPU_vertbuf_get_data(vbo);
  BMFace *efa;

  /* Quicker than doing it for each loop. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    for (int f = 0; f < mr->poly_len; f++) {
      efa = BM_face_at_index(mr->bm, f);
      const bool is_face_hidden = BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                             mr->p_origindex[f] == ORIGINDEX_NONE)) {
        normal_float_to_short_v3(&nor[f * 4], invalid_normal);
        nor[f * 4 + 3] = NOR_AND_FLAG_HIDDEN;
      }
      else {
        normal_float_to_short_v3(&nor[f * 4], bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f * 4 + 3] = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                              ((efa == mr->efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                              NOR_AND_FLAG_DEFAULT);
      }
    }
  }
  else {
    for (int f = 0; f < mr->poly_len; f++) {
      efa = bm_original_face_get(mr, f);
      const bool is_face_hidden = efa && BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                             mr->p_origindex[f] == ORIGINDEX_NONE)) {
        normal_float_to_short_v3(&nor[f * 4], invalid_normal);
        nor[f * 4 + 3] = NOR_AND_FLAG_HIDDEN;
      }
      else {
        normal_float_to_short_v3(&nor[f * 4], bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f * 4 + 3] = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                              ((efa == mr->efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                              NOR_AND_FLAG_DEFAULT);
      }
    }
  }
}

const MeshExtract extract_fdots_nor_hq = {
    .init = extract_fdots_nor_hq_init,
    .finish = extract_fdots_nor_hq_finish,
    .data_type = MR_DATA_POLY_NOR,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdots_nor)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots UV
 * \{ */

typedef struct MeshExtract_FdotUV_Data {
  float (*vbo_data)[2];
  MLoopUV *uv_data;
  int cd_ofs;
} MeshExtract_FdotUV_Data;

static void *extract_fdots_uv_init(const MeshRenderData *mr,
                                   struct MeshBatchCache *UNUSED(cache),
                                   void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "au");
    GPU_vertformat_alias_add(&format, "pos");
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  if (!mr->use_subsurf_fdots) {
    /* Clear so we can accumulate on it. */
    memset(GPU_vertbuf_get_data(vbo), 0x0, mr->poly_len * GPU_vertbuf_get_format(vbo)->stride);
  }

  MeshExtract_FdotUV_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (float(*)[2])GPU_vertbuf_get_data(vbo);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  }
  else {
    data->uv_data = CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
  }
  return data;
}

static void extract_fdots_uv_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                          BMFace *f,
                                          const int UNUSED(f_index),
                                          void *_data)
{
  MeshExtract_FdotUV_Data *data = _data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    float w = 1.0f / (float)f->len;
    const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, data->cd_ofs);
    madd_v2_v2fl(data->vbo_data[BM_elem_index_get(f)], luv->uv, w);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_fdots_uv_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int mp_index,
                                            void *_data)
{
  MeshExtract_FdotUV_Data *data = _data;
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    if (mr->use_subsurf_fdots) {
      const MVert *mv = &mr->mvert[ml->v];
      if (mv->flag & ME_VERT_FACEDOT) {
        copy_v2_v2(data->vbo_data[mp_index], data->uv_data[ml_index].uv);
      }
    }
    else {
      float w = 1.0f / (float)mp->totloop;
      madd_v2_v2fl(data->vbo_data[mp_index], data->uv_data[ml_index].uv, w);
    }
  }
}

static void extract_fdots_uv_finish(const MeshRenderData *UNUSED(mr),
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *UNUSED(buf),
                                    void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_fdots_uv = {
    .init = extract_fdots_uv_init,
    .iter_poly_bm = extract_fdots_uv_iter_poly_bm,
    .iter_poly_mesh = extract_fdots_uv_iter_poly_mesh,
    .finish = extract_fdots_uv_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdots_uv)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots  Edit UV flag
 * \{ */

typedef struct MeshExtract_EditUVFdotData_Data {
  EditLoopData *vbo_data;
  int cd_ofs;
} MeshExtract_EditUVFdotData_Data;

static void *extract_fdots_edituv_data_init(const MeshRenderData *mr,
                                            struct MeshBatchCache *UNUSED(cache),
                                            void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "flag", GPU_COMP_U8, 4, GPU_FETCH_INT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  MeshExtract_EditUVFdotData_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (EditLoopData *)GPU_vertbuf_get_data(vbo);
  data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  return data;
}

static void extract_fdots_edituv_data_iter_poly_bm(const MeshRenderData *mr,
                                                   BMFace *f,
                                                   const int UNUSED(f_index),
                                                   void *_data)
{
  MeshExtract_EditUVFdotData_Data *data = _data;
  EditLoopData *eldata = &data->vbo_data[BM_elem_index_get(f)];
  memset(eldata, 0x0, sizeof(*eldata));
  mesh_render_data_face_flag(mr, f, data->cd_ofs, eldata);
}

static void extract_fdots_edituv_data_iter_poly_mesh(const MeshRenderData *mr,
                                                     const MPoly *UNUSED(mp),
                                                     const int mp_index,
                                                     void *_data)
{
  MeshExtract_EditUVFdotData_Data *data = _data;
  EditLoopData *eldata = &data->vbo_data[mp_index];
  memset(eldata, 0x0, sizeof(*eldata));
  BMFace *efa = bm_original_face_get(mr, mp_index);
  if (efa) {
    mesh_render_data_face_flag(mr, efa, data->cd_ofs, eldata);
  }
}

static void extract_fdots_edituv_data_finish(const MeshRenderData *UNUSED(mr),
                                             struct MeshBatchCache *UNUSED(cache),
                                             void *UNUSED(buf),
                                             void *data)
{
  MEM_freeN(data);
}

const MeshExtract extract_fdots_edituv_data = {
    .init = extract_fdots_edituv_data_init,
    .iter_poly_bm = extract_fdots_edituv_data_iter_poly_bm,
    .iter_poly_mesh = extract_fdots_edituv_data_iter_poly_mesh,
    .finish = extract_fdots_edituv_data_finish,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdots_edituv_data)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Skin Modifier Roots
 * \{ */

typedef struct SkinRootData {
  float size;
  float local_pos[3];
} SkinRootData;

static void *extract_skin_roots_init(const MeshRenderData *mr,
                                     struct MeshBatchCache *UNUSED(cache),
                                     void *buf)
{
  GPUVertBuf *vbo = buf;
  /* Exclusively for edit mode. */
  BLI_assert(mr->bm);

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "local_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->bm->totvert);

  SkinRootData *vbo_data = (SkinRootData *)GPU_vertbuf_get_data(vbo);

  int root_len = 0;
  int cd_ofs = CustomData_get_offset(&mr->bm->vdata, CD_MVERT_SKIN);

  BMIter iter;
  BMVert *eve;
  BM_ITER_MESH (eve, &iter, mr->bm, BM_VERTS_OF_MESH) {
    const MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_ofs);
    if (vs->flag & MVERT_SKIN_ROOT) {
      vbo_data->size = (vs->radius[0] + vs->radius[1]) * 0.5f;
      copy_v3_v3(vbo_data->local_pos, bm_vert_co_get(mr, eve));
      vbo_data++;
      root_len++;
    }
  }

  /* It's really unlikely that all verts will be roots. Resize to avoid losing VRAM. */
  GPU_vertbuf_data_len_set(vbo, root_len);

  return NULL;
}

const MeshExtract extract_skin_roots = {
    .init = extract_skin_roots_init,
    .data_type = 0,
    .use_threading = false,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.skin_roots)};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Selection Index
 * \{ */

static void *extract_select_idx_init(const MeshRenderData *mr,
                                     struct MeshBatchCache *UNUSED(cache),
                                     void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* TODO rename "color" to something more descriptive. */
    GPU_vertformat_attr_add(&format, "color", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);
  return GPU_vertbuf_get_data(vbo);
}

/* TODO Use #glVertexID to get loop index and use the data structure on the CPU to retrieve the
 * select element associated with this loop ID. This would remove the need for this separate
 * index VBO's. We could upload the p/e/v_origindex as a buffer texture and sample it inside the
 * shader to output original index. */

static void extract_poly_idx_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                          BMFace *f,
                                          const int f_index,
                                          void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    ((uint32_t *)data)[l_index] = f_index;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_idx_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                          BMFace *f,
                                          const int UNUSED(f_index),
                                          void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    ((uint32_t *)data)[l_index] = BM_elem_index_get(l_iter->e);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_vert_idx_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                          BMFace *f,
                                          const int UNUSED(f_index),
                                          void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    ((uint32_t *)data)[l_index] = BM_elem_index_get(l_iter->v);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_idx_iter_ledge_bm(const MeshRenderData *mr,
                                           BMEdge *eed,
                                           const int ledge_index,
                                           void *data)
{
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 0] = BM_elem_index_get(eed);
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 1] = BM_elem_index_get(eed);
}

static void extract_vert_idx_iter_ledge_bm(const MeshRenderData *mr,
                                           BMEdge *eed,
                                           const int ledge_index,
                                           void *data)
{
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 0] = BM_elem_index_get(eed->v1);
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 1] = BM_elem_index_get(eed->v2);
}

static void extract_vert_idx_iter_lvert_bm(const MeshRenderData *mr,
                                           BMVert *eve,
                                           const int lvert_index,
                                           void *data)
{
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  ((uint32_t *)data)[offset + lvert_index] = BM_elem_index_get(eve);
}

static void extract_poly_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int mp_index,
                                            void *data)
{
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    ((uint32_t *)data)[ml_index] = (mr->p_origindex) ? mr->p_origindex[mp_index] : mp_index;
  }
}

static void extract_edge_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int UNUSED(mp_index),
                                            void *data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    ((uint32_t *)data)[ml_index] = (mr->e_origindex) ? mr->e_origindex[ml->e] : ml->e;
  }
}

static void extract_vert_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int UNUSED(mp_index),
                                            void *data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    ((uint32_t *)data)[ml_index] = (mr->v_origindex) ? mr->v_origindex[ml->v] : ml->v;
  }
}

static void extract_edge_idx_iter_ledge_mesh(const MeshRenderData *mr,
                                             const MEdge *UNUSED(med),
                                             const int ledge_index,
                                             void *data)
{
  const int e_index = mr->ledges[ledge_index];
  const int e_orig = (mr->e_origindex) ? mr->e_origindex[e_index] : e_index;
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 0] = e_orig;
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 1] = e_orig;
}

static void extract_vert_idx_iter_ledge_mesh(const MeshRenderData *mr,
                                             const MEdge *med,
                                             const int ledge_index,
                                             void *data)
{
  int v1_orig = (mr->v_origindex) ? mr->v_origindex[med->v1] : med->v1;
  int v2_orig = (mr->v_origindex) ? mr->v_origindex[med->v2] : med->v2;
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 0] = v1_orig;
  ((uint32_t *)data)[mr->loop_len + ledge_index * 2 + 1] = v2_orig;
}

static void extract_vert_idx_iter_lvert_mesh(const MeshRenderData *mr,
                                             const MVert *UNUSED(mv),
                                             const int lvert_index,
                                             void *data)
{
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int v_index = mr->lverts[lvert_index];
  const int v_orig = (mr->v_origindex) ? mr->v_origindex[v_index] : v_index;
  ((uint32_t *)data)[offset + lvert_index] = v_orig;
}

const MeshExtract extract_poly_idx = {
    .init = extract_select_idx_init,
    .iter_poly_bm = extract_poly_idx_iter_poly_bm,
    .iter_poly_mesh = extract_poly_idx_iter_poly_mesh,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.poly_idx)};

const MeshExtract extract_edge_idx = {
    .init = extract_select_idx_init,
    .iter_poly_bm = extract_edge_idx_iter_poly_bm,
    .iter_poly_mesh = extract_edge_idx_iter_poly_mesh,
    .iter_ledge_bm = extract_edge_idx_iter_ledge_bm,
    .iter_ledge_mesh = extract_edge_idx_iter_ledge_mesh,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.edge_idx)};

const MeshExtract extract_vert_idx = {
    .init = extract_select_idx_init,
    .iter_poly_bm = extract_vert_idx_iter_poly_bm,
    .iter_poly_mesh = extract_vert_idx_iter_poly_mesh,
    .iter_ledge_bm = extract_vert_idx_iter_ledge_bm,
    .iter_ledge_mesh = extract_vert_idx_iter_ledge_mesh,
    .iter_lvert_bm = extract_vert_idx_iter_lvert_bm,
    .iter_lvert_mesh = extract_vert_idx_iter_lvert_mesh,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.vert_idx)};

static void *extract_fdot_idx_init(const MeshRenderData *mr,
                                   struct MeshBatchCache *UNUSED(cache),
                                   void *buf)
{
  GPUVertBuf *vbo = buf;
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* TODO rename "color" to something more descriptive. */
    GPU_vertformat_attr_add(&format, "color", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);
  return GPU_vertbuf_get_data(vbo);
}

static void extract_fdot_idx_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                          BMFace *UNUSED(f),
                                          const int f_index,
                                          void *data)
{
  ((uint32_t *)data)[f_index] = f_index;
}

static void extract_fdot_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *UNUSED(mp),
                                            const int mp_index,
                                            void *data)
{
  if (mr->p_origindex != NULL) {
    ((uint32_t *)data)[mp_index] = mr->p_origindex[mp_index];
  }
  else {
    ((uint32_t *)data)[mp_index] = mp_index;
  }
}

const MeshExtract extract_fdot_idx = {
    .init = extract_fdot_idx_init,
    .iter_poly_bm = extract_fdot_idx_iter_poly_bm,
    .iter_poly_mesh = extract_fdot_idx_iter_poly_mesh,
    .data_type = 0,
    .use_threading = true,
    .mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdot_idx)};
