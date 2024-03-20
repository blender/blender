/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract UV  layers
 * \{ */

/* Initialize the vertex format to be used for UVs. Return true if any UV layer is
 * found, false otherwise. */
static bool mesh_extract_uv_format_init(GPUVertFormat *format,
                                        MeshBatchCache &cache,
                                        CustomData *cd_ldata,
                                        eMRExtractType extract_type,
                                        uint32_t &r_uv_layers)
{
  GPU_vertformat_deinterleave(format);

  uint32_t uv_layers = cache.cd_used.uv;
  /* HACK to fix #68857 */
  if (extract_type == MR_EXTRACT_BMESH && cache.cd_used.edit_uv == 1) {
    int layer = CustomData_get_active_layer(cd_ldata, CD_PROP_FLOAT2);
    if (layer != -1 && !CustomData_layer_is_anonymous(cd_ldata, CD_PROP_FLOAT2, layer)) {
      uv_layers |= (1 << layer);
    }
  }

  r_uv_layers = 0;

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_PROP_FLOAT2, i);

      /* not all UV layers are guaranteed to exist, since the list of available UV
       * layers is generated for the evaluated mesh, which is needed to show modifier
       * results in editmode, but the actual mesh might be the base mesh.
       */
      if (layer_name) {
        r_uv_layers |= (1 << i);
        GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
        /* UV layer name. */
        SNPRINTF(attr_name, "a%s", attr_safe_name);
        GPU_vertformat_attr_add(format, attr_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        /* Active render layer name. */
        if (i == CustomData_get_render_layer(cd_ldata, CD_PROP_FLOAT2)) {
          GPU_vertformat_alias_add(format, "a");
        }
        /* Active display layer name. */
        if (i == CustomData_get_active_layer(cd_ldata, CD_PROP_FLOAT2)) {
          GPU_vertformat_alias_add(format, "au");
          /* Alias to `pos` for edit uvs. */
          GPU_vertformat_alias_add(format, "pos");
        }
        /* Stencil mask uv layer name. */
        if (i == CustomData_get_stencil_layer(cd_ldata, CD_PROP_FLOAT2)) {
          GPU_vertformat_alias_add(format, "mu");
        }
      }
    }
  }

  if (format->attr_len == 0) {
    GPU_vertformat_attr_add(format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    return false;
  }

  return true;
}

static void extract_uv_init(const MeshRenderData &mr,
                            MeshBatchCache &cache,
                            void *buf,
                            void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  GPUVertFormat format = {0};

  CustomData *cd_ldata = (mr.extract_type == MR_EXTRACT_BMESH) ? &mr.bm->ldata :
                                                                 &mr.mesh->corner_data;
  int v_len = mr.corners_num;
  uint32_t uv_layers = cache.cd_used.uv;
  if (!mesh_extract_uv_format_init(&format, cache, cd_ldata, mr.extract_type, uv_layers)) {
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, v_len);

  MutableSpan<float2> uv_data(static_cast<float2 *>(GPU_vertbuf_get_data(vbo)),
                              v_len * format.attr_len);
  int vbo_index = 0;
  for (const int i : IndexRange(MAX_MTFACE)) {
    if (uv_layers & (1 << i)) {
      if (mr.extract_type == MR_EXTRACT_BMESH) {
        int cd_ofs = CustomData_get_n_offset(cd_ldata, CD_PROP_FLOAT2, i);
        BMIter f_iter;
        BMFace *efa;
        BM_ITER_MESH (efa, &f_iter, mr.bm, BM_FACES_OF_MESH) {
          BMLoop *l_iter, *l_first;
          l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
          do {
            uv_data[vbo_index] = BM_ELEM_CD_GET_FLOAT_P(l_iter, cd_ofs);
            vbo_index++;
          } while ((l_iter = l_iter->next) != l_first);
        }
      }
      else {
        const Span<float2> uv_map(
            static_cast<const float2 *>(CustomData_get_layer_n(cd_ldata, CD_PROP_FLOAT2, i)),
            mr.corners_num);
        array_utils::copy(uv_map, uv_data.slice(vbo_index, mr.corners_num));
        vbo_index += mr.corners_num;
      }
    }
  }
}

static void extract_uv_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                   const MeshRenderData & /*mr*/,
                                   MeshBatchCache &cache,
                                   void *buffer,
                                   void * /*data*/)
{
  Mesh *coarse_mesh = subdiv_cache.mesh;
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  GPUVertFormat format = {0};

  uint v_len = subdiv_cache.num_subdiv_loops;
  uint uv_layers;
  if (!mesh_extract_uv_format_init(
          &format, cache, &coarse_mesh->corner_data, MR_EXTRACT_MESH, uv_layers))
  {
    /* TODO(kevindietrich): handle this more gracefully. */
    v_len = 1;
  }

  GPU_vertbuf_init_build_on_device(vbo, &format, v_len);

  if (uv_layers == 0) {
    return;
  }

  /* Index of the UV layer in the compact buffer. Used UV layers are stored in a single buffer. */
  int pack_layer_index = 0;
  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      const int offset = int(subdiv_cache.num_subdiv_loops) * pack_layer_index++;
      draw_subdiv_extract_uvs(subdiv_cache, vbo, i, offset);
    }
  }
}

constexpr MeshExtract create_extractor_uv()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_uv_init;
  extractor.init_subdiv = extract_uv_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.uv);
  return extractor;
}

/** \} */

const MeshExtract extract_uv = create_extractor_uv();

}  // namespace blender::draw
