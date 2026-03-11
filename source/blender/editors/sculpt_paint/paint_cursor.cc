/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "paint_cursor.hh"

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_axis_angle.hh"
#include "BLI_math_color.h"
#include "BLI_math_rotation.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_image.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_screen.hh"

#include "NOD_texture.h"

#include "WM_api.hh"
#include "wm_cursors.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "ED_image.hh"
#include "ED_view3d.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "UI_resources.hh"

#include "paint_intern.hh"

namespace blender {

/* TODOs:
 *
 * Some of the cursor drawing code is doing non-draw stuff
 * (e.g. updating the brush rake angle). This should be cleaned up
 * still.
 *
 * There is also some ugliness with sculpt-specific code.
 */

struct TexSnapshot {
  gpu::Texture *overlay_texture;
  int winx;
  int winy;
  int old_size;
  float old_zoom;
  bool old_col;
};

struct CursorSnapshot {
  gpu::Texture *overlay_texture;
  int size;
  int zoom;
  int curve_preset;
};

static TexSnapshot primary_snap = {nullptr};
static TexSnapshot secondary_snap = {nullptr};
static CursorSnapshot cursor_snap = {nullptr};

void paint_cursor_delete_textures()
{
  if (primary_snap.overlay_texture) {
    GPU_texture_free(primary_snap.overlay_texture);
  }
  if (secondary_snap.overlay_texture) {
    GPU_texture_free(secondary_snap.overlay_texture);
  }
  if (cursor_snap.overlay_texture) {
    GPU_texture_free(cursor_snap.overlay_texture);
  }

  memset(&primary_snap, 0, sizeof(TexSnapshot));
  memset(&secondary_snap, 0, sizeof(TexSnapshot));
  memset(&cursor_snap, 0, sizeof(CursorSnapshot));

  BKE_paint_invalidate_overlay_all();
}

namespace ed::sculpt_paint {

static int same_tex_snap(TexSnapshot *snap, MTex *mtex, ViewContext *vc, bool col, float zoom)
{
  return (/* make brush smaller shouldn't cause a resample */
          //(mtex->brush_map_mode != MTEX_MAP_MODE_VIEW ||
          //(BKE_brush_size_get(vc->scene, brush) <= snap->BKE_brush_size_get)) &&

          (mtex->brush_map_mode != MTEX_MAP_MODE_TILED ||
           (vc->region->winx == snap->winx && vc->region->winy == snap->winy)) &&
          (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL || snap->old_zoom == zoom) &&
          snap->old_col == col);
}

static void make_tex_snap(TexSnapshot *snap, ViewContext *vc, float zoom)
{
  snap->old_zoom = zoom;
  snap->winx = vc->region->winx;
  snap->winy = vc->region->winy;
}

struct LoadTexData {
  Brush *br;
  ViewContext *vc;

  MTex *mtex;
  uchar *buffer;
  bool col;

  ImagePool *pool;
  int size;
  float rotation;
  float radius;
};

static void load_tex_task_cb_ex(void *__restrict userdata,
                                const int j,
                                const TaskParallelTLS *__restrict tls)
{
  LoadTexData *data = static_cast<LoadTexData *>(userdata);
  Brush *br = data->br;
  ViewContext *vc = data->vc;

  MTex *mtex = data->mtex;
  uchar *buffer = data->buffer;
  const bool col = data->col;

  ImagePool *pool = data->pool;
  const int size = data->size;
  const float rotation = data->rotation;
  const float radius = data->radius;

  bool convert_to_linear = false;
  const ColorSpace *colorspace = nullptr;

  const int thread_id = BLI_task_parallel_thread_id(tls);

  if (mtex->tex && mtex->tex->type == TEX_IMAGE && mtex->tex->ima) {
    ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(mtex->tex->ima, &mtex->tex->iuser, pool);
    /* For consistency, sampling always returns color in linear space. */
    if (tex_ibuf && tex_ibuf->float_buffer.data == nullptr) {
      convert_to_linear = true;
      colorspace = tex_ibuf->byte_buffer.colorspace;
    }
    BKE_image_pool_release_ibuf(mtex->tex->ima, tex_ibuf, pool);
  }

  for (int i = 0; i < size; i++) {
    /* Largely duplicated from tex_strength. */

    int index = j * size + i;

    float x = float(i) / size;
    float y = float(j) / size;
    float len;

    if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      x *= vc->region->winx / radius;
      y *= vc->region->winy / radius;
    }
    else {
      x = (x - 0.5f) * 2.0f;
      y = (y - 0.5f) * 2.0f;
    }

    len = sqrtf(x * x + y * y);

    if (ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL) || len <= 1.0f) {
      /* It is probably worth optimizing for those cases where the texture is not rotated by
       * skipping the calls to atan2, sqrtf, sin, and cos. */
      if (mtex->tex && (rotation > 0.001f || rotation < -0.001f)) {
        const float angle = atan2f(y, x) + rotation;

        x = len * cosf(angle);
        y = len * sinf(angle);
      }

      float avg;
      float rgba[4];
      paint_get_tex_pixel(mtex, x, y, pool, thread_id, &avg, rgba);

      if (col) {
        if (convert_to_linear) {
          IMB_colormanagement_colorspace_to_scene_linear_v3(rgba, colorspace);
        }

        linearrgb_to_srgb_v3_v3(rgba, rgba);

        clamp_v4(rgba, 0.0f, 1.0f);

        buffer[index * 4] = rgba[0] * 255;
        buffer[index * 4 + 1] = rgba[1] * 255;
        buffer[index * 4 + 2] = rgba[2] * 255;
        buffer[index * 4 + 3] = rgba[3] * 255;
      }
      else {
        avg += br->texture_sample_bias;

        /* Clamp to avoid precision overflow. */
        CLAMP(avg, 0.0f, 1.0f);
        buffer[index] = 255 - uchar(255 * avg);
      }
    }
    else {
      if (col) {
        buffer[index * 4] = 0;
        buffer[index * 4 + 1] = 0;
        buffer[index * 4 + 2] = 0;
        buffer[index * 4 + 3] = 0;
      }
      else {
        buffer[index] = 0;
      }
    }
  }
}

