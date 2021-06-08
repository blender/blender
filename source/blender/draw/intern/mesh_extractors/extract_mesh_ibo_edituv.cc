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
/** \name Extract Edit UV Triangles Indices
 * \{ */

struct MeshExtract_EditUvElem_Data {
  GPUIndexBufBuilder elb;
  bool sync_selection;
};

static void *extract_edituv_tris_init(const MeshRenderData *mr,
                                      struct MeshBatchCache *UNUSED(cache),
                                      void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(
      MEM_callocN(sizeof(*data), __func__));
  GPU_indexbuf_init(&data->elb, GPU_PRIM_TRIS, mr->tri_len, mr->loop_len);
  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_tri_add(
    MeshExtract_EditUvElem_Data *data, bool hidden, bool selected, int v1, int v2, int v3)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_add_tri_verts(&data->elb, v1, v2, v3);
  }
}

static void extract_edituv_tris_iter_looptri_bm(const MeshRenderData *UNUSED(mr),
                                                BMLoop **elt,
                                                const int UNUSED(elt_index),
                                                void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  edituv_tri_add(data,
                 BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN),
                 BM_elem_flag_test(elt[0]->f, BM_ELEM_SELECT),
                 BM_elem_index_get(elt[0]),
                 BM_elem_index_get(elt[1]),
                 BM_elem_index_get(elt[2]));
}

static void extract_edituv_tris_iter_looptri_mesh(const MeshRenderData *mr,
                                                  const MLoopTri *mlt,
                                                  const int UNUSED(elt_index),
                                                  void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  const MPoly *mp = &mr->mpoly[mlt->poly];
  edituv_tri_add(data,
                 (mp->flag & ME_HIDE) != 0,
                 (mp->flag & ME_FACE_SEL) != 0,
                 mlt->tri[0],
                 mlt->tri[1],
                 mlt->tri[2]);
}

static void extract_edituv_tris_finish(const MeshRenderData *UNUSED(mr),
                                       struct MeshBatchCache *UNUSED(cache),
                                       void *buf,
                                       void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

constexpr MeshExtract create_extractor_edituv_tris()
{
  MeshExtract extractor = {0};
  extractor.init = extract_edituv_tris_init;
  extractor.iter_looptri_bm = extract_edituv_tris_iter_looptri_bm;
  extractor.iter_looptri_mesh = extract_edituv_tris_iter_looptri_mesh;
  extractor.finish = extract_edituv_tris_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.edituv_tris);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Line Indices around faces
 * \{ */

static void *extract_edituv_lines_init(const MeshRenderData *mr,
                                       struct MeshBatchCache *UNUSED(cache),
                                       void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(
      MEM_callocN(sizeof(*data), __func__));
  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES, mr->loop_len, mr->loop_len);
  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_edge_add(
    MeshExtract_EditUvElem_Data *data, bool hidden, bool selected, int v1, int v2)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_add_line_verts(&data->elb, v1, v2);
  }
}

static void extract_edituv_lines_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                              BMFace *f,
                                              const int UNUSED(f_index),
                                              void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    edituv_edge_add(data,
                    BM_elem_flag_test_bool(f, BM_ELEM_HIDDEN),
                    BM_elem_flag_test_bool(f, BM_ELEM_SELECT),
                    l_index,
                    BM_elem_index_get(l_iter->next));
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edituv_lines_iter_poly_mesh(const MeshRenderData *mr,
                                                const MPoly *mp,
                                                const int UNUSED(mp_index),
                                                void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    const int ml_index_last = mp->totloop + mp->loopstart - 1;
    const int ml_index_next = (ml_index == ml_index_last) ? mp->loopstart : (ml_index + 1);
    const bool real_edge = (mr->e_origindex == nullptr ||
                            mr->e_origindex[ml->e] != ORIGINDEX_NONE);
    edituv_edge_add(data,
                    (mp->flag & ME_HIDE) != 0 || !real_edge,
                    (mp->flag & ME_FACE_SEL) != 0,
                    ml_index,
                    ml_index_next);
  }
}

