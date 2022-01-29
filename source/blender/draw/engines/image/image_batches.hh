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
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_texture_info.hh"

/** \brief Create GPUBatch for a IMAGE_ScreenSpaceTextureInfo. */
class BatchUpdater {
  TextureInfo &info;

  GPUVertFormat format = {0};
  int pos_id;
  int uv_id;

 public:
  BatchUpdater(TextureInfo &info) : info(info)
  {
  }

  void update_batch()
  {
    ensure_clear_batch();
    ensure_format();
    init_batch();
  }

  void discard_batch()
  {
    GPU_BATCH_DISCARD_SAFE(info.batch);
  }

 private:
  void ensure_clear_batch()
  {
    GPU_BATCH_CLEAR_SAFE(info.batch);
    if (info.batch == nullptr) {
      info.batch = GPU_batch_calloc();
    }
  }

  void init_batch()
  {
    GPUVertBuf *vbo = create_vbo();
    GPU_batch_init_ex(info.batch, GPU_PRIM_TRI_FAN, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  }

  GPUVertBuf *create_vbo()
  {
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 4);
    float pos[4][2];
    fill_tri_fan_from_rctf(pos, info.clipping_bounds);
    float uv[4][2];
    fill_tri_fan_from_rctf(uv, info.uv_bounds);

    for (int i = 0; i < 4; i++) {
      GPU_vertbuf_attr_set(vbo, pos_id, i, pos[i]);
      GPU_vertbuf_attr_set(vbo, uv_id, i, uv[i]);
    }

    return vbo;
  }

  static void fill_tri_fan_from_rctf(float result[4][2], rctf &rect)
  {
    result[0][0] = rect.xmin;
    result[0][1] = rect.ymin;
    result[1][0] = rect.xmax;
    result[1][1] = rect.ymin;
    result[2][0] = rect.xmax;
    result[2][1] = rect.ymax;
    result[3][0] = rect.xmin;
    result[3][1] = rect.ymax;
  }

  void ensure_format()
  {
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      GPU_vertformat_attr_add(&format, "uv", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      pos_id = GPU_vertformat_attr_id_get(&format, "pos");
      uv_id = GPU_vertformat_attr_id_get(&format, "uv");
    }
  }
};