static int load_tex(Paint *paint, Brush *br, ViewContext *vc, float zoom, bool col, bool primary)
{
  bool init;
  TexSnapshot *target;

  MTex *mtex = (primary) ? &br->mtex : &br->mask_mtex;
  ePaintOverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
  uchar *buffer = nullptr;

  int size;
  bool refresh;
  ePaintOverlayControlFlags invalid =
      ((primary) ? (overlay_flags & PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY) :
                   (overlay_flags & PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY));
  target = (primary) ? &primary_snap : &secondary_snap;

  refresh = !target->overlay_texture || (invalid != 0) ||
            !same_tex_snap(target, mtex, vc, col, zoom);

  init = (target->overlay_texture != nullptr);

  if (refresh) {
    ImagePool *pool = nullptr;
    /* Stencil is rotated later. */
    const float rotation = (mtex->brush_map_mode != MTEX_MAP_MODE_STENCIL) ? -mtex->rot : 0.0f;
    const float radius = BKE_brush_radius_get(paint, br) * zoom;

    make_tex_snap(target, vc, zoom);

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      int s = BKE_brush_radius_get(paint, br);
      int r = 1;

      for (s >>= 1; s > 0; s >>= 1) {
        r++;
      }

      size = (1 << r);

      size = std::max(size, 256);
      size = std::max(size, target->old_size);
    }
    else {
      size = 512;
    }

    if (target->old_size != size || target->old_col != col) {
      if (target->overlay_texture) {
        GPU_texture_free(target->overlay_texture);
        target->overlay_texture = nullptr;
      }
      init = false;

      target->old_size = size;
      target->old_col = col;
    }
    if (col) {
      buffer = MEM_new_array_uninitialized<uchar>(size * size * 4, "load_tex");
    }
    else {
      buffer = MEM_new_array_uninitialized<uchar>(size * size, "load_tex");
    }

    pool = BKE_image_pool_new();

    if (mtex->tex && mtex->tex->nodetree) {
      /* Has internal flag to detect it only does it once. */
      ntreeTexBeginExecTree(mtex->tex->nodetree);
    }

    LoadTexData data{};
    data.br = br;
    data.vc = vc;
    data.mtex = mtex;
    data.buffer = buffer;
    data.col = col;
    data.pool = pool;
    data.size = size;
    data.rotation = rotation;
    data.radius = radius;

    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    BLI_task_parallel_range(0, size, &data, load_tex_task_cb_ex, &settings);

    if (mtex->tex && mtex->tex->nodetree) {
      ntreeTexEndExecTree(mtex->tex->nodetree->runtime->execdata);
    }

    if (pool) {
      BKE_image_pool_free(pool);
    }

    if (!target->overlay_texture) {
      gpu::TextureFormat format = col ? gpu::TextureFormat::UNORM_8_8_8_8 :
                                        gpu::TextureFormat::UNORM_8;
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
      target->overlay_texture = GPU_texture_create_2d(
          "paint_cursor_overlay", size, size, 1, format, usage, nullptr);
      GPU_texture_update(target->overlay_texture, GPU_DATA_UBYTE, buffer);

      if (!col) {
        GPU_texture_swizzle_set(target->overlay_texture, "rrrr");
      }
    }

    if (init) {
      GPU_texture_update(target->overlay_texture, GPU_DATA_UBYTE, buffer);
    }

    if (buffer) {
      MEM_delete(buffer);
    }
  }
  else {
    size = target->old_size;
  }

  BKE_paint_reset_overlay_invalid(invalid);

  return 1;
}

static void load_tex_cursor_task_cb(void *__restrict userdata,
                                    const int j,
                                    const TaskParallelTLS *__restrict /*tls*/)
{
  LoadTexData *data = static_cast<LoadTexData *>(userdata);
  Brush *br = data->br;

  uchar *buffer = data->buffer;

  const int size = data->size;

  for (int i = 0; i < size; i++) {
    /* Largely duplicated from tex_strength. */

    const int index = j * size + i;
    const float x = ((float(i) / size) - 0.5f) * 2.0f;
    const float y = ((float(j) / size) - 0.5f) * 2.0f;
    const float len = sqrtf(x * x + y * y);

    if (len <= 1.0f) {

      /* Falloff curve. */
      float avg = BKE_brush_curve_strength_clamped(br, len, 1.0f);

      buffer[index] = uchar(255 * avg);
    }
    else {
      buffer[index] = 0;
    }
  }
}

