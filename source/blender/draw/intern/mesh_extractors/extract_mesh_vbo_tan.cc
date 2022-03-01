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

#include "BLI_string.h"

#include "BKE_editmesh.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h"

#include "extract_mesh.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Tangent layers
 * \{ */

static void extract_tan_init_common(const MeshRenderData *mr,
                                    struct MeshBatchCache *cache,
                                    GPUVertFormat *format,
                                    GPUVertCompType comp_type,
                                    GPUVertFetchMode fetch_mode,
                                    CustomData *r_loop_data,
                                    int *r_v_len,
                                    int *r_tan_len,
                                    char r_tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME],
                                    bool *r_use_orco_tan)
{
  GPU_vertformat_deinterleave(format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  uint32_t tan_layers = cache->cd_used.tan;
  float(*orco)[3] = (float(*)[3])CustomData_get_layer(cd_vdata, CD_ORCO);
  bool orco_allocated = false;
  bool use_orco_tan = cache->cd_used.tan_orco != 0;

  int tan_len = 0;

  /* FIXME(T91838): This is to avoid a crash when orco tangent was requested but there are valid
   * uv layers. It would be better to fix the root cause. */
  if (tan_layers == 0 && use_orco_tan && CustomData_get_layer_index(cd_ldata, CD_MLOOPUV) != -1) {
    tan_layers = 1;
    use_orco_tan = false;
  }

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (tan_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* Tangent layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "t%s", attr_safe_name);
      GPU_vertformat_attr_add(format, attr_name, comp_type, 4, fetch_mode);
      /* Active render layer name. */
      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(format, "t");
      }
      /* Active display layer name. */
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(format, "at");
      }

      BLI_strncpy(r_tangent_names[tan_len++], layer_name, MAX_CUSTOMDATA_LAYER_NAME);
    }
  }
  if (use_orco_tan && orco == nullptr) {
    /* If `orco` is not available compute it ourselves */
    orco_allocated = true;
    orco = (float(*)[3])MEM_mallocN(sizeof(*orco) * mr->vert_len, __func__);

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
  CustomData_reset(r_loop_data);
  if (tan_len != 0 || use_orco_tan) {
    short tangent_mask = 0;
    bool calc_active_tangent = false;
    if (mr->extract_type == MR_EXTRACT_BMESH) {
      BKE_editmesh_loop_tangent_calc(mr->edit_bmesh,
                                     calc_active_tangent,
                                     r_tangent_names,
                                     tan_len,
                                     mr->poly_normals,
                                     mr->loop_normals,
                                     orco,
                                     r_loop_data,
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
                                    r_tangent_names,
                                    tan_len,
                                    mr->vert_normals,
                                    mr->poly_normals,
                                    mr->loop_normals,
                                    orco,
                                    r_loop_data,
                                    mr->loop_len,
                                    &tangent_mask);
    }
  }

  if (use_orco_tan) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
    const char *layer_name = CustomData_get_layer_name(r_loop_data, CD_TANGENT, 0);
    GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
    BLI_snprintf(attr_name, sizeof(*attr_name), "t%s", attr_safe_name);
    GPU_vertformat_attr_add(format, attr_name, comp_type, 4, fetch_mode);
    GPU_vertformat_alias_add(format, "t");
    GPU_vertformat_alias_add(format, "at");
  }

  if (orco_allocated) {
    MEM_SAFE_FREE(orco);
  }

  int v_len = mr->loop_len;
  if (format->attr_len == 0) {
    GPU_vertformat_attr_add(format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  *r_use_orco_tan = use_orco_tan;
  *r_v_len = v_len;
  *r_tan_len = tan_len;
}

static void extract_tan_ex_init(const MeshRenderData *mr,
                                struct MeshBatchCache *cache,
                                GPUVertBuf *vbo,
                                const bool do_hq)
{
  GPUVertCompType comp_type = do_hq ? GPU_COMP_I16 : GPU_COMP_I10;
  GPUVertFetchMode fetch_mode = GPU_FETCH_INT_TO_FLOAT_UNIT;

  GPUVertFormat format = {0};
  CustomData loop_data;
  int v_len = 0;
  int tan_len = 0;
  bool use_orco_tan;
  char tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME];
  extract_tan_init_common(mr,
                          cache,
                          &format,
                          comp_type,
                          fetch_mode,
                          &loop_data,
                          &v_len,
                          &tan_len,
                          tangent_names,
                          &use_orco_tan);

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

static void extract_tan_init(const MeshRenderData *mr,
                             struct MeshBatchCache *cache,
                             void *buf,
                             void *UNUSED(tls_data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  extract_tan_ex_init(mr, cache, vbo, false);
}

static GPUVertFormat *get_coarse_tan_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "tan", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

static void extract_tan_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                    const MeshRenderData *mr,
                                    struct MeshBatchCache *cache,
                                    void *buffer,
                                    void *UNUSED(data))
{
  GPUVertCompType comp_type = GPU_COMP_F32;
  GPUVertFetchMode fetch_mode = GPU_FETCH_FLOAT;
  GPUVertFormat format = {0};
  CustomData loop_data;
  int coarse_len = 0;
  int tan_len = 0;
  bool use_orco_tan;
  char tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME];
  extract_tan_init_common(mr,
                          cache,
                          &format,
                          comp_type,
                          fetch_mode,
                          &loop_data,
                          &coarse_len,
                          &tan_len,
                          tangent_names,
                          &use_orco_tan);

  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  GPU_vertbuf_init_build_on_device(dst_buffer, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *coarse_vbo = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(coarse_vbo, get_coarse_tan_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(coarse_vbo, coarse_len);

  /* Index of the tangent layer in the compact buffer. Used layers are stored in a single buffer.
   */
  int pack_layer_index = 0;
  for (int i = 0; i < tan_len; i++) {
    float(*tan_data)[4] = (float(*)[4])GPU_vertbuf_get_data(coarse_vbo);
    const char *name = tangent_names[i];
    float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_named(&loop_data, CD_TANGENT, name);
    for (int ml_index = 0; ml_index < mr->loop_len; ml_index++) {
      copy_v3_v3(*tan_data, layer_data[ml_index]);
      (*tan_data)[3] = (layer_data[ml_index][3] > 0.0f) ? 1.0f : -1.0f;
      tan_data++;
    }

    /* Ensure data is uploaded properly. */
    GPU_vertbuf_tag_dirty(coarse_vbo);
    /* Include stride in offset. */
    const int dst_offset = (int)subdiv_cache->num_subdiv_loops * 4 * pack_layer_index++;
    draw_subdiv_interp_custom_data(subdiv_cache, coarse_vbo, dst_buffer, 4, dst_offset, false);
  }
  if (use_orco_tan) {
    float(*tan_data)[4] = (float(*)[4])GPU_vertbuf_get_data(coarse_vbo);
    float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_n(&loop_data, CD_TANGENT, 0);
    for (int ml_index = 0; ml_index < mr->loop_len; ml_index++) {
      copy_v3_v3(*tan_data, layer_data[ml_index]);
      (*tan_data)[3] = (layer_data[ml_index][3] > 0.0f) ? 1.0f : -1.0f;
      tan_data++;
    }

    /* Ensure data is uploaded properly. */
    GPU_vertbuf_tag_dirty(coarse_vbo);
    /* Include stride in offset. */
    const int dst_offset = (int)subdiv_cache->num_subdiv_loops * 4 * pack_layer_index++;
    draw_subdiv_interp_custom_data(subdiv_cache, coarse_vbo, dst_buffer, 4, dst_offset, true);
  }

  CustomData_free(&loop_data, mr->loop_len);
  GPU_vertbuf_discard(coarse_vbo);
}

constexpr MeshExtract create_extractor_tan()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tan_init;
  extractor.init_subdiv = extract_tan_init_subdiv;
  extractor.data_type = MR_DATA_POLY_NOR | MR_DATA_TAN_LOOP_NOR | MR_DATA_LOOPTRI;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.tan);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract HQ Tangent layers
 * \{ */

static void extract_tan_hq_init(const MeshRenderData *mr,
                                struct MeshBatchCache *cache,
                                void *buf,
                                void *UNUSED(tls_data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  extract_tan_ex_init(mr, cache, vbo, true);
}

constexpr MeshExtract create_extractor_tan_hq()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tan_hq_init;
  extractor.data_type = MR_DATA_POLY_NOR | MR_DATA_TAN_LOOP_NOR | MR_DATA_LOOPTRI;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.tan);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_tan = blender::draw::create_extractor_tan();
const MeshExtract extract_tan_hq = blender::draw::create_extractor_tan_hq();
}
