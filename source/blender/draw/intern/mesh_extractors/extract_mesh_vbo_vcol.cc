/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BKE_attribute.h"
#include "BLI_string.h"

#include "draw_subdivision.h"
#include "extract_mesh.h"

#include <vector>

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract VCol
 * \{ */

/* get all vcol layers as AttributeRefs, filtered by vcol_layers.
 *  the casual use of std::vector should be okay here.
 */
static std::vector<AttributeRef> get_vcol_refs(const CustomData *cd_vdata,
                                               const CustomData *cd_ldata,
                                               const uint vcol_layers)
{
  std::vector<AttributeRef> refs;

  const CustomDataType vcol_types[2] = {CD_PROP_COLOR, CD_MLOOPCOL};
  const AttributeDomain domains[2] = {ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER};

  uint layeri = 0;

  for (int step1 = 0; step1 < ARRAY_SIZE(vcol_types); step1++) {
    CustomDataType type = vcol_types[step1];

    for (int step2 = 0; step2 < 2; step2++) {
      const CustomData *cdata = step2 ? cd_ldata : cd_vdata;
      AttributeDomain domain = domains[step2];

      int i = cdata->typemap[(int)type];

      if (i == -1) {
        continue;
      }

      for (; i < cdata->totlayer && (CustomDataType)cdata->layers[i].type == type; i++, layeri++) {
        const CustomDataLayer *layer = cdata->layers + i;

        if (!(vcol_layers & (1UL << layeri)) || (layer->flag & CD_FLAG_TEMPORARY)) {
          continue;
        }

        AttributeRef ref;
        ref.domain = domain;
        ref.type = layer->type;
        BLI_strncpy(ref.name, layer->name, sizeof(ref.name));

        refs.push_back(ref);
      }
    }
  }

  return refs;
}

extern "C" int mesh_cd_get_vcol_i(const Mesh *me,
                                  const CustomData *cd_vdata,
                                  const CustomData *cd_ldata,
                                  const struct AttributeRef *ref)
{
  auto refs = get_vcol_refs(cd_vdata, cd_ldata, UINT_MAX);
  int i = 0;

  for (AttributeRef ref2 : refs) {
    if (BKE_id_attribute_ref_equals(&ref2, ref)) {
      return i;
    }
    i++;
  }

  return -1;
}
extern "C" int mesh_cd_get_active_color_i(const Mesh *me,
                                          const CustomData *cd_vdata,
                                          const CustomData *cd_ldata)
{
  return mesh_cd_get_vcol_i(me, cd_vdata, cd_ldata, &me->attr_color_active);
}

extern "C" int mesh_cd_get_render_color_i(const Mesh *me,
                                          const CustomData *cd_vdata,
                                          const CustomData *cd_ldata)
{
  return mesh_cd_get_vcol_i(me, cd_vdata, cd_ldata, &me->attr_color_render);
}

/* Initialize the common vertex format for vcol for coarse and subdivided meshes. */
static void init_vcol_format(GPUVertFormat *format,
                             const MeshBatchCache *cache,
                             CustomData *cd_vdata,
                             CustomData *cd_ldata,
                             AttributeRef *attr_active,
                             AttributeRef *attr_render)
{
  GPU_vertformat_deinterleave(format);

  /* note that there are two vcol types that work across point and corner domains */

  const uint32_t vcol_layers = cache->cd_used.vcol;

  std::vector<AttributeRef> refs = get_vcol_refs(cd_vdata, cd_ldata, vcol_layers);

  for (AttributeRef ref : refs) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];

    GPU_vertformat_safe_attr_name(ref.name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

    BLI_snprintf(attr_name, sizeof(attr_name), "c%s", attr_safe_name);

    GPU_vertformat_attr_add(format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

    if (BKE_id_attribute_ref_equals(attr_active, &ref)) {
      GPU_vertformat_alias_add(format, "ac");
    }

    if (BKE_id_attribute_ref_equals(attr_render, &ref)) {
      GPU_vertformat_alias_add(format, "c");
    }

    /* Gather number of auto layers. */
    /* We only do `vcols` that are not overridden by `uvs`. */
    if (ref.domain == ATTR_DOMAIN_CORNER &&
        CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, ref.name) == -1) {
      BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
      GPU_vertformat_alias_add(format, attr_name);
    }
  }

#if 0
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
#endif
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
  AttributeRef *attr_active = BKE_id_attributes_active_color_ref_p(&mr->me->id);
  AttributeRef *attr_render = BKE_id_attributes_render_color_ref_p(&mr->me->id);

  const uint32_t vcol_layers = cache->cd_used.vcol;
  init_vcol_format(&format, cache, cd_vdata, cd_ldata, attr_active, attr_render);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  gpuMeshVcol *vcol_data = (gpuMeshVcol *)GPU_vertbuf_get_data(vbo);

  std::vector<AttributeRef> refs = get_vcol_refs(cd_vdata, cd_ldata, vcol_layers);

  for (auto ref : refs) {
    CustomData *cdata = ref.domain == ATTR_DOMAIN_POINT ? cd_vdata : cd_ldata;

    if (mr->extract_type == MR_EXTRACT_BMESH) {
      int cd_ofs = CustomData_get_named_offset(cdata, ref.type, ref.name);

      if (cd_ofs == -1) {
        vcol_data += ref.domain == ATTR_DOMAIN_POINT ? mr->bm->totvert : mr->bm->totloop;
        continue;
      }

      BMIter iter;
      const bool is_byte = ref.type == CD_MLOOPCOL;
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
      if (ref.domain == ATTR_DOMAIN_CORNER) {
        if (ref.type == CD_MLOOPCOL) {
          int totloop = mr->loop_len;
          int idx = CustomData_get_named_layer_index(cdata, ref.type, ref.name);

          if (idx == -1) {
            vcol_data += totloop;
            continue;
          }

          MLoopCol *mloopcol = (MLoopCol *)cdata->layers[idx].data;

          for (int i = 0; i < totloop; i++, mloopcol++) {
            vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
            vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
            vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
            vcol_data->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
            vcol_data++;
          }
        }
        else {
          int totloop = mr->loop_len;
          int idx = CustomData_get_named_layer_index(cdata, ref.type, ref.name);

          if (idx == -1) {
            vcol_data += totloop;
            continue;
          }

          MPropCol *pcol = (MPropCol *)cdata->layers[idx].data;

          for (int i = 0; i < totloop; i++, pcol++) {
            vcol_data->r = unit_float_to_ushort_clamp(pcol->color[0]);
            vcol_data->g = unit_float_to_ushort_clamp(pcol->color[1]);
            vcol_data->b = unit_float_to_ushort_clamp(pcol->color[2]);
            vcol_data->a = unit_float_to_ushort_clamp(pcol->color[3]);
            vcol_data++;
          }
        }
      }
      else if (ref.domain == ATTR_DOMAIN_POINT) {
        if (ref.type == CD_MLOOPCOL) {
          int totloop = mr->loop_len;
          int idx = CustomData_get_named_layer_index(cdata, ref.type, ref.name);

          if (idx == -1) {
            vcol_data += totloop;
            continue;
          }

          const MLoop *ml = mr->mloop;
          MLoopCol *mloopcol = (MLoopCol *)cdata->layers[idx].data;

          for (int i = 0; i < totloop; i++, ml++) {
            vcol_data->r = unit_float_to_ushort_clamp(
                BLI_color_from_srgb_table[mloopcol[ml->v].r]);
            vcol_data->g = unit_float_to_ushort_clamp(
                BLI_color_from_srgb_table[mloopcol[ml->v].g]);
            vcol_data->b = unit_float_to_ushort_clamp(
                BLI_color_from_srgb_table[mloopcol[ml->v].b]);
            vcol_data->a = unit_float_to_ushort_clamp(mloopcol[ml->v].a * (1.0f / 255.0f));
            vcol_data++;
          }
        }
        else {
          int totloop = mr->loop_len;
          int idx = CustomData_get_named_layer_index(cdata, ref.type, ref.name);
          const MLoop *ml = mr->mloop;

          if (idx == -1) {
            vcol_data += totloop;
            continue;
          }

          MPropCol *pcol = (MPropCol *)cdata->layers[idx].data;

          for (int i = 0; i < totloop; i++, ml++) {
            vcol_data->r = unit_float_to_ushort_clamp(pcol[ml->v].color[0]);
            vcol_data->g = unit_float_to_ushort_clamp(pcol[ml->v].color[1]);
            vcol_data->b = unit_float_to_ushort_clamp(pcol[ml->v].color[2]);
            vcol_data->a = unit_float_to_ushort_clamp(pcol[ml->v].color[3]);
            vcol_data++;
          }
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

  AttributeRef *attr_active = BKE_id_attributes_active_color_ref_p(&coarse_mesh->id);
  AttributeRef *attr_render = BKE_id_attributes_render_color_ref_p(&coarse_mesh->id);

  GPUVertFormat format = {0};
  init_vcol_format(
      &format, cache, &coarse_mesh->vdata, &coarse_mesh->ldata, attr_active, attr_render);

  GPU_vertbuf_init_build_on_device(dst_buffer, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *src_data = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(src_data, get_coarse_vcol_format(), GPU_USAGE_DYNAMIC);

  GPU_vertbuf_data_alloc(src_data, coarse_mesh->totloop);

  gpuMeshVcol *mesh_vcol = (gpuMeshVcol *)GPU_vertbuf_get_data(src_data);

  const CustomData *cd_vdata = &coarse_mesh->vdata;
  const CustomData *cd_ldata = &coarse_mesh->ldata;

  const uint vcol_layers = cache->cd_used.vcol;

  std::vector<AttributeRef> refs = get_vcol_refs(cd_vdata, cd_ldata, vcol_layers);

  gpuMeshVcol *vcol = mesh_vcol;

  /* Index of the vertex color layer in the compact buffer. Used vertex color layers are stored in
   * a single buffer. */
  int pack_layer_index = 0;
  for (auto ref : refs) {
    /* Include stride in offset, we use a stride of 2 since colors are packed into 2 uints. */
    const int dst_offset = (int)subdiv_cache->num_subdiv_loops * 2 * pack_layer_index++;

    const CustomData *cdata = ref.domain == ATTR_DOMAIN_POINT ? cd_vdata : cd_ldata;
    const MLoop *ml = coarse_mesh->mloop;

    int layer_i = CustomData_get_named_layer_index(cdata, ref.type, ref.name);

    if (layer_i == -1) {
      printf("%s: missing color layer %s\n", __func__, ref.name);
      vcol += coarse_mesh->totloop;
      continue;
    }

    MLoopCol *mcol = NULL;
    MPropCol *pcol = NULL;

    if (ref.type == CD_PROP_COLOR) {
      pcol = static_cast<MPropCol *>(cdata->layers[layer_i].data);
    }
    else {
      mcol = static_cast<MLoopCol *>(cdata->layers[layer_i].data);
    }

    const bool is_vert = ref.domain == ATTR_DOMAIN_POINT;

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