static int load_tex_cursor(Paint *paint, Brush *br, float zoom)
{
  bool init;

  ePaintOverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
  uchar *buffer = nullptr;

  int size;
  const bool refresh = !cursor_snap.overlay_texture ||
                       (overlay_flags & PAINT_OVERLAY_INVALID_CURVE) || cursor_snap.zoom != zoom ||
                       cursor_snap.curve_preset != br->curve_distance_falloff_preset;

  init = (cursor_snap.overlay_texture != nullptr);

  if (refresh) {
    int s, r;

    cursor_snap.zoom = zoom;

    s = BKE_brush_radius_get(paint, br);
    r = 1;

    for (s >>= 1; s > 0; s >>= 1) {
      r++;
    }

    size = (1 << r);

    size = std::max(size, 256);
    size = std::max(size, cursor_snap.size);

    if (cursor_snap.size != size) {
      if (cursor_snap.overlay_texture) {
        GPU_texture_free(cursor_snap.overlay_texture);
        cursor_snap.overlay_texture = nullptr;
      }

      init = false;

      cursor_snap.size = size;
    }
    buffer = MEM_new_array_uninitialized<uchar>(size * size, "load_tex");

    BKE_curvemapping_init(br->curve_distance_falloff);

    LoadTexData data{};
    data.br = br;
    data.buffer = buffer;
    data.size = size;

    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    BLI_task_parallel_range(0, size, &data, load_tex_cursor_task_cb, &settings);

    if (!cursor_snap.overlay_texture) {
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
      cursor_snap.overlay_texture = GPU_texture_create_2d(
          "cursor_snap_overaly", size, size, 1, gpu::TextureFormat::UNORM_8, usage, nullptr);
      GPU_texture_update(cursor_snap.overlay_texture, GPU_DATA_UBYTE, buffer);

      GPU_texture_swizzle_set(cursor_snap.overlay_texture, "rrrr");
    }

    if (init) {
      GPU_texture_update(cursor_snap.overlay_texture, GPU_DATA_UBYTE, buffer);
    }

    if (buffer) {
      MEM_delete(buffer);
    }
  }
  else {
    size = cursor_snap.size;
  }

  cursor_snap.curve_preset = br->curve_distance_falloff_preset;
  BKE_paint_reset_overlay_invalid(PAINT_OVERLAY_INVALID_CURVE);

  return 1;
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength. */
static bool paint_draw_tex_overlay(Paint *paint,
                                   Brush *brush,
                                   ViewContext *vc,
                                   int x,
                                   int y,
                                   float zoom,
                                   const PaintMode mode,
                                   bool col,
                                   bool primary)
{
  rctf quad;
  /* Check for overlay mode. */

  MTex *mtex = (primary) ? &brush->mtex : &brush->mask_mtex;
  bool valid = ((primary) ? (brush->overlay_flags & BRUSH_OVERLAY_PRIMARY) != 0 :
                            (brush->overlay_flags & BRUSH_OVERLAY_SECONDARY) != 0);
  int overlay_alpha = (primary) ? brush->texture_overlay_alpha : brush->mask_overlay_alpha;

  if (mode == PaintMode::Texture3D) {
    if (primary && brush->image_brush_type != IMAGE_PAINT_BRUSH_TYPE_DRAW) {
      /* All non-draw tools don't use the primary texture (clone, smear, soften.. etc). */
      return false;
    }
  }

  if (!(mtex->tex) ||
      !((mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) ||
        (valid && ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_TILED))))
  {
    return false;
  }

  bke::PaintRuntime *paint_runtime = paint->runtime;
  if (load_tex(paint, brush, vc, zoom, col, primary)) {
    GPU_color_mask(true, true, true, true);
    GPU_depth_test(GPU_DEPTH_NONE);

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      GPU_matrix_push();

      float center[2] = {
          paint_runtime->draw_anchored ? paint_runtime->anchored_initial_mouse[0] : x,
          paint_runtime->draw_anchored ? paint_runtime->anchored_initial_mouse[1] : y,
      };

      /* Brush rotation. */
      GPU_matrix_translate_2fv(center);
      GPU_matrix_rotate_2d(
          RAD2DEGF(primary ? paint_runtime->brush_rotation : paint_runtime->brush_rotation_sec));
      GPU_matrix_translate_2f(-center[0], -center[1]);

      /* Scale based on tablet pressure. */
      if (primary && paint_runtime->stroke_active && BKE_brush_use_size_pressure(brush)) {
        const float scale = paint_runtime->size_pressure_value;
        GPU_matrix_translate_2fv(center);
        GPU_matrix_scale_2f(scale, scale);
        GPU_matrix_translate_2f(-center[0], -center[1]);
      }

      if (paint_runtime->draw_anchored) {
        quad.xmin = center[0] - paint_runtime->anchored_size;
        quad.ymin = center[1] - paint_runtime->anchored_size;
        quad.xmax = center[0] + paint_runtime->anchored_size;
        quad.ymax = center[1] + paint_runtime->anchored_size;
      }
      else {
        const int radius = BKE_brush_radius_get(paint, brush) * zoom;
        quad.xmin = center[0] - radius;
        quad.ymin = center[1] - radius;
        quad.xmax = center[0] + radius;
        quad.ymax = center[1] + radius;
      }
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      quad.xmin = 0;
      quad.ymin = 0;
      quad.xmax = BLI_rcti_size_x(&vc->region->winrct);
      quad.ymax = BLI_rcti_size_y(&vc->region->winrct);
    }
    /* Stencil code goes here. */
    else {
      if (primary) {
        quad.xmin = -brush->stencil_dimension[0];
        quad.ymin = -brush->stencil_dimension[1];
        quad.xmax = brush->stencil_dimension[0];
        quad.ymax = brush->stencil_dimension[1];
      }
      else {
        quad.xmin = -brush->mask_stencil_dimension[0];
        quad.ymin = -brush->mask_stencil_dimension[1];
        quad.xmax = brush->mask_stencil_dimension[0];
        quad.ymax = brush->mask_stencil_dimension[1];
      }
      GPU_matrix_push();
      if (primary) {
        GPU_matrix_translate_2fv(brush->stencil_pos);
      }
      else {
        GPU_matrix_translate_2fv(brush->mask_stencil_pos);
      }
      GPU_matrix_rotate_2d(RAD2DEGF(mtex->rot));
    }

    /* Set quad color. Colored overlay does not get blending. */
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

    /* Premultiplied alpha blending. */
    GPU_blend(GPU_BLEND_ALPHA_PREMULT);

    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);

    float final_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (!col) {
      copy_v3_v3(final_color, U.sculpt_paint_overlay_col);
    }
    mul_v4_fl(final_color, overlay_alpha * 0.01f);
    immUniformColor4fv(final_color);

    gpu::Texture *texture = (primary) ? primary_snap.overlay_texture :
                                        secondary_snap.overlay_texture;

    GPUSamplerExtendMode extend_mode = (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) ?
                                           GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER :
                                           GPU_SAMPLER_EXTEND_MODE_REPEAT;
    immBindTextureSampler(
        "image", texture, {GPU_SAMPLER_FILTERING_LINEAR, extend_mode, extend_mode});

    /* Draw textured quad. */
    immBegin(GPU_PRIM_TRI_FAN, 4);
    immAttr2f(texCoord, 0.0f, 0.0f);
    immVertex2f(pos, quad.xmin, quad.ymin);
    immAttr2f(texCoord, 1.0f, 0.0f);
    immVertex2f(pos, quad.xmax, quad.ymin);
    immAttr2f(texCoord, 1.0f, 1.0f);
    immVertex2f(pos, quad.xmax, quad.ymax);
    immAttr2f(texCoord, 0.0f, 1.0f);
    immVertex2f(pos, quad.xmin, quad.ymax);
    immEnd();

    immUnbindProgram();

    GPU_texture_unbind(texture);

    if (ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_VIEW)) {
      GPU_matrix_pop();
    }
  }
  return true;
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength. */
static bool paint_draw_cursor_overlay(Paint *paint, Brush *brush, int x, int y, float zoom)
{
  rctf quad;
  /* Check for overlay mode. */

  if (!(brush->overlay_flags & BRUSH_OVERLAY_CURSOR)) {
    return false;
  }

  if (load_tex_cursor(paint, brush, zoom)) {
    bool do_pop = false;
    float center[2];

    GPU_color_mask(true, true, true, true);
    GPU_depth_test(GPU_DEPTH_NONE);

    bke::PaintRuntime *paint_runtime = paint->runtime;
    if (paint_runtime->draw_anchored) {
      copy_v2_v2(center, paint_runtime->anchored_initial_mouse);
      quad.xmin = paint_runtime->anchored_initial_mouse[0] - paint_runtime->anchored_size;
      quad.ymin = paint_runtime->anchored_initial_mouse[1] - paint_runtime->anchored_size;
      quad.xmax = paint_runtime->anchored_initial_mouse[0] + paint_runtime->anchored_size;
      quad.ymax = paint_runtime->anchored_initial_mouse[1] + paint_runtime->anchored_size;
    }
    else {
      const int radius = BKE_brush_radius_get(paint, brush) * zoom;
      center[0] = x;
      center[1] = y;

      quad.xmin = x - radius;
      quad.ymin = y - radius;
      quad.xmax = x + radius;
      quad.ymax = y + radius;
    }

    /* Scale based on tablet pressure. */
    if (paint_runtime->stroke_active && BKE_brush_use_size_pressure(brush)) {
      do_pop = true;
      GPU_matrix_push();
      GPU_matrix_translate_2fv(center);
      GPU_matrix_scale_1f(paint_runtime->size_pressure_value);
      GPU_matrix_translate_2f(-center[0], -center[1]);
    }

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

    GPU_blend(GPU_BLEND_ALPHA_PREMULT);

    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);

    float final_color[4] = {UNPACK3(U.sculpt_paint_overlay_col), 1.0f};
    mul_v4_fl(final_color, brush->cursor_overlay_alpha * 0.01f);
    immUniformColor4fv(final_color);

    /* Draw textured quad. */
    immBindTextureSampler("image",
                          cursor_snap.overlay_texture,
                          {GPU_SAMPLER_FILTERING_LINEAR,
                           GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER,
                           GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER});

    immBegin(GPU_PRIM_TRI_FAN, 4);
    immAttr2f(texCoord, 0.0f, 0.0f);
    immVertex2f(pos, quad.xmin, quad.ymin);
    immAttr2f(texCoord, 1.0f, 0.0f);
    immVertex2f(pos, quad.xmax, quad.ymin);
    immAttr2f(texCoord, 1.0f, 1.0f);
    immVertex2f(pos, quad.xmax, quad.ymax);
    immAttr2f(texCoord, 0.0f, 1.0f);
    immVertex2f(pos, quad.xmin, quad.ymax);
    immEnd();

    GPU_texture_unbind(cursor_snap.overlay_texture);

    immUnbindProgram();

    if (do_pop) {
      GPU_matrix_pop();
    }
  }
  return true;
}

