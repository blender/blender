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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Utilities to read id buffer created in select_engine.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_bitmap.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"

#include "GPU_select.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"
#include "DRW_select_buffer.h"

#include "draw_manager.h"

#include "../engines/select/select_engine.h"

/* -------------------------------------------------------------------- */
/** \name Buffer of select ID's
 * \{ */

/* Main function to read a block of pixels from the select frame buffer. */
uint *DRW_select_buffer_read(struct Depsgraph *depsgraph,
                             struct ARegion *region,
                             struct View3D *v3d,
                             const rcti *rect,
                             uint *r_buf_len)
{
  uint *r_buf = NULL;
  uint buf_len = 0;

  /* Clamp rect. */
  rcti r = {
      .xmin = 0,
      .xmax = region->winx,
      .ymin = 0,
      .ymax = region->winy,
  };

  /* Make sure that the rect is within the bounds of the viewport.
   * Some GPUs have problems reading pixels off limits. */
  rcti rect_clamp = *rect;
  if (BLI_rcti_isect(&r, &rect_clamp, &rect_clamp)) {
    struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

    DRW_opengl_context_enable();
    /* Update the drawing. */
    DRW_draw_select_id(depsgraph, region, v3d, rect);

    if (select_ctx->index_drawn_len > 1) {
      BLI_assert(region->winx == GPU_texture_width(DRW_engine_select_texture_get()) &&
                 region->winy == GPU_texture_height(DRW_engine_select_texture_get()));

      /* Read the UI32 pixels. */
      buf_len = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);
      r_buf = MEM_mallocN(buf_len * sizeof(*r_buf), __func__);

      GPUFrameBuffer *select_id_fb = DRW_engine_select_framebuffer_get();
      GPU_framebuffer_bind(select_id_fb);
      GPU_framebuffer_read_color(select_id_fb,
                                 rect_clamp.xmin,
                                 rect_clamp.ymin,
                                 BLI_rcti_size_x(&rect_clamp),
                                 BLI_rcti_size_y(&rect_clamp),
                                 1,
                                 0,
                                 GPU_DATA_UINT,
                                 r_buf);

      if (!BLI_rcti_compare(rect, &rect_clamp)) {
        /* The rect has been clamped so you need to realign the buffer and fill in the blanks */
        GPU_select_buffer_stride_realign(rect, &rect_clamp, r_buf);
      }
    }

    GPU_framebuffer_restore();
    DRW_opengl_context_disable();
  }

  if (r_buf_len) {
    *r_buf_len = buf_len;
  }

  return r_buf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Bitmap from ID's
 *
 * Given a buffer of select ID's, fill in a booleans (true/false) per index.
 * #BLI_bitmap is used for memory efficiency.
 *
 * \{ */

/**
 * \param rect: The rectangle to sample indices from (min/max inclusive).
 * \returns a #BLI_bitmap the length of \a bitmap_len or NULL on failure.
 */
uint *DRW_select_buffer_bitmap_from_rect(struct Depsgraph *depsgraph,
                                         struct ARegion *region,
                                         struct View3D *v3d,
                                         const rcti *rect,
                                         uint *r_bitmap_len)
{
  struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

  rcti rect_px = *rect;
  rect_px.xmax += 1;
  rect_px.ymax += 1;

  uint buf_len;
  uint *buf = DRW_select_buffer_read(depsgraph, region, v3d, &rect_px, &buf_len);
  if (buf == NULL) {
    return NULL;
  }

  BLI_assert(select_ctx->index_drawn_len > 0);
  const uint bitmap_len = select_ctx->index_drawn_len - 1;

  BLI_bitmap *bitmap_buf = BLI_BITMAP_NEW(bitmap_len, __func__);
  const uint *buf_iter = buf;
  while (buf_len--) {
    const uint index = *buf_iter - 1;
    if (index < bitmap_len) {
      BLI_BITMAP_ENABLE(bitmap_buf, index);
    }
    buf_iter++;
  }
  MEM_freeN((void *)buf);

  if (r_bitmap_len) {
    *r_bitmap_len = bitmap_len;
  }

  return bitmap_buf;
}

/**
 * \param center: Circle center.
 * \param radius: Circle radius.
 * \param r_bitmap_len: Number of indices in the selection id buffer.
 * \returns a #BLI_bitmap the length of \a r_bitmap_len or NULL on failure.
 */
uint *DRW_select_buffer_bitmap_from_circle(struct Depsgraph *depsgraph,
                                           struct ARegion *region,
                                           struct View3D *v3d,
                                           const int center[2],
                                           const int radius,
                                           uint *r_bitmap_len)
{
  struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

  const rcti rect = {
      .xmin = center[0] - radius,
      .xmax = center[0] + radius + 1,
      .ymin = center[1] - radius,
      .ymax = center[1] + radius + 1,
  };

  const uint *buf = DRW_select_buffer_read(depsgraph, region, v3d, &rect, NULL);

  if (buf == NULL) {
    return NULL;
  }

  BLI_assert(select_ctx->index_drawn_len > 0);
  const uint bitmap_len = select_ctx->index_drawn_len - 1;

  BLI_bitmap *bitmap_buf = BLI_BITMAP_NEW(bitmap_len, __func__);
  const uint *buf_iter = buf;
  const int radius_sq = radius * radius;
  for (int yc = -radius; yc <= radius; yc++) {
    for (int xc = -radius; xc <= radius; xc++, buf_iter++) {
      if (xc * xc + yc * yc < radius_sq) {
        /* Intentionally wrap to max value if this is zero. */
        const uint index = *buf_iter - 1;
        if (index < bitmap_len) {
          BLI_BITMAP_ENABLE(bitmap_buf, index);
        }
      }
    }
  }
  MEM_freeN((void *)buf);

  if (r_bitmap_len) {
    *r_bitmap_len = bitmap_len;
  }

  return bitmap_buf;
}

struct PolyMaskData {
  BLI_bitmap *px;
  int width;
};

static void drw_select_mask_px_cb(int x, int x_end, int y, void *user_data)
{
  struct PolyMaskData *data = user_data;
  BLI_bitmap *px = data->px;
  int i = (y * data->width) + x;
  do {
    BLI_BITMAP_ENABLE(px, i);
    i++;
  } while (++x != x_end);
}

/**
 * \param poly: The polygon coordinates.
 * \param poly_len: Length of the polygon.
 * \param rect: Polygon boundaries.
 * \returns a #BLI_bitmap.
 */
uint *DRW_select_buffer_bitmap_from_poly(struct Depsgraph *depsgraph,
                                         struct ARegion *region,
                                         struct View3D *v3d,
                                         const int poly[][2],
                                         const int poly_len,
                                         const rcti *rect,
                                         uint *r_bitmap_len)
{
  struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

  rcti rect_px = *rect;
  rect_px.xmax += 1;
  rect_px.ymax += 1;

  uint buf_len;
  uint *buf = DRW_select_buffer_read(depsgraph, region, v3d, &rect_px, &buf_len);
  if (buf == NULL) {
    return NULL;
  }

  BLI_bitmap *buf_mask = BLI_BITMAP_NEW(buf_len, __func__);

  struct PolyMaskData poly_mask_data;
  poly_mask_data.px = buf_mask;
  poly_mask_data.width = (rect->xmax - rect->xmin) + 1;

  BLI_bitmap_draw_2d_poly_v2i_n(rect_px.xmin,
                                rect_px.ymin,
                                rect_px.xmax,
                                rect_px.ymax,
                                poly,
                                poly_len,
                                drw_select_mask_px_cb,
                                &poly_mask_data);

  BLI_assert(select_ctx->index_drawn_len > 0);
  const uint bitmap_len = select_ctx->index_drawn_len - 1;

  BLI_bitmap *bitmap_buf = BLI_BITMAP_NEW(bitmap_len, __func__);
  const uint *buf_iter = buf;
  int i = 0;
  while (buf_len--) {
    const uint index = *buf_iter - 1;
    if (index < bitmap_len && BLI_BITMAP_TEST(buf_mask, i)) {
      BLI_BITMAP_ENABLE(bitmap_buf, index);
    }
    buf_iter++;
    i++;
  }
  MEM_freeN((void *)buf);
  MEM_freeN(buf_mask);

  if (r_bitmap_len) {
    *r_bitmap_len = bitmap_len;
  }

  return bitmap_buf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Single Select ID's
 *
 * Given a buffer of select ID's, find the a single select id.
 *
 * \{ */

/**
 * Samples a single pixel.
 */
uint DRW_select_buffer_sample_point(struct Depsgraph *depsgraph,
                                    struct ARegion *region,
                                    struct View3D *v3d,
                                    const int center[2])
{
  uint ret = 0;

  const rcti rect = {
      .xmin = center[0],
      .xmax = center[0] + 1,
      .ymin = center[1],
      .ymax = center[1] + 1,
  };

  uint buf_len;
  uint *buf = DRW_select_buffer_read(depsgraph, region, v3d, &rect, &buf_len);
  if (buf) {
    BLI_assert(0 != buf_len);
    ret = buf[0];
    MEM_freeN(buf);
  }

  return ret;
}

struct SelectReadData {
  const void *val_ptr;
  uint id_min;
  uint id_max;
  uint r_index;
};

static bool select_buffer_test_fn(const void *__restrict value, void *__restrict userdata)
{
  struct SelectReadData *data = userdata;
  uint hit_id = *(uint *)value;
  if (hit_id && hit_id >= data->id_min && hit_id < data->id_max) {
    /* Start at 1 to confirm. */
    data->val_ptr = value;
    data->r_index = (hit_id - data->id_min) + 1;
    return true;
  }
  return false;
}

/**
 * Find the selection id closest to \a center.
 * \param dist: Use to initialize the distance,
 * when found, this value is set to the distance of the selection that's returned.
 */
uint DRW_select_buffer_find_nearest_to_point(struct Depsgraph *depsgraph,
                                             struct ARegion *region,
                                             struct View3D *v3d,
                                             const int center[2],
                                             const uint id_min,
                                             const uint id_max,
                                             uint *dist)
{
  /* Create region around center (typically the mouse cursor).
   * This must be square and have an odd width. */

  rcti rect;
  BLI_rcti_init_pt_radius(&rect, center, *dist);
  rect.xmax += 1;
  rect.ymax += 1;

  int width = BLI_rcti_size_x(&rect);
  int height = width;

  /* Read from selection framebuffer. */

  uint buf_len;
  const uint *buf = DRW_select_buffer_read(depsgraph, region, v3d, &rect, &buf_len);

  if (buf == NULL) {
    return 0;
  }

  const int shape[2] = {height, width};
  const int center_yx[2] = {(height - 1) / 2, (width - 1) / 2};
  struct SelectReadData data = {NULL, id_min, id_max, 0};
  BLI_array_iter_spiral_square(buf, shape, center_yx, select_buffer_test_fn, &data);

  if (data.val_ptr) {
    size_t offset = ((size_t)data.val_ptr - (size_t)buf) / sizeof(*buf);
    int hit_x = offset % width;
    int hit_y = offset / width;
    *dist = (uint)(abs(hit_y - center_yx[0]) + abs(hit_x - center_yx[1]));
  }

  MEM_freeN((void *)buf);
  return data.r_index;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Utils
 * \{ */

bool DRW_select_buffer_elem_get(const uint sel_id,
                                uint *r_elem,
                                uint *r_base_index,
                                char *r_elem_type)
{
  struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

  char elem_type = 0;
  uint elem_id = 0;
  uint base_index = 0;

  for (; base_index < select_ctx->objects_drawn_len; base_index++) {
    struct ObjectOffsets *base_ofs = &select_ctx->index_offsets[base_index];

    if (base_ofs->face > sel_id) {
      elem_id = sel_id - base_ofs->face_start;
      elem_type = SCE_SELECT_FACE;
      break;
    }
    if (base_ofs->edge > sel_id) {
      elem_id = sel_id - base_ofs->edge_start;
      elem_type = SCE_SELECT_EDGE;
      break;
    }
    if (base_ofs->vert > sel_id) {
      elem_id = sel_id - base_ofs->vert_start;
      elem_type = SCE_SELECT_VERTEX;
      break;
    }
  }

  if (base_index == select_ctx->objects_drawn_len) {
    return false;
  }

  *r_elem = elem_id;

  if (r_base_index) {
    Object *obj_orig = DEG_get_original_object(select_ctx->objects_drawn[base_index]);
    *r_base_index = obj_orig->runtime.select_id;
  }

  if (r_elem_type) {
    *r_elem_type = elem_type;
  }

  return true;
}

uint DRW_select_buffer_context_offset_for_object_elem(Depsgraph *depsgraph,
                                                      Object *object,
                                                      char elem_type)
{
  struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, object);

  SELECTID_ObjectData *sel_data = (SELECTID_ObjectData *)DRW_drawdata_get(
      &ob_eval->id, &draw_engine_select_type);

  if (!sel_data || !sel_data->is_drawn) {
    return 0;
  }

  struct ObjectOffsets *base_ofs = &select_ctx->index_offsets[sel_data->drawn_index];

  if (elem_type == SCE_SELECT_VERTEX) {
    return base_ofs->vert_start;
  }
  if (elem_type == SCE_SELECT_EDGE) {
    return base_ofs->edge_start;
  }
  if (elem_type == SCE_SELECT_FACE) {
    return base_ofs->face_start;
  }
  BLI_assert(0);
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context
 * \{ */

void DRW_select_buffer_context_create(Base **bases, const uint bases_len, short select_mode)
{
  struct SELECTID_Context *select_ctx = DRW_select_engine_context_get();

  select_ctx->objects = MEM_reallocN(select_ctx->objects,
                                     sizeof(*select_ctx->objects) * bases_len);

  select_ctx->index_offsets = MEM_reallocN(select_ctx->index_offsets,
                                           sizeof(*select_ctx->index_offsets) * bases_len);

  select_ctx->objects_drawn = MEM_reallocN(select_ctx->objects_drawn,
                                           sizeof(*select_ctx->objects_drawn) * bases_len);

  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obj = bases[base_index]->object;
    select_ctx->objects[base_index] = obj;

    /* Weak but necessary for `DRW_select_buffer_elem_get`. */
    obj->runtime.select_id = base_index;
  }

  select_ctx->objects_len = bases_len;
  select_ctx->select_mode = select_mode;
  memset(select_ctx->persmat, 0, sizeof(select_ctx->persmat));
}
/** \} */
