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

#include "extract_mesh.h"

#include "draw_cache_impl.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edit Mode Data / Flags
 * \{ */

static void mesh_render_data_edge_flag(const MeshRenderData *mr,
                                       const BMEdge *eed,
                                       EditLoopData *eattr)
{
  const ToolSettings *ts = mr->toolsettings;
  const bool is_vertex_select_mode = (ts != nullptr) && (ts->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool is_face_only_select_mode = (ts != nullptr) && (ts->selectmode == SCE_SELECT_FACE);

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
    if (mr->efa_act != nullptr) {
      if (BM_edge_in_face(eed, mr->efa_act)) {
        eattr->e_flag |= VFLAG_EDGE_ACTIVE;
      }
    }
  }

  /* Use half a byte for value range */
  if (mr->edge_crease_ofs != -1) {
    float crease = BM_ELEM_CD_GET_FLOAT(eed, mr->edge_crease_ofs);
    if (crease > 0) {
      eattr->crease = (uchar)ceil(crease * 15.0f);
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
    const FreestyleEdge *fed = (const FreestyleEdge *)BM_ELEM_CD_GET_VOID_P(
        eed, mr->freestyle_edge_ofs);
    if (fed->flag & FREESTYLE_EDGE_MARK) {
      eattr->e_flag |= VFLAG_EDGE_FREESTYLE;
    }
  }
#endif
}

static void mesh_render_data_vert_flag(const MeshRenderData *mr,
                                       const BMVert *eve,
                                       EditLoopData *eattr)
{
  if (eve == mr->eve_act) {
    eattr->e_flag |= VFLAG_VERT_ACTIVE;
  }
  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
  /* Use half a byte for value range */
  if (mr->vert_crease_ofs != -1) {
    float crease = BM_ELEM_CD_GET_FLOAT(eve, mr->vert_crease_ofs);
    if (crease > 0) {
      eattr->crease |= (uchar)ceil(crease * 15.0f) << 4;
    }
  }
}

static GPUVertFormat *get_edit_data_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING: Adjust #EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }
  return &format;
}

static void extract_edit_data_init(const MeshRenderData *mr,
                                   struct MeshBatchCache *UNUSED(cache),
                                   void *buf,
                                   void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  GPUVertFormat *format = get_edit_data_format();
  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);
  EditLoopData *vbo_data = (EditLoopData *)GPU_vertbuf_get_data(vbo);
  *(EditLoopData **)tls_data = vbo_data;
}

static void extract_edit_data_iter_poly_bm(const MeshRenderData *mr,
                                           const BMFace *f,
                                           const int UNUSED(f_index),
                                           void *_data)
{
  EditLoopData *vbo_data = *(EditLoopData **)_data;

  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    EditLoopData *data = vbo_data + l_index;
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
  EditLoopData *vbo_data = *(EditLoopData **)_data;

  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    EditLoopData *data = vbo_data + ml_index;
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
                                            const BMEdge *eed,
                                            const int ledge_index,
                                            void *_data)
{
  EditLoopData *vbo_data = *(EditLoopData **)_data;
  EditLoopData *data = vbo_data + mr->loop_len + (ledge_index * 2);
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
  EditLoopData *vbo_data = *(EditLoopData **)_data;
  EditLoopData *data = vbo_data + mr->loop_len + ledge_index * 2;
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
                                            const BMVert *eve,
                                            const int lvert_index,
                                            void *_data)
{
  EditLoopData *vbo_data = *(EditLoopData **)_data;
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);
  EditLoopData *data = vbo_data + offset + lvert_index;
  memset(data, 0x0, sizeof(*data));
  mesh_render_data_vert_flag(mr, eve, data);
}

static void extract_edit_data_iter_lvert_mesh(const MeshRenderData *mr,
                                              const MVert *UNUSED(mv),
                                              const int lvert_index,
                                              void *_data)
{
  EditLoopData *vbo_data = *(EditLoopData **)_data;
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  EditLoopData *data = vbo_data + offset + lvert_index;
  memset(data, 0x0, sizeof(*data));
  const int v_index = mr->lverts[lvert_index];
  BMVert *eve = bm_original_vert_get(mr, v_index);
  if (eve) {
    mesh_render_data_vert_flag(mr, eve, data);
  }
}

static void extract_edit_data_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                          const MeshRenderData *mr,
                                          MeshBatchCache *UNUSED(cache),
                                          void *buf,
                                          void *data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  GPU_vertbuf_init_with_format(vbo, get_edit_data_format());
  GPU_vertbuf_data_alloc(vbo, subdiv_cache->num_subdiv_loops + mr->loop_loose_len);
  EditLoopData *vbo_data = (EditLoopData *)GPU_vertbuf_get_data(vbo);
  *(EditLoopData **)data = vbo_data;
}