static bool paint_draw_alpha_overlay(
    Paint *paint, Brush *brush, ViewContext *vc, int x, int y, float zoom, PaintMode mode)
{
  /* Color means that primary brush texture is colored and
   * secondary is used for alpha/mask control. */
  bool col = ELEM(mode, PaintMode::Texture3D, PaintMode::Texture2D, PaintMode::Vertex);

  bool alpha_overlay_active = false;

  ePaintOverlayControlFlags flags = BKE_paint_get_overlay_flags();
  GPUBlend blend_state = GPU_blend_get();
  GPUDepthTest depth_test = GPU_depth_test_get();

  /* Translate to region. */
  GPU_matrix_push();
  GPU_matrix_translate_2f(vc->region->winrct.xmin, vc->region->winrct.ymin);
  x -= vc->region->winrct.xmin;
  y -= vc->region->winrct.ymin;

  /* Colored overlay should be drawn separately. */
  if (col) {
    if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY)) {
      alpha_overlay_active = paint_draw_tex_overlay(
          paint, brush, vc, x, y, zoom, mode, true, true);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_SECONDARY)) {
      alpha_overlay_active = paint_draw_tex_overlay(
          paint, brush, vc, x, y, zoom, mode, false, false);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR)) {
      alpha_overlay_active = paint_draw_cursor_overlay(paint, brush, x, y, zoom);
    }
  }
  else {
    if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY) && (mode != PaintMode::Weight)) {
      alpha_overlay_active = paint_draw_tex_overlay(
          paint, brush, vc, x, y, zoom, mode, false, true);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR)) {
      alpha_overlay_active = paint_draw_cursor_overlay(paint, brush, x, y, zoom);
    }
  }

  GPU_matrix_pop();
  GPU_blend(blend_state);
  GPU_depth_test(depth_test);

  return alpha_overlay_active;
}

