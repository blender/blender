/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_texture_info.hh"

namespace blender::draw::image_engine {

/** \brief Create GPUBatch for a IMAGE_ScreenSpaceTextureInfo. */
class BatchUpdater {
  TextureInfo &info;

  GPUVertFormat format = {0};
  int pos_id;
  int uv_id;

 public:
  BatchUpdater(TextureInfo &info) : info(info) {}

  void update_batch()
  {
    ensure_clear_batch();
    ensure_format();
    init_batch();
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

  template<typename DataType, typename RectType>
  static void fill_tri_fan_from_rect(DataType result[4][2], RectType &rect)
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

  GPUVertBuf *create_vbo()
  {
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, 4);
    int pos[4][2];
    fill_tri_fan_from_rect<int, rcti>(pos, info.clipping_bounds);
    float uv[4][2];
    fill_tri_fan_from_rect<float, rctf>(uv, info.clipping_uv_bounds);

    for (int i = 0; i < 4; i++) {
      GPU_vertbuf_attr_set(vbo, pos_id, i, pos[i]);
      GPU_vertbuf_attr_set(vbo, uv_id, i, uv[i]);
    }

    return vbo;
  }

  void ensure_format()
  {
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT);
      GPU_vertformat_attr_add(&format, "uv", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      pos_id = GPU_vertformat_attr_id_get(&format, "pos");
      uv_id = GPU_vertformat_attr_id_get(&format, "uv");
    }
  }
};

}  // namespace blender::draw::image_engine
