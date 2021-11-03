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

#include "extract_mesh.h"

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

  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;

#if 0
  uint32_t vcol_layers = cache->cd_used.vcol;
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
      /* We only do `vcols` that are not overridden by `uvs`. */
      if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, layer_name) == -1) {
        BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
        GPU_vertformat_alias_add(&format, attr_name);
      }
    }
  }
#endif

  /*
  note that there are three color attribute types that operate over two domains
  (verts and face corners)
  */
  int vcol_types[2] = {CD_MLOOPCOL, CD_PROP_COLOR};

  CustomDataLayer *actlayer = BKE_id_attributes_active_get((ID *)mr->me);
  AttributeDomain actdomain = actlayer ? BKE_id_attribute_domain((ID *)mr->me, actlayer) :
                                         ATTR_DOMAIN_AUTO;
  int actn = -1;

  if (actlayer && ELEM(actdomain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER)) {
    CustomData *cdata = actdomain == ATTR_DOMAIN_POINT ? &mr->me->vdata : &mr->me->ldata;
    actn = actlayer - (cdata->layers + cdata->typemap[actlayer->type]);
  }

  for (int i = 0; i < ARRAY_SIZE(vcol_types); i++) {
    int type = vcol_types[i];

    for (int step = 0; step < 2; step++) {
      CustomData *cdata = step ? cd_ldata : cd_vdata;
      int count = CustomData_number_of_layers(cdata, type);
      AttributeDomain domain = step ? ATTR_DOMAIN_CORNER : ATTR_DOMAIN_POINT;

      for (int j = 0; j < count; j++) {
        int idx = CustomData_get_layer_index_n(cdata, type, j);

        char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
        const char *layer_name = CustomData_get_layer_name(cdata, type, j);
        GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

        BLI_snprintf(attr_name, sizeof(attr_name), "c%s", attr_safe_name);
        GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

        if (j == CustomData_get_render_layer(cdata, type)) {
          GPU_vertformat_alias_add(&format, "c");
        }

        bool is_active = actn == -1 && j == CustomData_get_active_layer(cdata, type);
        is_active |= actn != -1 && domain == actdomain && j == actn && type == actlayer->type;

        if (is_active) {
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

  for (int i = 0; i < 3; i++) {
    int type = vcol_types[i];

    for (int step = 0; step < 2; step++) {
      CustomData *cdata = step ? cd_ldata : cd_vdata;
      int count = CustomData_number_of_layers(cdata, type);

      for (int j = 0; j < count; j++) {
        int idx = CustomData_get_layer_index_n(cdata, type, j);

        if (mr->extract_type == MR_EXTRACT_BMESH) {
          BMFace *f;
          BMIter iter;

          int cd_vcol = cdata->layers[idx].offset;

          BM_ITER_MESH (f, &iter, mr->bm, BM_FACES_OF_MESH) {
            BMLoop *l_iter = BM_FACE_FIRST_LOOP(f);
            do {
              BMElem *elem = step ? (BMElem *)l_iter : (BMElem *)l_iter->v;

              switch (type) {
                case CD_PROP_COLOR: {
                  float *color = (float *)BM_ELEM_CD_GET_VOID_P(elem, cd_vcol);

                  vcol_data->r = unit_float_to_ushort_clamp(color[0]);
                  vcol_data->g = unit_float_to_ushort_clamp(color[1]);
                  vcol_data->b = unit_float_to_ushort_clamp(color[2]);
                  vcol_data->a = unit_float_to_ushort_clamp(color[3]);

                  break;
                }
                case CD_MLOOPCOL: {
                  float temp[4];

                  MLoopCol *mloopcol = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(elem, cd_vcol);
                  rgba_float_to_uchar((unsigned char *)mloopcol, temp);
                  linearrgb_to_srgb_v3_v3(temp, temp);

                  vcol_data->r = unit_float_to_ushort_clamp(temp[0]);
                  vcol_data->g = unit_float_to_ushort_clamp(temp[1]);
                  vcol_data->b = unit_float_to_ushort_clamp(temp[2]);
                  vcol_data->a = unit_float_to_ushort_clamp(temp[3]);
                  break;
                }
              }

              vcol_data++;
            } while ((l_iter = l_iter->next) != BM_FACE_FIRST_LOOP(f));
          }
        }
        else {
          using MPropCol3 = struct {
            float color[3];
          };

          switch (type) {
            case CD_PROP_COLOR: {
              MPropCol *colors = (MPropCol *)cdata->layers[idx].data;

              if (step) {
                for (int k = 0; k < mr->loop_len; k++, vcol_data++, colors++) {
                  vcol_data->r = unit_float_to_ushort_clamp(colors->color[0]);
                  vcol_data->g = unit_float_to_ushort_clamp(colors->color[1]);
                  vcol_data->b = unit_float_to_ushort_clamp(colors->color[2]);
                  vcol_data->a = unit_float_to_ushort_clamp(colors->color[3]);
                }
              }
              else {
                const MLoop *ml = mr->mloop;

                for (int k = 0; k < mr->loop_len; k++, vcol_data++, ml++) {
                  MPropCol *color = colors + ml->v;

                  vcol_data->r = unit_float_to_ushort_clamp(color->color[0]);
                  vcol_data->g = unit_float_to_ushort_clamp(color->color[1]);
                  vcol_data->b = unit_float_to_ushort_clamp(color->color[2]);
                  vcol_data->a = unit_float_to_ushort_clamp(color->color[3]);
                }
              }
              break;
            }
            case CD_MLOOPCOL: {
              MLoopCol *colors = (MLoopCol *)cdata->layers[idx].data;

              if (step) {
                for (int k = 0; k < mr->loop_len; k++, vcol_data++, colors++) {
                  float temp[4];
                  rgba_float_to_uchar((unsigned char *)colors, temp);
                  linearrgb_to_srgb_v3_v3(temp, temp);

                  vcol_data->r = unit_float_to_ushort_clamp(temp[0]);
                  vcol_data->g = unit_float_to_ushort_clamp(temp[1]);
                  vcol_data->b = unit_float_to_ushort_clamp(temp[2]);
                  vcol_data->a = unit_float_to_ushort_clamp(temp[3]);
                }
              }
              else {
                const MLoop *ml = mr->mloop;

                for (int k = 0; k < mr->loop_len; k++, vcol_data++, ml++) {
                  MLoopCol *color = colors + ml->v;
                  float temp[4];

                  rgba_float_to_uchar((unsigned char *)color, temp);
                  linearrgb_to_srgb_v3_v3(temp, temp);

                  vcol_data->r = unit_float_to_ushort_clamp(temp[0]);
                  vcol_data->g = unit_float_to_ushort_clamp(temp[1]);
                  vcol_data->b = unit_float_to_ushort_clamp(temp[2]);
                  vcol_data->a = unit_float_to_ushort_clamp(temp[3]);
                }
              }
              break;
            }
          }
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
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.vcol);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_vcol = blender::draw::create_extractor_vcol();
}