BLI_INLINE void draw_tri_point(uint pos,
                               const float sel_col[4],
                               const float pivot_col[4],
                               float *co,
                               float width,
                               bool selected)
{
  immUniformColor4fv(selected ? sel_col : pivot_col);

  GPU_line_width(3.0f);

  float w = width / 2.0f;
  const float tri[3][2] = {
      {co[0], co[1] + w},
      {co[0] - w, co[1] - w},
      {co[0] + w, co[1] - w},
  };

  immBegin(GPU_PRIM_LINE_LOOP, 3);
  immVertex2fv(pos, tri[0]);
  immVertex2fv(pos, tri[1]);
  immVertex2fv(pos, tri[2]);
  immEnd();

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  GPU_line_width(1.0f);

  immBegin(GPU_PRIM_LINE_LOOP, 3);
  immVertex2fv(pos, tri[0]);
  immVertex2fv(pos, tri[1]);
  immVertex2fv(pos, tri[2]);
  immEnd();
}

BLI_INLINE void draw_rect_point(uint pos,
                                const float sel_col[4],
                                const float handle_col[4],
                                const float *co,
                                float width,
                                bool selected)
{
  immUniformColor4fv(selected ? sel_col : handle_col);

  GPU_line_width(3.0f);

  float w = width / 2.0f;
  float minx = co[0] - w;
  float miny = co[1] - w;
  float maxx = co[0] + w;
  float maxy = co[1] + w;

  imm_draw_box_wire_2d(pos, minx, miny, maxx, maxy);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  GPU_line_width(1.0f);

  imm_draw_box_wire_2d(pos, minx, miny, maxx, maxy);
}

BLI_INLINE void draw_bezier_handle_lines(uint pos, const float sel_col[4], BezTriple *bez)
{
  immUniformColor4f(0.0f, 0.0f, 0.0f, 0.5f);
  GPU_line_width(3.0f);

  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2fv(pos, bez->vec[0]);
  immVertex2fv(pos, bez->vec[1]);
  immVertex2fv(pos, bez->vec[2]);
  immEnd();

  GPU_line_width(1.0f);

  if (bez->f1 || bez->f2) {
    immUniformColor4fv(sel_col);
  }
  else {
    immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  }
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2fv(pos, bez->vec[0]);
  immVertex2fv(pos, bez->vec[1]);
  immEnd();

  if (bez->f3 || bez->f2) {
    immUniformColor4fv(sel_col);
  }
  else {
    immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
  }
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2fv(pos, bez->vec[1]);
  immVertex2fv(pos, bez->vec[2]);
  immEnd();
}