static void extract_edituv_lines_finish(const MeshRenderData *UNUSED(mr),
                                        struct MeshBatchCache *UNUSED(cache),
                                        void *buf,
                                        void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

constexpr MeshExtract create_extractor_edituv_lines()
{
  MeshExtract extractor = {0};
  extractor.init = extract_edituv_lines_init;
  extractor.iter_poly_bm = extract_edituv_lines_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edituv_lines_iter_poly_mesh;
  extractor.finish = extract_edituv_lines_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.edituv_lines);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Points Indices
 * \{ */

static void *extract_edituv_points_init(const MeshRenderData *mr,
                                        struct MeshBatchCache *UNUSED(cache),
                                        void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(
      MEM_callocN(sizeof(*data), __func__));
  GPU_indexbuf_init(&data->elb, GPU_PRIM_POINTS, mr->loop_len, mr->loop_len);
  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_point_add(MeshExtract_EditUvElem_Data *data,
                                 bool hidden,
                                 bool selected,
                                 int v1)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_add_point_vert(&data->elb, v1);
  }
}

static void extract_edituv_points_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                               BMFace *f,
                                               const int UNUSED(f_index),
                                               void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    edituv_point_add(
        data, BM_elem_flag_test(f, BM_ELEM_HIDDEN), BM_elem_flag_test(f, BM_ELEM_SELECT), l_index);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edituv_points_iter_poly_mesh(const MeshRenderData *mr,
                                                 const MPoly *mp,
                                                 const int UNUSED(mp_index),
                                                 void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    const bool real_vert = (mr->extract_type == MR_EXTRACT_MAPPED && (mr->v_origindex) &&
                            mr->v_origindex[ml->v] != ORIGINDEX_NONE);
    edituv_point_add(
        data, ((mp->flag & ME_HIDE) != 0) || !real_vert, (mp->flag & ME_FACE_SEL) != 0, ml_index);
  }
}

static void extract_edituv_points_finish(const MeshRenderData *UNUSED(mr),
                                         struct MeshBatchCache *UNUSED(cache),
                                         void *buf,
                                         void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

constexpr MeshExtract create_extractor_edituv_points()
{
  MeshExtract extractor = {0};
  extractor.init = extract_edituv_points_init;
  extractor.iter_poly_bm = extract_edituv_points_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edituv_points_iter_poly_mesh;
  extractor.finish = extract_edituv_points_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.edituv_points);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Face-dots Indices
 * \{ */

static void *extract_edituv_fdots_init(const MeshRenderData *mr,
                                       struct MeshBatchCache *UNUSED(cache),
                                       void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(
      MEM_callocN(sizeof(*data), __func__));
  GPU_indexbuf_init(&data->elb, GPU_PRIM_POINTS, mr->poly_len, mr->poly_len);
  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_facedot_add(MeshExtract_EditUvElem_Data *data,
                                   bool hidden,
                                   bool selected,
                                   int face_index)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_set_point_vert(&data->elb, face_index, face_index);
  }
  else {
    GPU_indexbuf_set_point_restart(&data->elb, face_index);
  }
}

static void extract_edituv_fdots_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                              BMFace *f,
                                              const int f_index,
                                              void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  edituv_facedot_add(data,
                     BM_elem_flag_test_bool(f, BM_ELEM_HIDDEN),
                     BM_elem_flag_test_bool(f, BM_ELEM_SELECT),
                     f_index);
}

static void extract_edituv_fdots_iter_poly_mesh(const MeshRenderData *mr,
                                                const MPoly *mp,
                                                const int mp_index,
                                                void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  if (mr->use_subsurf_fdots) {
    /* Check #ME_VERT_FACEDOT. */
    const MLoop *mloop = mr->mloop;
    const int ml_index_end = mp->loopstart + mp->totloop;
    for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
      const MLoop *ml = &mloop[ml_index];

      const bool real_fdot = (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                              mr->p_origindex[mp_index] != ORIGINDEX_NONE);
      const bool subd_fdot = (!mr->use_subsurf_fdots ||
                              (mr->mvert[ml->v].flag & ME_VERT_FACEDOT) != 0);
      edituv_facedot_add(data,
                         ((mp->flag & ME_HIDE) != 0) || !real_fdot || !subd_fdot,
                         (mp->flag & ME_FACE_SEL) != 0,
                         mp_index);
    }
  }
  else {
    const bool real_fdot = (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                            mr->p_origindex[mp_index] != ORIGINDEX_NONE);
    edituv_facedot_add(
        data, ((mp->flag & ME_HIDE) != 0) || !real_fdot, (mp->flag & ME_FACE_SEL) != 0, mp_index);
  }
}

static void extract_edituv_fdots_finish(const MeshRenderData *UNUSED(mr),
                                        struct MeshBatchCache *UNUSED(cache),
                                        void *buf,
                                        void *_data)
{
  MeshExtract_EditUvElem_Data *data = static_cast<MeshExtract_EditUvElem_Data *>(_data);
  GPUIndexBuf *ibo = static_cast<GPUIndexBuf *>(buf);
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

constexpr MeshExtract create_extractor_edituv_fdots()
{
  MeshExtract extractor = {0};
  extractor.init = extract_edituv_fdots_init;
  extractor.iter_poly_bm = extract_edituv_fdots_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edituv_fdots_iter_poly_mesh;
  extractor.finish = extract_edituv_fdots_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, ibo.edituv_fdots);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_edituv_tris = blender::draw::create_extractor_edituv_tris();
const MeshExtract extract_edituv_lines = blender::draw::create_extractor_edituv_lines();
const MeshExtract extract_edituv_points = blender::draw::create_extractor_edituv_points();
const MeshExtract extract_edituv_fdots = blender::draw::create_extractor_edituv_fdots();
}

/** \} */
