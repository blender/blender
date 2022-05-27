/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BKE_attribute.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "draw_subdivision.h"
#include "extract_mesh.h"

struct VColRef {
  const CustomDataLayer *layer;
  AttributeDomain domain;
};

/** Get all vcol layers as AttributeRefs.
 *
 * \param vcol_layers: bitmask to filter vcol layers by, each bit
 *                     corresponds to the integer position of the attribute
 *                     within the global color attribute list.
 */
static blender::Vector<VColRef> get_vcol_refs(const CustomData *cd_vdata,
                                              const CustomData *cd_ldata,
                                              const uint vcol_layers)
{
  blender::Vector<VColRef> refs;
  uint layeri = 0;

  auto buildList = [&](const CustomData *cdata, AttributeDomain domain) {
    for (int i = 0; i < cdata->totlayer; i++) {
      const CustomDataLayer *layer = cdata->layers + i;

      if (!(CD_TYPE_AS_MASK(layer->type) & CD_MASK_COLOR_ALL)) {
        continue;
      }

      if (layer->flag & CD_FLAG_TEMPORARY) {
        continue;
      }

      if (!(vcol_layers & (1UL << layeri))) {
        layeri++;
        continue;
      }

      VColRef ref = {};
      ref.domain = domain;
      ref.layer = layer;

      refs.append(ref);
      layeri++;
    }
  };

  buildList(cd_vdata, ATTR_DOMAIN_POINT);
  buildList(cd_ldata, ATTR_DOMAIN_CORNER);

  return refs;
}

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract VCol
 * \{ */

/* Initialize the common vertex format for vcol for coarse and subdivided meshes. */
static void init_vcol_format(GPUVertFormat *format,
                             const MeshBatchCache *cache,
                             CustomData *cd_vdata,
                             CustomData *cd_ldata,
                             CustomDataLayer *active,
                             CustomDataLayer *render)
{
  GPU_vertformat_deinterleave(format);

  const uint32_t vcol_layers = cache->cd_used.vcol;

  blender::Vector<VColRef> refs = get_vcol_refs(cd_vdata, cd_ldata, vcol_layers);

  for (const VColRef &ref : refs) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];

    GPU_vertformat_safe_attr_name(ref.layer->name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

    /* VCol layer name. */
    BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
    GPU_vertformat_attr_add(format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

    /* Active layer name. */
    if (ref.layer == active) {
      GPU_vertformat_alias_add(format, "ac");
    }

    /* Active render layer name. */
    if (ref.layer == render) {
      GPU_vertformat_alias_add(format, "c");
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

  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;

  Mesh me_query = blender::dna::shallow_zero_initialize();

  BKE_id_attribute_copy_domains_temp(
      ID_ME, cd_vdata, nullptr, cd_ldata, nullptr, nullptr, &me_query.id);

  CustomDataLayer *active_color = BKE_id_attributes_active_color_get(&me_query.id);
  CustomDataLayer *render_color = BKE_id_attributes_render_color_get(&me_query.id);

  const uint32_t vcol_layers = cache->cd_used.vcol;
  init_vcol_format(&format, cache, cd_vdata, cd_ldata, active_color, render_color);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  gpuMeshVcol *vcol_data = (gpuMeshVcol *)GPU_vertbuf_get_data(vbo);

  blender::Vector<VColRef> refs = get_vcol_refs(cd_vdata, cd_ldata, vcol_layers);

  for (const VColRef &ref : refs) {
    CustomData *cdata = ref.domain == ATTR_DOMAIN_POINT ? cd_vdata : cd_ldata;

    if (mr->extract_type == MR_EXTRACT_BMESH) {
      int cd_ofs = ref.layer->offset;

      if (cd_ofs == -1) {
        vcol_data += ref.domain == ATTR_DOMAIN_POINT ? mr->bm->totvert : mr->bm->totloop;
        continue;
      }

      BMIter iter;
      const bool is_byte = ref.layer->type == CD_PROP_BYTE_COLOR;
      const bool is_point = ref.domain == ATTR_DOMAIN_POINT;

      BMFace *f;
      BM_ITER_MESH (f, &iter, mr->bm, BM_FACES_OF_MESH) {
        BMLoop *l_iter = f->l_first;
        do {
          BMElem *elem = is_point ? reinterpret_cast<BMElem *>(l_iter->v) :
                                    reinterpret_cast<BMElem *>(l_iter);
          if (is_byte) {
            const MLoopCol *mloopcol = (const MLoopCol *)BM_ELEM_CD_GET_VOID_P(elem, cd_ofs);
            vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
            vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
            vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
            vcol_data->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
            vcol_data++;
          }
          else {
            const MPropCol *mpcol = (const MPropCol *)BM_ELEM_CD_GET_VOID_P(elem, cd_ofs);
            vcol_data->r = unit_float_to_ushort_clamp(mpcol->color[0]);
            vcol_data->g = unit_float_to_ushort_clamp(mpcol->color[1]);
            vcol_data->b = unit_float_to_ushort_clamp(mpcol->color[2]);
            vcol_data->a = unit_float_to_ushort_clamp(mpcol->color[3]);
            vcol_data++;
          }
        } while ((l_iter = l_iter->next) != f->l_first);
      }
    }
    else {
      int totloop = mr->loop_len;
      int idx = CustomData_get_named_layer_index(cdata, ref.layer->type, ref.layer->name);

      MLoopCol *mcol = nullptr;
      MPropCol *pcol = nullptr;
      const MLoop *mloop = mr->mloop;

      if (ref.layer->type == CD_PROP_COLOR) {
        pcol = static_cast<MPropCol *>(cdata->layers[idx].data);
      }
      else {
        mcol = static_cast<MLoopCol *>(cdata->layers[idx].data);
      }

      const bool is_corner = ref.domain == ATTR_DOMAIN_CORNER;

      for (int i = 0; i < totloop; i++, mloop++) {
        const int v_i = is_corner ? i : mloop->v;

        if (mcol) {
          vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol[v_i].r]);
          vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol[v_i].g]);
          vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol[v_i].b]);
          vcol_data->a = unit_float_to_ushort_clamp(mcol[v_i].a * (1.0f / 255.0f));
          vcol_data++;
        }
        else if (pcol) {
          vcol_data->r = unit_float_to_ushort_clamp(pcol[v_i].color[0]);
          vcol_data->g = unit_float_to_ushort_clamp(pcol[v_i].color[1]);
          vcol_data->b = unit_float_to_ushort_clamp(pcol[v_i].color[2]);
          vcol_data->a = unit_float_to_ushort_clamp(pcol[v_i].color[3]);
          vcol_data++;
        }
      }
    }
  }
}

