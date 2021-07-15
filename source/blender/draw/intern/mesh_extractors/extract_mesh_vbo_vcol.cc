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

#include "draw_cache_extract_mesh_private.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract VCol
 * \{ */

static void extract_vcol_init(const MeshRenderData *mr,
                              struct MeshBatchCache *cache,
                              void *buf,
                              void *UNUSED(tls_data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
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

  using gpuMeshVcol = struct gpuMeshVcol {
    ushort r, g, b, a;
  };

  gpuMeshVcol *vcol_data = (gpuMeshVcol *)GPU_vertbuf_get_data(vbo);
  MLoop *loops = (MLoop *)CustomData_get_layer(cd_ldata, CD_MLOOP);

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
            const MLoopCol *mloopcol = (const MLoopCol *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_ofs);
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
            const MPropCol *prop_col = (const MPropCol *)BM_ELEM_CD_GET_VOID_P(l_iter->v, cd_ofs);
            vcol_data->r = unit_float_to_ushort_clamp(prop_col->color[0]);
            vcol_data->g = unit_float_to_ushort_clamp(prop_col->color[1]);
            vcol_data->b = unit_float_to_ushort_clamp(prop_col->color[2]);
            vcol_data->a = unit_float_to_ushort_clamp(prop_col->color[3]);
            vcol_data++;
          } while ((l_iter = l_iter->next) != l_first);
        }
      }
      else {
        MPropCol *vcol = (MPropCol *)CustomData_get_layer_n(cd_vdata, CD_PROP_COLOR, i);
        for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vcol_data++) {
          vcol_data->r = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[0]);
          vcol_data->g = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[1]);
          vcol_data->b = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[2]);
          vcol_data->a = unit_float_to_ushort_clamp(vcol[loops[ml_index].v].color[3]);
        }
      }
    }
  }
}

constexpr MeshExtract create_extractor_vcol()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_vcol_init;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, vbo.vcol);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_vcol = blender::draw::create_extractor_vcol();
}
