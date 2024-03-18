/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Loop Normal
 * \{ */

static void extract_lnor_init(const MeshRenderData &mr,
                              MeshBatchCache & /*cache*/,
                              void *buf,
                              void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.loop_len);

  *(GPUPackedNormal **)tls_data = static_cast<GPUPackedNormal *>(GPU_vertbuf_get_data(vbo));
}

static void extract_lnor_iter_face_bm(const MeshRenderData &mr,
                                      const BMFace *f,
                                      const int /*f_index*/,
                                      void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    GPUPackedNormal *lnor_data = &(*(GPUPackedNormal **)data)[l_index];
    if (!mr.corner_normals.is_empty()) {
      *lnor_data = GPU_normal_convert_i10_v3(mr.corner_normals[l_index]);
    }
    else {
      if (mr.normals_domain == bke::MeshNormalDomain::Face ||
          !BM_elem_flag_test(f, BM_ELEM_SMOOTH))
      {
        *lnor_data = GPU_normal_convert_i10_v3(bm_face_no_get(mr, f));
      }
      else {
        *lnor_data = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, l_iter->v));
      }
    }
    lnor_data->w = BM_elem_flag_test(f, BM_ELEM_HIDDEN) ? -1 : 0;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_lnor_iter_face_mesh(const MeshRenderData &mr, const int face_index, void *data)
{
  const bool hidden = !mr.hide_poly.is_empty() && mr.hide_poly[face_index];

  for (const int corner : mr.faces[face_index]) {
    const int vert = mr.corner_verts[corner];
    GPUPackedNormal *lnor_data = &(*(GPUPackedNormal **)data)[corner];
    if (!mr.corner_normals.is_empty()) {
      *lnor_data = GPU_normal_convert_i10_v3(mr.corner_normals[corner]);
    }
    else if (mr.normals_domain == bke::MeshNormalDomain::Face ||
             (!mr.sharp_faces.is_empty() && mr.sharp_faces[face_index]))
    {
      *lnor_data = GPU_normal_convert_i10_v3(mr.face_normals[face_index]);
    }
    else {
      *lnor_data = GPU_normal_convert_i10_v3(mr.vert_normals[vert]);
    }

    /* Flag for paint mode overlay.
     * Only use origindex in edit mode where it is used to display the edge-normals.
     * In paint mode it will use the un-mapped data to draw the wire-frame. */
    if (hidden || (mr.edit_bmesh && (mr.v_origindex) && mr.v_origindex[vert] == ORIGINDEX_NONE)) {
      lnor_data->w = -1;
    }
    else if (!mr.select_poly.is_empty() && mr.select_poly[face_index]) {
      lnor_data->w = 1;
    }
    else {
      lnor_data->w = 0;
    }
  }
}

static GPUVertFormat *get_subdiv_lnor_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  return &format;
}

static void extract_lnor_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                     const MeshRenderData & /*mr*/,
                                     MeshBatchCache &cache,
                                     void *buffer,
                                     void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  GPUVertBuf *pos_nor = cache.final.buff.vbo.pos_nor;
  BLI_assert(pos_nor);
  GPU_vertbuf_init_build_on_device(vbo, get_subdiv_lnor_format(), subdiv_cache.num_subdiv_loops);
  draw_subdiv_build_lnor_buffer(subdiv_cache, pos_nor, vbo);
}

constexpr MeshExtract create_extractor_lnor()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lnor_init;
  extractor.init_subdiv = extract_lnor_init_subdiv;
  extractor.iter_face_bm = extract_lnor_iter_face_bm;
  extractor.iter_face_mesh = extract_lnor_iter_face_mesh;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = sizeof(GPUPackedNormal *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.lnor);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract HQ Loop Normal
 * \{ */

struct gpuHQNor {
  short x, y, z, w;
};

static void extract_lnor_hq_init(const MeshRenderData &mr,
                                 MeshBatchCache & /*cache*/,
                                 void *buf,
                                 void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.loop_len);

  *(gpuHQNor **)tls_data = static_cast<gpuHQNor *>(GPU_vertbuf_get_data(vbo));
}

static void extract_lnor_hq_iter_face_bm(const MeshRenderData &mr,
                                         const BMFace *f,
                                         const int /*f_index*/,
                                         void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (!mr.corner_normals.is_empty()) {
      normal_float_to_short_v3(&(*(gpuHQNor **)data)[l_index].x, mr.corner_normals[l_index]);
    }
    else {
      if (BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
        normal_float_to_short_v3(&(*(gpuHQNor **)data)[l_index].x, bm_vert_no_get(mr, l_iter->v));
      }
      else {
        normal_float_to_short_v3(&(*(gpuHQNor **)data)[l_index].x, bm_face_no_get(mr, f));
      }
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_lnor_hq_iter_face_mesh(const MeshRenderData &mr,
                                           const int face_index,
                                           void *data)
{
  const bool hidden = !mr.hide_poly.is_empty() && mr.hide_poly[face_index];

  for (const int corner : mr.faces[face_index]) {
    const int vert = mr.corner_verts[corner];
    gpuHQNor *lnor_data = &(*(gpuHQNor **)data)[corner];
    if (!mr.corner_normals.is_empty()) {
      normal_float_to_short_v3(&lnor_data->x, mr.corner_normals[corner]);
    }
    else if (mr.normals_domain == bke::MeshNormalDomain::Face ||
             (!mr.sharp_faces.is_empty() && mr.sharp_faces[face_index]))
    {
      normal_float_to_short_v3(&lnor_data->x, mr.face_normals[face_index]);
    }
    else {
      normal_float_to_short_v3(&lnor_data->x, mr.vert_normals[vert]);
    }

    /* Flag for paint mode overlay.
     * Only use origindex in edit mode where it is used to display the edge-normals.
     * In paint mode it will use the un-mapped data to draw the wire-frame. */
    if (hidden || (mr.edit_bmesh && (mr.v_origindex) && mr.v_origindex[vert] == ORIGINDEX_NONE)) {
      lnor_data->w = -1;
    }
    else if (!mr.select_poly.is_empty() && mr.select_poly[face_index]) {
      lnor_data->w = 1;
    }
    else {
      lnor_data->w = 0;
    }
  }
}

constexpr MeshExtract create_extractor_lnor_hq()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lnor_hq_init;
  extractor.init_subdiv = extract_lnor_init_subdiv;
  extractor.iter_face_bm = extract_lnor_hq_iter_face_bm;
  extractor.iter_face_mesh = extract_lnor_hq_iter_face_mesh;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = sizeof(gpuHQNor *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.lnor);
  return extractor;
}

/** \} */

const MeshExtract extract_lnor = create_extractor_lnor();
const MeshExtract extract_lnor_hq = create_extractor_lnor_hq();

}  // namespace blender::draw