static void paint_draw_curve_cursor(Brush *brush, ViewContext *vc)
{
  GPU_matrix_push();
  GPU_matrix_translate_2f(vc->region->winrct.xmin, vc->region->winrct.ymin);

  if (brush->paint_curve && brush->paint_curve->points) {
    PaintCurve *pc = brush->paint_curve;
    PaintCurvePoint *cp = pc->points;

    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    /* Draw the bezier handles and the curve segment between the current and next point. */
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    float selec_col[4], handle_col[4], pivot_col[4];
    ui::theme::get_color_type_4fv(TH_VERTEX_SELECT, SPACE_VIEW3D, selec_col);
    ui::theme::get_color_type_4fv(TH_GIZMO_PRIMARY, SPACE_VIEW3D, handle_col);
    ui::theme::get_color_type_4fv(TH_GIZMO_SECONDARY, SPACE_VIEW3D, pivot_col);

    for (int i = 0; i < pc->tot_points - 1; i++, cp++) {
      int j;
      PaintCurvePoint *cp_next = cp + 1;
      float data[(PAINT_CURVE_NUM_SEGMENTS + 1) * 2];
      /* Use color coding to distinguish handles vs curve segments. */
      draw_bezier_handle_lines(pos, selec_col, &cp->bez);
      draw_tri_point(pos, selec_col, pivot_col, &cp->bez.vec[1][0], 10.0f, cp->bez.f2);
      draw_rect_point(
          pos, selec_col, handle_col, &cp->bez.vec[0][0], 8.0f, cp->bez.f1 || cp->bez.f2);
      draw_rect_point(
          pos, selec_col, handle_col, &cp->bez.vec[2][0], 8.0f, cp->bez.f3 || cp->bez.f2);

      for (j = 0; j < 2; j++) {
        BKE_curve_forward_diff_bezier(cp->bez.vec[1][j],
                                      cp->bez.vec[2][j],
                                      cp_next->bez.vec[0][j],
                                      cp_next->bez.vec[1][j],
                                      data + j,
                                      PAINT_CURVE_NUM_SEGMENTS,
                                      sizeof(float[2]));
      }

      float (*v)[2] = reinterpret_cast<float (*)[2]>(data);

      immUniformColor4f(0.0f, 0.0f, 0.0f, 0.5f);
      GPU_line_width(3.0f);
      immBegin(GPU_PRIM_LINE_STRIP, PAINT_CURVE_NUM_SEGMENTS + 1);
      for (j = 0; j <= PAINT_CURVE_NUM_SEGMENTS; j++) {
        immVertex2fv(pos, v[j]);
      }
      immEnd();

      immUniformColor4f(0.9f, 0.9f, 1.0f, 0.5f);
      GPU_line_width(1.0f);
      immBegin(GPU_PRIM_LINE_STRIP, PAINT_CURVE_NUM_SEGMENTS + 1);
      for (j = 0; j <= PAINT_CURVE_NUM_SEGMENTS; j++) {
        immVertex2fv(pos, v[j]);
      }
      immEnd();
    }

    /* Draw last line segment. */
    draw_bezier_handle_lines(pos, selec_col, &cp->bez);
    draw_tri_point(pos, selec_col, pivot_col, &cp->bez.vec[1][0], 10.0f, cp->bez.f2);
    draw_rect_point(
        pos, selec_col, handle_col, &cp->bez.vec[0][0], 8.0f, cp->bez.f1 || cp->bez.f2);
    draw_rect_point(
        pos, selec_col, handle_col, &cp->bez.vec[2][0], 8.0f, cp->bez.f3 || cp->bez.f2);

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);

    immUnbindProgram();
  }
  GPU_matrix_pop();
}

static bool paint_use_2d_cursor(PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
    case PaintMode::Vertex:
    case PaintMode::Weight:
      return false;
    case PaintMode::Texture3D:
    case PaintMode::Texture2D:
    case PaintMode::VertexGPencil:
    case PaintMode::SculptGPencil:
    case PaintMode::WeightGPencil:
    case PaintMode::SculptCurves:
    case PaintMode::GPencil:
      return true;
    case PaintMode::Invalid:
      BLI_assert_unreachable();
  }
  return true;
}

static bool paint_cursor_context_init(bContext *C,
                                      const int2 &xy,
                                      const float2 &tilt,
                                      PaintCursorContext &pcontext)
{
  ARegion *region = CTX_wm_region(C);
  if (region && region->regiontype != RGN_TYPE_WINDOW) {
    return false;
  }

  pcontext.region = region;
  pcontext.wm = CTX_wm_manager(C);
  pcontext.win = CTX_wm_window(C);
  pcontext.screen = CTX_wm_screen(C);
  pcontext.depsgraph = CTX_data_depsgraph_pointer(C);
  pcontext.scene = CTX_data_scene(C);
  pcontext.object = CTX_data_active_object(C);
  pcontext.paint = BKE_paint_get_active_from_context(C);
  if (pcontext.paint == nullptr) {
    return false;
  }
  pcontext.ups = &pcontext.paint->unified_paint_settings;
  pcontext.brush = BKE_paint_brush(pcontext.paint);
  if (pcontext.brush == nullptr) {
    return false;
  }
  pcontext.mode = BKE_paintmode_get_active_from_context(C);
  if (pcontext.mode == PaintMode::Sculpt) {
    pcontext.sd = CTX_data_tool_settings(C)->sculpt;
  }

  if (ELEM(pcontext.mode,
           PaintMode::Sculpt,
           PaintMode::Vertex,
           PaintMode::Weight,
           PaintMode::Texture3D))
  {
    pcontext.base = CTX_data_active_base(C);
  }

  pcontext.vc = ED_view3d_viewcontext_init(C, pcontext.depsgraph);

  if (pcontext.brush->stroke_method == BRUSH_STROKE_CURVE) {
    pcontext.cursor_type = PaintCursorDrawingType::Curve;
  }
  else if (paint_use_2d_cursor(pcontext.mode)) {
    pcontext.cursor_type = PaintCursorDrawingType::Cursor2D;
  }
  else {
    pcontext.cursor_type = PaintCursorDrawingType::Cursor3D;
  }

  pcontext.mval = xy;
  pcontext.translation = {float(xy[0]), float(xy[1])};
  pcontext.tilt = tilt;

  float zoomx, zoomy;
  get_imapaint_zoom(C, &zoomx, &zoomy);
  pcontext.zoomx = max_ff(zoomx, zoomy);
  pcontext.final_radius = (BKE_brush_radius_get(pcontext.paint, pcontext.brush) * zoomx);

  const bke::PaintRuntime &paint_runtime = *pcontext.paint->runtime;
  /* There is currently no way to check if the direction is inverted before starting the stroke,
   * so this does not reflect the state of the brush in the UI. */
  if (((!paint_runtime.draw_inverted) ^ ((pcontext.brush->flag & BRUSH_DIR_IN) == 0)) &&
      bke::brush::supports_secondary_cursor_color(*pcontext.brush))
  {
    pcontext.outline_col = float3(pcontext.brush->sub_col);
  }
  else {
    pcontext.outline_col = float3(pcontext.brush->add_col);
  }
  pcontext.outline_alpha = pcontext.brush->add_col[3];

  Object *active_object = pcontext.vc.obact;
  pcontext.ss = active_object ? active_object->runtime->sculpt_session : nullptr;

  if (pcontext.ss && pcontext.ss->draw_faded_cursor) {
    pcontext.outline_alpha = 0.3f;
    pcontext.outline_col = float3(0.8f);
  }

  const ScrArea *area = CTX_wm_area(C);
  pcontext.is_brush_active = paint_brush_tool_poll(area, region, pcontext.paint, pcontext.object);
  if (!pcontext.is_brush_active) {
    /* Use a default color for tools that are not brushes. */
    pcontext.outline_alpha = 0.8f;
    pcontext.outline_col = float3(0.8f);
  }

  pcontext.is_stroke_active = paint_runtime.stroke_active;

  return true;
}

