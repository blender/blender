/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "extract_mesh.hh"

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
                                 MeshBatchCache * /*cache*/,
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
                                         const int /*f_index*/,
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
                                           const int poly_index,
                                           void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  const bool poly_hidden = mr->hide_poly && mr->hide_poly[poly_index];

  for (const int ml_index : mr->polys[poly_index]) {
    const int vert_i = mr->corner_verts[ml_index];
    PosNorLoop *vert = &data->vbo_data[ml_index];
    const bool vert_hidden = mr->hide_vert && mr->hide_vert[vert_i];
    copy_v3_v3(vert->pos, mr->vert_positions[vert_i]);
    vert->nor = data->normals[vert_i].low;
    /* Flag for paint mode overlay. */
    if (poly_hidden || vert_hidden ||
        ((mr->v_origindex) && (mr->v_origindex[vert_i] == ORIGINDEX_NONE)))
    {
      vert->nor.w = -1;
    }
    else if (mr->select_vert && mr->select_vert[vert_i]) {
      vert->nor.w = 1;
    }
    else {
      vert->nor.w = 0;
    }
  }
}

static void extract_pos_nor_iter_loose_edge_bm(const MeshRenderData *mr,
                                               const BMEdge *eed,
                                               const int loose_edge_i,
                                               void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);

  int l_index = mr->loop_len + loose_edge_i * 2;
  PosNorLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert[0].pos, bm_vert_co_get(mr, eed->v1));
  copy_v3_v3(vert[1].pos, bm_vert_co_get(mr, eed->v2));
  vert[0].nor = data->normals[BM_elem_index_get(eed->v1)].low;
  vert[1].nor = data->normals[BM_elem_index_get(eed->v2)].low;
}

static void extract_pos_nor_iter_loose_edge_mesh(const MeshRenderData *mr,
                                                 const int2 edge,
                                                 const int loose_edge_i,
                                                 void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  const int ml_index = mr->loop_len + loose_edge_i * 2;
  PosNorLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert[0].pos, mr->vert_positions[edge[0]]);
  copy_v3_v3(vert[1].pos, mr->vert_positions[edge[1]]);
  vert[0].nor = data->normals[edge[0]].low;
  vert[1].nor = data->normals[edge[1]].low;
}

static void extract_pos_nor_iter_loose_vert_bm(const MeshRenderData *mr,
                                               const BMVert *eve,
                                               const int loose_vert_i,
                                               void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int l_index = offset + loose_vert_i;
  PosNorLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert->pos, bm_vert_co_get(mr, eve));
  vert->nor = data->normals[BM_elem_index_get(eve)].low;
}

static void extract_pos_nor_iter_loose_vert_mesh(const MeshRenderData *mr,
                                                 const int loose_vert_i,
                                                 void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int ml_index = offset + loose_vert_i;
  const int v_index = mr->loose_verts[loose_vert_i];
  PosNorLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert->pos, mr->vert_positions[v_index]);
  vert->nor = data->normals[v_index].low;
}

