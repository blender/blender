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

#include "BKE_paint.h"

#include "draw_cache_extract_mesh_private.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Sculpt Data
 * \{ */

static void extract_sculpt_data_init(const MeshRenderData *mr,
                                     struct MeshBatchCache *UNUSED(cache),
                                     void *buf,
                                     void *UNUSED(tls_data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  GPUVertFormat format = {0};

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  CustomData *cd_pdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->pdata : &mr->me->pdata;

  float *cd_mask = (float *)CustomData_get_layer(cd_vdata, CD_PAINT_MASK);
  int *cd_face_set = (int *)CustomData_get_layer(cd_pdata, CD_SCULPT_FACE_SETS);

  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "fset", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_attr_add(&format, "msk", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  struct gpuSculptData {
    uint8_t face_set_color[4];
    float mask;
  };

  gpuSculptData *vbo_data = (gpuSculptData *)GPU_vertbuf_get_data(vbo);
  MLoop *loops = (MLoop *)CustomData_get_layer(cd_ldata, CD_MLOOP);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    int cd_mask_ofs = CustomData_get_offset(cd_vdata, CD_PAINT_MASK);
    int cd_face_set_ofs = CustomData_get_offset(cd_pdata, CD_SCULPT_FACE_SETS);
    BMIter f_iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        float v_mask = 0.0f;
        if (cd_mask) {
          v_mask = BM_ELEM_CD_GET_FLOAT(l_iter->v, cd_mask_ofs);
        }
        vbo_data->mask = v_mask;
        uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        if (cd_face_set) {
          const int face_set_id = BM_ELEM_CD_GET_INT(l_iter->f, cd_face_set_ofs);
          if (face_set_id != mr->me->face_sets_color_default) {
            BKE_paint_face_set_overlay_color_get(
                face_set_id, mr->me->face_sets_color_seed, face_set_color);
          }
        }
        copy_v3_v3_uchar(vbo_data->face_set_color, face_set_color);
        vbo_data++;
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    int mp_loop = 0;
    for (int mp_index = 0; mp_index < mr->poly_len; mp_index++) {
      const MPoly *p = &mr->mpoly[mp_index];
      for (int l = 0; l < p->totloop; l++) {
        float v_mask = 0.0f;
        if (cd_mask) {
          v_mask = cd_mask[loops[mp_loop].v];
        }
        vbo_data->mask = v_mask;

        uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        if (cd_face_set) {
          const int face_set_id = cd_face_set[mp_index];
          /* Skip for the default color Face Set to render it white. */
          if (face_set_id != mr->me->face_sets_color_default) {
            BKE_paint_face_set_overlay_color_get(
                face_set_id, mr->me->face_sets_color_seed, face_set_color);
          }
        }
        copy_v3_v3_uchar(vbo_data->face_set_color, face_set_color);
        mp_loop++;
        vbo_data++;
      }
    }
  }
}

constexpr MeshExtract create_extractor_sculpt_data()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_sculpt_data_init;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, vbo.sculpt_data);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_sculpt_data = blender::draw::create_extractor_sculpt_data();
}