static void extract_edit_data_iter_subdiv_bm(const DRWSubdivCache *subdiv_cache,
                                             const MeshRenderData *mr,
                                             void *_data,
                                             uint subdiv_quad_index,
                                             const BMFace *coarse_quad)
{
  EditLoopData *vbo_data = *(EditLoopData **)_data;
  int *subdiv_loop_vert_index = (int *)GPU_vertbuf_get_data(subdiv_cache->verts_orig_index);
  int *subdiv_loop_edge_index = (int *)GPU_vertbuf_get_data(subdiv_cache->edges_orig_index);

  uint start_loop_idx = subdiv_quad_index * 4;
  uint end_loop_idx = (subdiv_quad_index + 1) * 4;
  for (uint i = start_loop_idx; i < end_loop_idx; i++) {
    const int vert_origindex = subdiv_loop_vert_index[i];
    const int edge_origindex = subdiv_loop_edge_index[i];

    EditLoopData *edit_loop_data = &vbo_data[i];
    memset(edit_loop_data, 0, sizeof(EditLoopData));

    if (vert_origindex != -1) {
      const BMVert *eve = mr->v_origindex ? bm_original_vert_get(mr, vert_origindex) :
                                            BM_vert_at_index(mr->bm, vert_origindex);
      if (eve) {
        mesh_render_data_vert_flag(mr, eve, edit_loop_data);
      }
    }

    if (edge_origindex != -1) {
      /* NOTE: #subdiv_loop_edge_index already has the origindex layer baked in. */
      const BMEdge *eed = BM_edge_at_index(mr->bm, edge_origindex);
      mesh_render_data_edge_flag(mr, eed, edit_loop_data);
    }

    /* coarse_quad can be null when called by the mesh iteration below. */
    if (coarse_quad) {
      /* The -1 parameter is for edit_uvs, which we don't do here. */
      mesh_render_data_face_flag(mr, coarse_quad, -1, edit_loop_data);
    }
  }
}

static void extract_edit_data_iter_subdiv_mesh(const DRWSubdivCache *subdiv_cache,
                                               const MeshRenderData *mr,
                                               void *_data,
                                               uint subdiv_quad_index,
                                               const MPoly *coarse_quad)
{
  const int coarse_quad_index = static_cast<int>(coarse_quad - mr->mpoly);
  BMFace *coarse_quad_bm = bm_original_face_get(mr, coarse_quad_index);
  extract_edit_data_iter_subdiv_bm(subdiv_cache, mr, _data, subdiv_quad_index, coarse_quad_bm);
}

static void extract_edit_data_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                                const MeshRenderData *mr,
                                                const MeshExtractLooseGeom *loose_geom,
                                                void *UNUSED(buffer),
                                                void *_data)
{
  if (loose_geom->edge_len == 0) {
    return;
  }

  EditLoopData *vbo_data = *(EditLoopData **)_data;

  for (int ledge_index = 0; ledge_index < loose_geom->edge_len; ledge_index++) {
    const int offset = subdiv_cache->num_subdiv_loops + ledge_index * 2;
    EditLoopData *data = &vbo_data[offset];
    memset(data, 0, sizeof(EditLoopData));
    const int edge_index = loose_geom->edges[ledge_index];
    BMEdge *eed = mr->e_origindex ? bm_original_edge_get(mr, edge_index) :
                                    BM_edge_at_index(mr->bm, edge_index);
    mesh_render_data_edge_flag(mr, eed, &data[0]);
    data[1] = data[0];
    mesh_render_data_vert_flag(mr, eed->v1, &data[0]);
    mesh_render_data_vert_flag(mr, eed->v2, &data[1]);
  }
}

constexpr MeshExtract create_extractor_edit_data()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_edit_data_init;
  extractor.iter_poly_bm = extract_edit_data_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edit_data_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_edit_data_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_edit_data_iter_ledge_mesh;
  extractor.iter_lvert_bm = extract_edit_data_iter_lvert_bm;
  extractor.iter_lvert_mesh = extract_edit_data_iter_lvert_mesh;
  extractor.init_subdiv = extract_edit_data_init_subdiv;
  extractor.iter_subdiv_bm = extract_edit_data_iter_subdiv_bm;
  extractor.iter_subdiv_mesh = extract_edit_data_iter_subdiv_mesh;
  extractor.iter_loose_geom_subdiv = extract_edit_data_loose_geom_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(EditLoopData *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edit_data);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_edit_data = blender::draw::create_extractor_edit_data();
}
