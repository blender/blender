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

#include "MEM_guardedalloc.h"

#include "extract_mesh.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Position and Vertex Normal
 * \{ */

struct PosNorLoop {
  float pos[3];
  GPUPackedNormal nor;
};

struct MeshExtract_PosNor_Data {
  PosNorLoop *vbo_data;
  GPUNormal *normals;
};

static void extract_pos_nor_init(const MeshRenderData *mr,
                                 struct MeshBatchCache *UNUSED(cache),
                                 void *buf,
                                 void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
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
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(tls_data);
  data->vbo_data = static_cast<PosNorLoop *>(GPU_vertbuf_get_data(vbo));
  data->normals = (GPUNormal *)MEM_mallocN(sizeof(GPUNormal) * mr->vert_len, __func__);

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
    for (int v = 0; v < mr->vert_len; v++) {
      data->normals[v].low = GPU_normal_convert_i10_v3(mr->vert_normals[v]);
    }
  }
}

static void extract_pos_nor_iter_poly_bm(const MeshRenderData *mr,
                                         const BMFace *f,
                                         const int UNUSED(f_index),
                                         void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
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
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);

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
                                          const BMEdge *eed,
                                          const int ledge_index,
                                          void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);

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
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  const int ml_index = mr->loop_len + ledge_index * 2;
  PosNorLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert[0].pos, mr->mvert[med->v1].co);
  copy_v3_v3(vert[1].pos, mr->mvert[med->v2].co);
  vert[0].nor = data->normals[med->v1].low;
  vert[1].nor = data->normals[med->v2].low;
}

static void extract_pos_nor_iter_lvert_bm(const MeshRenderData *mr,
                                          const BMVert *eve,
                                          const int lvert_index,
                                          void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
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
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
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
                                   void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  MEM_freeN(data->normals);
}

static GPUVertFormat *get_pos_nor_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "vnor");
  }
  return &format;
}

static GPUVertFormat *get_normals_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  return &format;
}

static GPUVertFormat *get_custom_normals_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  return &format;
}

static void extract_pos_nor_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                        const MeshRenderData *UNUSED(mr),
                                        struct MeshBatchCache *UNUSED(cache),
                                        void *buffer,
                                        void *UNUSED(data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;

  /* Initialize the vertex buffer, it was already allocated. */
  GPU_vertbuf_init_build_on_device(
      vbo, get_pos_nor_format(), subdiv_cache->num_subdiv_loops + loose_geom.loop_len);

  if (subdiv_cache->num_subdiv_loops == 0) {
    return;
  }

  draw_subdiv_extract_pos_nor(subdiv_cache, vbo);

  if (subdiv_cache->use_custom_loop_normals) {
    Mesh *coarse_mesh = subdiv_cache->mesh;
    float(*lnors)[3] = static_cast<float(*)[3]>(
        CustomData_get_layer(&coarse_mesh->ldata, CD_NORMAL));
    BLI_assert(lnors != NULL);

    GPUVertBuf *src_custom_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_with_format(src_custom_normals, get_custom_normals_format());
    GPU_vertbuf_data_alloc(src_custom_normals, coarse_mesh->totloop);

    memcpy(
        GPU_vertbuf_get_data(src_custom_normals), lnors, sizeof(float[3]) * coarse_mesh->totloop);

    GPUVertBuf *dst_custom_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(
        dst_custom_normals, get_custom_normals_format(), subdiv_cache->num_subdiv_loops);

    draw_subdiv_interp_custom_data(
        subdiv_cache, src_custom_normals, dst_custom_normals, 3, 0, false);

    draw_subdiv_finalize_custom_normals(subdiv_cache, dst_custom_normals, vbo);

    GPU_vertbuf_discard(src_custom_normals);
    GPU_vertbuf_discard(dst_custom_normals);
  }
  else {
    /* We cannot evaluate vertex normals using the limit surface, so compute them manually. */
    GPUVertBuf *subdiv_loop_subdiv_vert_index = draw_subdiv_build_origindex_buffer(
        subdiv_cache->subdiv_loop_subdiv_vert_index, subdiv_cache->num_subdiv_loops);

    GPUVertBuf *vertex_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(
        vertex_normals, get_normals_format(), subdiv_cache->num_subdiv_verts);

    draw_subdiv_accumulate_normals(subdiv_cache,
                                   vbo,
                                   subdiv_cache->subdiv_vertex_face_adjacency_offsets,
                                   subdiv_cache->subdiv_vertex_face_adjacency,
                                   subdiv_loop_subdiv_vert_index,
                                   vertex_normals);

    draw_subdiv_finalize_normals(subdiv_cache, vertex_normals, subdiv_loop_subdiv_vert_index, vbo);

    GPU_vertbuf_discard(vertex_normals);
    GPU_vertbuf_discard(subdiv_loop_subdiv_vert_index);
  }
}