static void extract_vcol_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                     const MeshRenderData *mr,
                                     struct MeshBatchCache *cache,
                                     void *buffer,
                                     void *UNUSED(data))
{
  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  Mesh *coarse_mesh = subdiv_cache->mesh;

  bool extract_bmesh = mr->extract_type == MR_EXTRACT_BMESH;

  const CustomData *cd_vdata = extract_bmesh ? &coarse_mesh->edit_mesh->bm->vdata :
                                               &coarse_mesh->vdata;
  const CustomData *cd_ldata = extract_bmesh ? &coarse_mesh->edit_mesh->bm->ldata :
                                               &coarse_mesh->ldata;

  Mesh me_query = blender::dna::shallow_copy(*coarse_mesh);
  BKE_id_attribute_copy_domains_temp(
      ID_ME, cd_vdata, nullptr, cd_ldata, nullptr, nullptr, &me_query.id);

  CustomDataLayer *active_color = BKE_id_attributes_active_color_get(&me_query.id);
  CustomDataLayer *render_color = BKE_id_attributes_render_color_get(&me_query.id);

  GPUVertFormat format = {0};
  init_vcol_format(
      &format, cache, &coarse_mesh->vdata, &coarse_mesh->ldata, active_color, render_color);

  GPU_vertbuf_init_build_on_device(dst_buffer, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *src_data = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(src_data, get_coarse_vcol_format(), GPU_USAGE_DYNAMIC);

  GPU_vertbuf_data_alloc(src_data, coarse_mesh->totloop);

  gpuMeshVcol *mesh_vcol = (gpuMeshVcol *)GPU_vertbuf_get_data(src_data);

  const uint vcol_layers = cache->cd_used.vcol;

  blender::Vector<VColRef> refs = get_vcol_refs(cd_vdata, cd_ldata, vcol_layers);

  /* Index of the vertex color layer in the compact buffer. Used vertex color layers are stored in
   * a single buffer. */
  int pack_layer_index = 0;
  for (const VColRef &ref : refs) {
    /* Include stride in offset, we use a stride of 2 since colors are packed into 2 uints. */
    const int dst_offset = (int)subdiv_cache->num_subdiv_loops * 2 * pack_layer_index++;

    const CustomData *cdata = ref.domain == ATTR_DOMAIN_POINT ? cd_vdata : cd_ldata;
    const MLoop *ml = coarse_mesh->mloop;

    int layer_i = CustomData_get_named_layer_index(cdata, ref.layer->type, ref.layer->name);

    if (layer_i == -1) {
      printf("%s: missing color layer %s\n", __func__, ref.layer->name);
      continue;
    }

    gpuMeshVcol *vcol = mesh_vcol;
    MLoopCol *mcol = nullptr;
    MPropCol *pcol = nullptr;

    if (ref.layer->type == CD_PROP_COLOR) {
      pcol = static_cast<MPropCol *>(cdata->layers[layer_i].data);
    }
    else {
      mcol = static_cast<MLoopCol *>(cdata->layers[layer_i].data);
    }

    const bool is_vert = ref.domain == ATTR_DOMAIN_POINT;

    if (extract_bmesh) {
      BMesh *bm = coarse_mesh->edit_mesh->bm;
      BMIter iter;
      BMFace *f;
      int cd_ofs = cdata->layers[layer_i].offset;
      const bool is_byte = ref.layer->type == CD_PROP_BYTE_COLOR;

      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        BMLoop *l_iter = f->l_first;

        do {
          BMElem *elem = is_vert ? reinterpret_cast<BMElem *>(l_iter->v) :
                                   reinterpret_cast<BMElem *>(l_iter);

          if (is_byte) {
            MLoopCol *mcol2 = static_cast<MLoopCol *>(BM_ELEM_CD_GET_VOID_P(elem, cd_ofs));

            vcol->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->r]);
            vcol->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->g]);
            vcol->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->b]);
            vcol->a = unit_float_to_ushort_clamp(mcol2->a * (1.0f / 255.0f));
          }
          else {
            MPropCol *pcol2 = static_cast<MPropCol *>(BM_ELEM_CD_GET_VOID_P(elem, cd_ofs));

            vcol->r = unit_float_to_ushort_clamp(pcol2->color[0]);
            vcol->g = unit_float_to_ushort_clamp(pcol2->color[1]);
            vcol->b = unit_float_to_ushort_clamp(pcol2->color[2]);
            vcol->a = unit_float_to_ushort_clamp(pcol2->color[3]);
          }
        } while ((l_iter = l_iter->next) != f->l_first);
      }
    }
    else {
      for (int ml_index = 0; ml_index < coarse_mesh->totloop; ml_index++, vcol++, ml++) {
        int idx = is_vert ? ml->v : ml_index;

        if (mcol) {
          vcol->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol[idx].r]);
          vcol->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol[idx].g]);
          vcol->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol[idx].b]);
          vcol->a = unit_float_to_ushort_clamp(mcol[idx].a * (1.0f / 255.0f));
        }
        else if (pcol) {
          vcol->r = unit_float_to_ushort_clamp(pcol[idx].color[0]);
          vcol->g = unit_float_to_ushort_clamp(pcol[idx].color[1]);
          vcol->b = unit_float_to_ushort_clamp(pcol[idx].color[2]);
          vcol->a = unit_float_to_ushort_clamp(pcol[idx].color[3]);
        }
      }
    }

    /* Ensure data is uploaded properly. */
    GPU_vertbuf_tag_dirty(src_data);
    draw_subdiv_interp_custom_data(subdiv_cache, src_data, dst_buffer, 4, dst_offset, true);
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