static void paint_update_mouse_cursor(PaintCursorContext &pcontext)
{
  if (pcontext.win->grabcursor != 0 || pcontext.win->modalcursor != 0) {
    /* Don't set the cursor while it's grabbed, since this will show the cursor when interacting
     * with the UI (dragging a number button for example), see: #102792.
     * And don't overwrite a modal cursor, allowing modal operators to set a cursor temporarily. */
    return;
  }

  /* Don't set the cursor when a temporary popup is opened (e.g. a context menu, pie menu or
   * dialog), see: #137386. */
  if (!BLI_listbase_is_empty(&pcontext.screen->regionbase) &&
      (BKE_screen_find_region_type(pcontext.screen, RGN_TYPE_TEMPORARY) != nullptr))
  {
    return;
  }

  if (ELEM(pcontext.mode, PaintMode::GPencil, PaintMode::VertexGPencil)) {
    WM_cursor_set(pcontext.win, WM_CURSOR_DOT);
  }
  else {
    /* Don't use paint cursor when overlapping with the size circle. */
    const int brush_size = BKE_brush_size_get(pcontext.paint, pcontext.brush);
    const bool small = brush_size < 28 && brush_size > 12;
    WM_cursor_set(pcontext.win, small ? WM_CURSOR_DOT : WM_CURSOR_PAINT);
  }
}

static void paint_draw_2D_view_brush_cursor_default(PaintCursorContext &pcontext)
{
  immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha);
  const bke::PaintRuntime *paint_runtime = pcontext.paint->runtime;

  /* Draw brush outline. */
  if (paint_runtime->stroke_active && BKE_brush_use_size_pressure(pcontext.brush)) {
    imm_draw_circle_wire_2d(pcontext.pos,
                            pcontext.translation[0],
                            pcontext.translation[1],
                            pcontext.final_radius * paint_runtime->size_pressure_value,
                            40);
    /* Outer at half alpha. */
    immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha * 0.5f);
  }

  GPU_line_width(1.0f);
  imm_draw_circle_wire_2d(
      pcontext.pos, pcontext.translation[0], pcontext.translation[1], pcontext.final_radius, 40);
}

static void paint_draw_2D_view_brush_cursor(PaintCursorContext &pcontext)
{
  switch (pcontext.mode) {
    case PaintMode::GPencil:
    case PaintMode::VertexGPencil:
      grease_pencil_cursor_draw(pcontext);
      break;
    default:
      paint_draw_2D_view_brush_cursor_default(pcontext);
  }
}

static void paint_draw_legacy_3D_view_brush_cursor(PaintCursorContext &pcontext)
{
  GPU_line_width(1.0f);
  immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha);
  imm_draw_circle_wire_3d(
      pcontext.pos, pcontext.translation[0], pcontext.translation[1], pcontext.final_radius, 40);
}

static void paint_cursor_draw_3D_view_brush_cursor(PaintCursorContext &pcontext)
{
  BLI_assert(ELEM(pcontext.mode,
                  PaintMode::Sculpt,
                  PaintMode::Vertex,
                  PaintMode::Weight,
                  PaintMode::Texture3D));
  /* These paint tools are not using the SculptSession, so they need to use the default 2D brush
   * cursor in the 3D view. */
  if (pcontext.mode == PaintMode::Texture3D) {
    paint_draw_legacy_3D_view_brush_cursor(pcontext);
    return;
  }

  BLI_assert(pcontext.ss);

  mesh_cursor_update_and_init(pcontext);

  if (pcontext.is_stroke_active) {
    mesh_cursor_active_draw(pcontext);
  }
  else {
    const Brush &brush = *pcontext.brush;
    /* 2D falloff is better represented with the default 2D cursor,
     * there is no need to draw anything else. */
    if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      paint_draw_legacy_3D_view_brush_cursor(pcontext);
      return;
    }
    if (pcontext.alpha_overlay_drawn) {
      paint_draw_legacy_3D_view_brush_cursor(pcontext);
      return;
    }

    mesh_cursor_inactive_draw(pcontext);
  }
}

