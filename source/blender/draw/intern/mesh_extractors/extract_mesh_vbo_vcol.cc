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

#include "draw_subdivision.h"
#include "extract_mesh.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract VCol
 * \{ */

/* Initialize the common vertex format for vcol for coarse and subdivided meshes. */
static void init_vcol_format(GPUVertFormat *format,
                             const MeshBatchCache *cache,
                             CustomData *cd_ldata)
{
  GPU_vertformat_deinterleave(format);

  const uint32_t vcol_layers = cache->cd_used.vcol;

  for (int i = 0; i < MAX_MCOL; i++) {
    if (vcol_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

      BLI_snprintf(attr_name, sizeof(attr_name), "c%s", attr_safe_name);
      GPU_vertformat_attr_add(format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPCOL)) {
        GPU_vertformat_alias_add(format, "c");
      }
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL)) {
        GPU_vertformat_alias_add(format, "ac");
      }

      /* Gather number of auto layers. */
      /* We only do `vcols` that are not overridden by `uvs`. */
      if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, layer_name) == -1) {
        BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
        GPU_vertformat_alias_add(format, attr_name);
      }
    }
  }
}

/* Vertex format for vertex colors, only used during the coarse data upload for the subdivision
 * case. */
static GPUVertFormat *get_coarse_vcol_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "cCol", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "c");
    GPU_vertformat_alias_add(&format, "ac");
  }
  return &format;
}

using gpuMeshVcol = struct gpuMeshVcol {
  ushort r, g, b, a;
};

static void extract_vcol_init(const MeshRenderData *mr,
                              struct MeshBatchCache *cache,
                              void *buf,
                              void *UNUSED(tls_data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  GPUVertFormat format = {0};
  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  const uint32_t vcol_layers = cache->cd_used.vcol;
  init_vcol_format(&format, cache, cd_ldata);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  gpuMeshVcol *vcol_data = (gpuMeshVcol *)GPU_vertbuf_get_data(vbo);

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
  }
}

static void extract_vcol_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                     const MeshRenderData *UNUSED(mr),
                                     struct MeshBatchCache *cache,
                                     void *buffer,
                                     void *UNUSED(data))
{
  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  Mesh *coarse_mesh = subdiv_cache->mesh;

  GPUVertFormat format = {0};
  init_vcol_format(&format, cache, &coarse_mesh->ldata);

  GPU_vertbuf_init_build_on_device(dst_buffer, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *src_data = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(src_data, get_coarse_vcol_format(), GPU_USAGE_DYNAMIC);

  GPU_vertbuf_data_alloc(src_data, coarse_mesh->totloop);

  gpuMeshVcol *mesh_vcol = (gpuMeshVcol *)GPU_vertbuf_get_data(src_data);

  const CustomData *cd_ldata = &coarse_mesh->ldata;

  const uint vcol_layers = cache->cd_used.vcol;

  /* Index of the vertex color layer in the compact buffer. Used vertex color layers are stored in
   * a single buffer. */
  int pack_layer_index = 0;
  for (int i = 0; i < MAX_MTFACE; i++) {
    if (vcol_layers & (1 << i)) {
      /* Include stride in offset, we use a stride of 2 since colors are packed into 2 uints. */
      const int dst_offset = (int)subdiv_cache->num_subdiv_loops * 2 * pack_layer_index++;
      const MLoopCol *mloopcol = (MLoopCol *)CustomData_get_layer_n(cd_ldata, CD_MLOOPCOL, i);

      gpuMeshVcol *vcol = mesh_vcol;

      for (int ml_index = 0; ml_index < coarse_mesh->totloop; ml_index++, vcol++, mloopcol++) {
        vcol->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
        vcol->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
        vcol->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
        vcol->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
      }

      /* Ensure data is uploaded properly. */
      GPU_vertbuf_tag_dirty(src_data);
      draw_subdiv_interp_custom_data(subdiv_cache, src_data, dst_buffer, 4, dst_offset);
    }
  }

  GPU_vertbuf_discard(src_data);
}

constexpr MeshExtract create_extractor_vcol()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_vcol_init;
  extractor.init_subdiv = extract_vcol_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.vcol);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_vcol = blender::draw::create_extractor_vcol();
}