static void extract_pos_nor_finish(const MeshRenderData * /*mr*/,
                                   MeshBatchCache * /*cache*/,
                                   void * /*buf*/,
                                   void *_data)
{
  MeshExtract_PosNor_Data *data = static_cast<MeshExtract_PosNor_Data *>(_data);
  MEM_freeN(data->normals);
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

static void extract_vertex_flags(const MeshRenderData *mr, char *flags)
{
  for (int i = 0; i < mr->vert_len; i++) {
    char *flag = &flags[i];
    const bool vert_hidden = mr->hide_vert && mr->hide_vert[i];
    /* Flag for paint mode overlay. */
    if (vert_hidden || ((mr->v_origindex) && (mr->v_origindex[i] == ORIGINDEX_NONE))) {
      *flag = -1;
    }
    else if (mr->select_vert && mr->select_vert[i]) {
      *flag = 1;
    }
    else {
      *flag = 0;
    }
  }
}

static void extract_pos_nor_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                        const MeshRenderData *mr,
                                        MeshBatchCache *cache,
                                        void *buffer,
                                        void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;

  /* Initialize the vertex buffer, it was already allocated. */
  GPU_vertbuf_init_build_on_device(
      vbo, draw_subdiv_get_pos_nor_format(), subdiv_cache->num_subdiv_loops + loose_geom.loop_len);

  if (subdiv_cache->num_subdiv_loops == 0) {
    return;
  }

  GPUVertBuf *flags_buffer = GPU_vertbuf_calloc();
  static GPUVertFormat flag_format = {0};
  if (flag_format.attr_len == 0) {
    GPU_vertformat_attr_add(&flag_format, "flag", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  GPU_vertbuf_init_with_format(flags_buffer, &flag_format);
  GPU_vertbuf_data_alloc(flags_buffer, divide_ceil_u(mr->vert_len, 4));
  char *flags = static_cast<char *>(GPU_vertbuf_get_data(flags_buffer));
  extract_vertex_flags(mr, flags);
  GPU_vertbuf_tag_dirty(flags_buffer);

  GPUVertBuf *orco_vbo = cache->final.buff.vbo.orco;

  if (orco_vbo) {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
       * attributes. This is a substantial waste of video-ram and should be done another way.
       * Unfortunately, at the time of writing, I did not found any other "non disruptive"
       * alternative. */
      GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    }
    GPU_vertbuf_init_build_on_device(orco_vbo, &format, subdiv_cache->num_subdiv_loops);
  }

  draw_subdiv_extract_pos_nor(subdiv_cache, flags_buffer, vbo, orco_vbo);

  if (subdiv_cache->use_custom_loop_normals) {
    Mesh *coarse_mesh = subdiv_cache->mesh;
    const float(*loop_normals)[3] = static_cast<const float(*)[3]>(
        CustomData_get_layer(&coarse_mesh->ldata, CD_NORMAL));
    BLI_assert(loop_normals != nullptr);

    GPUVertBuf *src_custom_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_with_format(src_custom_normals, get_custom_normals_format());
    GPU_vertbuf_data_alloc(src_custom_normals, coarse_mesh->totloop);

    memcpy(GPU_vertbuf_get_data(src_custom_normals),
           loop_normals,
           sizeof(float[3]) * coarse_mesh->totloop);

    GPUVertBuf *dst_custom_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(
        dst_custom_normals, get_custom_normals_format(), subdiv_cache->num_subdiv_loops);

    draw_subdiv_interp_custom_data(
        subdiv_cache, src_custom_normals, dst_custom_normals, GPU_COMP_F32, 3, 0);

    draw_subdiv_finalize_custom_normals(subdiv_cache, dst_custom_normals, vbo);

    GPU_vertbuf_discard(src_custom_normals);
    GPU_vertbuf_discard(dst_custom_normals);
  }
  else {
    /* We cannot evaluate vertex normals using the limit surface, so compute them manually. */
    GPUVertBuf *subdiv_loop_subdiv_vert_index = draw_subdiv_build_origindex_buffer(
        subdiv_cache->subdiv_loop_subdiv_vert_index, subdiv_cache->num_subdiv_loops);

    GPUVertBuf *vert_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(
        vert_normals, get_normals_format(), subdiv_cache->num_subdiv_verts);

    draw_subdiv_accumulate_normals(subdiv_cache,
                                   vbo,
                                   subdiv_cache->subdiv_vertex_face_adjacency_offsets,
                                   subdiv_cache->subdiv_vertex_face_adjacency,
                                   subdiv_loop_subdiv_vert_index,
                                   vert_normals);

    draw_subdiv_finalize_normals(subdiv_cache, vert_normals, subdiv_loop_subdiv_vert_index, vbo);

    GPU_vertbuf_discard(vert_normals);
    GPU_vertbuf_discard(subdiv_loop_subdiv_vert_index);
  }

  GPU_vertbuf_discard(flags_buffer);
}

static void extract_pos_nor_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                              const MeshRenderData * /*mr*/,
                                              void *buffer,
                                              void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  if (loose_geom.loop_len == 0) {
    return;
  }

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  uint offset = subdiv_cache->num_subdiv_loops;

  /* TODO(@kevindietrich): replace this when compressed normals are supported. */
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
  extractor.iter_loose_edge_bm = extract_pos_nor_iter_loose_edge_bm;
  extractor.iter_loose_edge_mesh = extract_pos_nor_iter_loose_edge_mesh;
  extractor.iter_loose_vert_bm = extract_pos_nor_iter_loose_vert_bm;
  extractor.iter_loose_vert_mesh = extract_pos_nor_iter_loose_vert_mesh;
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
                                    MeshBatchCache * /*cache*/,
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
                                            const int /*f_index*/,
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
                                              const int poly_index,
                                              void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  const bool poly_hidden = mr->hide_poly && mr->hide_poly[poly_index];

  for (const int ml_index : mr->polys[poly_index]) {
    const int vert_i = mr->corner_verts[ml_index];

    const bool vert_hidden = mr->hide_vert && mr->hide_vert[vert_i];
    PosNorHQLoop *vert = &data->vbo_data[ml_index];
    copy_v3_v3(vert->pos, mr->vert_positions[vert_i]);
    copy_v3_v3_short(vert->nor, data->normals[vert_i].high);

    /* Flag for paint mode overlay. */
    if (poly_hidden || vert_hidden ||
        ((mr->v_origindex) && (mr->v_origindex[vert_i] == ORIGINDEX_NONE)))
    {
      vert->nor[3] = -1;
    }
    else if (mr->select_vert && mr->select_vert[vert_i]) {
      vert->nor[3] = 1;
    }
    else {
      vert->nor[3] = 0;
    }
  }
}