static bool paint_cursor_is_3d_view_navigating(const PaintCursorContext &pcontext)
{
  const ViewContext *vc = &pcontext.vc;
  return vc->rv3d && (vc->rv3d->rflag & RV3D_NAVIGATING);
}

static bool paint_cursor_is_brush_cursor_enabled(const PaintCursorContext &pcontext)
{
  if (pcontext.paint->flags & PAINT_SHOW_BRUSH) {
    if (ELEM(pcontext.mode, PaintMode::Texture2D, PaintMode::Texture3D) &&
        pcontext.brush->image_brush_type == IMAGE_PAINT_BRUSH_TYPE_FILL)
    {
      return false;
    }
    return true;
  }
  return false;
}

static void paint_cursor_update_rake_rotation(PaintCursorContext &pcontext)
{
  /* Don't calculate rake angles while a stroke is active because the rake variables are global
   * and we may get interference with the stroke itself.
   * For line strokes, such interference is visible. */
  const bke::PaintRuntime *paint_runtime = pcontext.paint->runtime;
  if (!paint_runtime->stroke_active) {
    paint_calculate_rake_rotation(
        *pcontext.paint, *pcontext.brush, pcontext.translation, pcontext.mode, true);
  }
}

static void paint_cursor_check_and_draw_alpha_overlays(PaintCursorContext &pcontext)
{
  pcontext.alpha_overlay_drawn = pcontext.is_brush_active &&
                                 paint_draw_alpha_overlay(pcontext.paint,
                                                          pcontext.brush,
                                                          &pcontext.vc,
                                                          pcontext.mval.x,
                                                          pcontext.mval.y,
                                                          pcontext.zoomx,
                                                          pcontext.mode);
}

static void paint_cursor_update_anchored_location(PaintCursorContext &pcontext)
{
  bke::PaintRuntime *paint_runtime = pcontext.paint->runtime;
  if (paint_runtime->draw_anchored) {
    pcontext.final_radius = paint_runtime->anchored_size;
    pcontext.translation = {
        paint_runtime->anchored_initial_mouse[0] + pcontext.region->winrct.xmin,
        paint_runtime->anchored_initial_mouse[1] + pcontext.region->winrct.ymin};
  }
}

static void paint_cursor_setup_2D_drawing(PaintCursorContext &pcontext)
{
  GPU_line_width(2.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  pcontext.pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
}

static void paint_cursor_setup_3D_drawing(PaintCursorContext &pcontext)
{
  GPU_line_width(2.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  pcontext.pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
}

static void paint_cursor_restore_drawing_state()
{
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

static void paint_draw_cursor(bContext *C, const int2 &xy, const float2 &tilt, void * /*unused*/)
{
  PaintCursorContext pcontext;
  if (!paint_cursor_context_init(C, xy, tilt, pcontext)) {
    return;
  }

  if (!paint_cursor_is_brush_cursor_enabled(pcontext)) {
    /* For Grease Pencil draw mode, we want to we only render a small mouse cursor (dot) if the
     * paint cursor is disabled so that the default mouse cursor doesn't get in the way of tablet
     * users. See #130089. But don't overwrite a modal cursor, allowing modal operators to set one
     * temporarily. */
    if (pcontext.mode == PaintMode::GPencil && pcontext.win->modalcursor == 0) {
      WM_cursor_set(pcontext.win, WM_CURSOR_DOT);
    }
    return;
  }
  if (paint_cursor_is_3d_view_navigating(pcontext)) {
    /* Still draw stencil while navigating. */
    paint_cursor_check_and_draw_alpha_overlays(pcontext);
    return;
  }

  switch (pcontext.cursor_type) {
    case PaintCursorDrawingType::Curve:
      paint_draw_curve_cursor(pcontext.brush, &pcontext.vc);
      break;
    case PaintCursorDrawingType::Cursor2D:
      paint_update_mouse_cursor(pcontext);

      paint_cursor_update_rake_rotation(pcontext);
      paint_cursor_check_and_draw_alpha_overlays(pcontext);
      paint_cursor_update_anchored_location(pcontext);

      paint_cursor_setup_2D_drawing(pcontext);
      paint_draw_2D_view_brush_cursor(pcontext);
      paint_cursor_restore_drawing_state();
      break;
    case PaintCursorDrawingType::Cursor3D:
      paint_update_mouse_cursor(pcontext);

      paint_cursor_update_rake_rotation(pcontext);
      paint_cursor_check_and_draw_alpha_overlays(pcontext);
      paint_cursor_update_anchored_location(pcontext);

      paint_cursor_setup_3D_drawing(pcontext);
      paint_cursor_draw_3D_view_brush_cursor(pcontext);
      paint_cursor_restore_drawing_state();
      break;
    default:
      BLI_assert_unreachable();
  }
}

}  // namespace ed::sculpt_paint

/* Public API */

void ED_paint_cursor_start(Paint *paint, bool (*poll)(bContext *C))
{
  if (paint && paint->runtime && !paint->runtime->paint_cursor) {
    paint->runtime->paint_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, poll, ed::sculpt_paint::paint_draw_cursor, nullptr);
  }

  /* Invalidate the paint cursors. */
  BKE_paint_invalidate_overlay_all();
}

}  // namespace blender