static void extract_pos_nor_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                              const MeshRenderData *UNUSED(mr),
                                              void *buffer,
                                              void *UNUSED(data))
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  if (loose_geom.loop_len == 0) {
    return;
  }

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  uint offset = subdiv_cache->num_subdiv_loops;

  /* TODO(kevindietrich) : replace this when compressed normals are supported. */
  struct SubdivPosNorLoop {
    float pos[3];
    float nor[3];
    float flag;
  };

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(vbo);

  blender::Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);

  SubdivPosNorLoop edge_data[2];
  memset(edge_data, 0, sizeof(SubdivPosNorLoop) * 2);
  for (const DRWSubdivLooseEdge &loose_edge : loose_edges) {
    const DRWSubdivLooseVertex &v1 = loose_geom.verts[loose_edge.loose_subdiv_v1_index];
    const DRWSubdivLooseVertex &v2 = loose_geom.verts[loose_edge.loose_subdiv_v2_index];

    copy_v3_v3(edge_data[0].pos, v1.co);
    copy_v3_v3(edge_data[1].pos, v2.co);

    GPU_vertbuf_update_sub(
        vbo, offset * sizeof(SubdivPosNorLoop), sizeof(SubdivPosNorLoop) * 2, &edge_data);

    offset += 2;
  }

  SubdivPosNorLoop vert_data;
  memset(&vert_data, 0, sizeof(SubdivPosNorLoop));
  blender::Span<DRWSubdivLooseVertex> loose_verts = draw_subdiv_cache_get_loose_verts(
      subdiv_cache);

  for (const DRWSubdivLooseVertex &loose_vert : loose_verts) {
    copy_v3_v3(vert_data.pos, loose_vert.co);

    GPU_vertbuf_update_sub(
        vbo, offset * sizeof(SubdivPosNorLoop), sizeof(SubdivPosNorLoop), &vert_data);

    offset += 1;
  }
}

constexpr MeshExtract create_extractor_pos_nor()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_pos_nor_init;
  extractor.iter_poly_bm = extract_pos_nor_iter_poly_bm;
  extractor.iter_poly_mesh = extract_pos_nor_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_pos_nor_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_pos_nor_iter_ledge_mesh;
  extractor.iter_lvert_bm = extract_pos_nor_iter_lvert_bm;
  extractor.iter_lvert_mesh = extract_pos_nor_iter_lvert_mesh;
  extractor.finish = extract_pos_nor_finish;
  extractor.init_subdiv = extract_pos_nor_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_pos_nor_loose_geom_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_PosNor_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.pos_nor);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Position and High Quality Vertex Normal
 * \{ */

struct PosNorHQLoop {
  float pos[3];
  short nor[4];
};

struct MeshExtract_PosNorHQ_Data {
  PosNorHQLoop *vbo_data;
  GPUNormal *normals;
};

static void extract_pos_nor_hq_init(const MeshRenderData *mr,
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *buf,
                                    void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
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
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(tls_data);
  data->vbo_data = static_cast<PosNorHQLoop *>(GPU_vertbuf_get_data(vbo));
  data->normals = (GPUNormal *)MEM_mallocN(sizeof(GPUNormal) * mr->vert_len, __func__);

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
    for (int v = 0; v < mr->vert_len; v++) {
      normal_float_to_short_v3(data->normals[v].high, mr->vert_normals[v]);
    }
  }
}

static void extract_pos_nor_hq_iter_poly_bm(const MeshRenderData *mr,
                                            const BMFace *f,
                                            const int UNUSED(f_index),
                                            void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    PosNorHQLoop *vert = &data->vbo_data[l_index];
    copy_v3_v3(vert->pos, bm_vert_co_get(mr, l_iter->v));
    copy_v3_v3_short(vert->nor, data->normals[BM_elem_index_get(l_iter->v)].high);

    vert->nor[3] = BM_elem_flag_test(f, BM_ELEM_HIDDEN) ? -1 : 0;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_pos_nor_hq_iter_poly_mesh(const MeshRenderData *mr,
                                              const MPoly *mp,
                                              const int UNUSED(mp_index),
                                              void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
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
                                             const BMEdge *eed,
                                             const int ledge_index,
                                             void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
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
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
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
                                             const BMVert *eve,
                                             const int lvert_index,
                                             void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
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
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
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
                                      void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  MEM_freeN(data->normals);
}

constexpr MeshExtract create_extractor_pos_nor_hq()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_pos_nor_hq_init;
  extractor.init_subdiv = extract_pos_nor_init_subdiv;
  extractor.iter_poly_bm = extract_pos_nor_hq_iter_poly_bm;
  extractor.iter_poly_mesh = extract_pos_nor_hq_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_pos_nor_hq_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_pos_nor_hq_iter_ledge_mesh;
  extractor.iter_lvert_bm = extract_pos_nor_hq_iter_lvert_bm;
  extractor.iter_lvert_mesh = extract_pos_nor_hq_iter_lvert_mesh;
  extractor.finish = extract_pos_nor_hq_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_PosNorHQ_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.pos_nor);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_pos_nor = blender::draw::create_extractor_pos_nor();
const MeshExtract extract_pos_nor_hq = blender::draw::create_extractor_pos_nor_hq();
}