static void extract_pos_nor_hq_iter_loose_edge_bm(const MeshRenderData *mr,
                                                  const BMEdge *eed,
                                                  const int loose_edge_i,
                                                  void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  int l_index = mr->loop_len + loose_edge_i * 2;
  PosNorHQLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert[0].pos, bm_vert_co_get(mr, eed->v1));
  copy_v3_v3(vert[1].pos, bm_vert_co_get(mr, eed->v2));
  copy_v3_v3_short(vert[0].nor, data->normals[BM_elem_index_get(eed->v1)].high);
  vert[0].nor[3] = 0;
  copy_v3_v3_short(vert[1].nor, data->normals[BM_elem_index_get(eed->v2)].high);
  vert[1].nor[3] = 0;
}

static void extract_pos_nor_hq_iter_loose_edge_mesh(const MeshRenderData *mr,
                                                    const int2 edge,
                                                    const int loose_edge_i,
                                                    void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  const int ml_index = mr->loop_len + loose_edge_i * 2;
  PosNorHQLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert[0].pos, mr->vert_positions[edge[0]]);
  copy_v3_v3(vert[1].pos, mr->vert_positions[edge[1]]);
  copy_v3_v3_short(vert[0].nor, data->normals[edge[0]].high);
  vert[0].nor[3] = 0;
  copy_v3_v3_short(vert[1].nor, data->normals[edge[1]].high);
  vert[1].nor[3] = 0;
}

static void extract_pos_nor_hq_iter_loose_vert_bm(const MeshRenderData *mr,
                                                  const BMVert *eve,
                                                  const int loose_vert_i,
                                                  void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int l_index = offset + loose_vert_i;
  PosNorHQLoop *vert = &data->vbo_data[l_index];
  copy_v3_v3(vert->pos, bm_vert_co_get(mr, eve));
  copy_v3_v3_short(vert->nor, data->normals[BM_elem_index_get(eve)].high);
  vert->nor[3] = 0;
}

static void extract_pos_nor_hq_iter_loose_vert_mesh(const MeshRenderData *mr,
                                                    const int loose_vert_i,
                                                    void *_data)
{
  MeshExtract_PosNorHQ_Data *data = static_cast<MeshExtract_PosNorHQ_Data *>(_data);
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int ml_index = offset + loose_vert_i;
  const int v_index = mr->loose_verts[loose_vert_i];
  PosNorHQLoop *vert = &data->vbo_data[ml_index];
  copy_v3_v3(vert->pos, mr->vert_positions[v_index]);
  copy_v3_v3_short(vert->nor, data->normals[v_index].high);
  vert->nor[3] = 0;
}

static void extract_pos_nor_hq_finish(const MeshRenderData * /*mr*/,
                                      MeshBatchCache * /*cache*/,
                                      void * /*buf*/,
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
  extractor.iter_loose_edge_bm = extract_pos_nor_hq_iter_loose_edge_bm;
  extractor.iter_loose_edge_mesh = extract_pos_nor_hq_iter_loose_edge_mesh;
  extractor.iter_loose_vert_bm = extract_pos_nor_hq_iter_loose_vert_bm;
  extractor.iter_loose_vert_mesh = extract_pos_nor_hq_iter_loose_vert_mesh;
  extractor.finish = extract_pos_nor_hq_finish;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_PosNorHQ_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.pos_nor);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_pos_nor = blender::draw::create_extractor_pos_nor();
const MeshExtract extract_pos_nor_hq = blender::draw::create_extractor_pos_nor_hq();
