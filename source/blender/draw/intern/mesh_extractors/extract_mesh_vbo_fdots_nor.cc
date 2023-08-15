/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots Normal and edit flag
 * \{ */

#define NOR_AND_FLAG_DEFAULT 0
#define NOR_AND_FLAG_SELECT 1
#define NOR_AND_FLAG_ACTIVE -1
#define NOR_AND_FLAG_HIDDEN -2

static void extract_fdots_nor_init(const MeshRenderData &mr,
                                   MeshBatchCache & /*cache*/,
                                   void *buf,
                                   void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.face_len);
}

static void extract_fdots_nor_finish(const MeshRenderData &mr,
                                     MeshBatchCache & /*cache*/,
                                     void *buf,
                                     void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static float invalid_normal[3] = {0.0f, 0.0f, 0.0f};
  GPUPackedNormal *nor = (GPUPackedNormal *)GPU_vertbuf_get_data(vbo);
  BMFace *efa;

  /* Quicker than doing it for each loop. */
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    for (int f = 0; f < mr.face_len; f++) {
      efa = BM_face_at_index(mr.bm, f);
      const bool is_face_hidden = BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr.p_origindex && mr.p_origindex[f] == ORIGINDEX_NONE)) {
        nor[f] = GPU_normal_convert_i10_v3(invalid_normal);
        nor[f].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        nor[f] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f].w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                        ((efa == mr.efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                        NOR_AND_FLAG_DEFAULT);
      }
    }
  }
  else {
    for (int f = 0; f < mr.face_len; f++) {
      efa = bm_original_face_get(mr, f);
      const bool is_face_hidden = efa && BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr.p_origindex && mr.p_origindex[f] == ORIGINDEX_NONE)) {
        nor[f] = GPU_normal_convert_i10_v3(invalid_normal);
        nor[f].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        nor[f] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f].w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                        ((efa == mr.efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                        NOR_AND_FLAG_DEFAULT);
      }
    }
  }
}

constexpr MeshExtract create_extractor_fdots_nor()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_nor_init;
  extractor.finish = extract_fdots_nor_finish;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdots_nor);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots High Quality Normal and edit flag
 * \{ */

static void extract_fdots_nor_hq_init(const MeshRenderData &mr,
                                      MeshBatchCache & /*cache*/,
                                      void *buf,
                                      void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.face_len);
}

static void extract_fdots_nor_hq_finish(const MeshRenderData &mr,
                                        MeshBatchCache & /*cache*/,
                                        void *buf,
                                        void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static float invalid_normal[3] = {0.0f, 0.0f, 0.0f};
  short *nor = (short *)GPU_vertbuf_get_data(vbo);
  BMFace *efa;

  /* Quicker than doing it for each loop. */
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    for (int f = 0; f < mr.face_len; f++) {
      efa = BM_face_at_index(mr.bm, f);
      const bool is_face_hidden = BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr.p_origindex && mr.p_origindex[f] == ORIGINDEX_NONE)) {
        normal_float_to_short_v3(&nor[f * 4], invalid_normal);
        nor[f * 4 + 3] = NOR_AND_FLAG_HIDDEN;
      }
      else {
        normal_float_to_short_v3(&nor[f * 4], bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f * 4 + 3] = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                              ((efa == mr.efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                              NOR_AND_FLAG_DEFAULT);
      }
    }
  }
  else {
    for (int f = 0; f < mr.face_len; f++) {
      efa = bm_original_face_get(mr, f);
      const bool is_face_hidden = efa && BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr.p_origindex && mr.p_origindex[f] == ORIGINDEX_NONE)) {
        normal_float_to_short_v3(&nor[f * 4], invalid_normal);
        nor[f * 4 + 3] = NOR_AND_FLAG_HIDDEN;
      }
      else {
        normal_float_to_short_v3(&nor[f * 4], bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f * 4 + 3] = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                              ((efa == mr.efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                              NOR_AND_FLAG_DEFAULT);
      }
    }
  }
}

constexpr MeshExtract create_extractor_fdots_nor_hq()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_nor_hq_init;
  extractor.finish = extract_fdots_nor_hq_finish;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdots_nor);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_fdots_nor = blender::draw::create_extractor_fdots_nor();
const MeshExtract extract_fdots_nor_hq = blender::draw::create_extractor_fdots_nor_hq();
