/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 by Nicholas Bishop. All rights reserved. */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_image.h"
#include "BKE_node_runtime.hh"
#include "BKE_object.h"
#include "BKE_paint.h"

#include "NOD_texture.h"

#include "WM_api.h"
#include "wm_cursors.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "ED_image.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "UI_resources.h"

#include "paint_intern.hh"
/* still needed for sculpt_stroke_get_location, should be
 * removed eventually (TODO) */
#include "sculpt_intern.hh"

/* TODOs:
 *
 * Some of the cursor drawing code is doing non-draw stuff
 * (e.g. updating the brush rake angle). This should be cleaned up
 * still.
 *
 * There is also some ugliness with sculpt-specific code.
 */

struct TexSnapshot {
  GPUTexture *overlay_texture;
  int winx;
  int winy;
  int old_size;
  float old_zoom;
  bool old_col;
};

struct CursorSnapshot {
  GPUTexture *overlay_texture;
  int size;
  int zoom;
  int curve_preset;
};

static TexSnapshot primary_snap = {nullptr};
static TexSnapshot secondary_snap = {nullptr};
static CursorSnapshot cursor_snap = {nullptr};

void paint_cursor_delete_textures(void)
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
  ColorSpace *colorspace = nullptr;

  const int thread_id = BLI_task_parallel_thread_id(tls);

  if (mtex->tex && mtex->tex->type == TEX_IMAGE && mtex->tex->ima) {
    ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(mtex->tex->ima, &mtex->tex->iuser, pool);
    /* For consistency, sampling always returns color in linear space. */
    if (tex_ibuf && tex_ibuf->rect_float == nullptr) {
      convert_to_linear = true;
      colorspace = tex_ibuf->rect_colorspace;
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

static int load_tex(Brush *br, ViewContext *vc, float zoom, bool col, bool primary)
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
    const float radius = BKE_brush_size_get(vc->scene, br) * zoom;

    make_tex_snap(target, vc, zoom);

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      int s = BKE_brush_size_get(vc->scene, br);
      int r = 1;

      for (s >>= 1; s > 0; s >>= 1) {
        r++;
      }

      size = (1 << r);

      if (size < 256) {
        size = 256;
      }

      if (size < target->old_size) {
        size = target->old_size;
      }
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
      buffer = static_cast<uchar *>(MEM_mallocN(sizeof(uchar) * size * size * 4, "load_tex"));
    }
    else {
      buffer = static_cast<uchar *>(MEM_mallocN(sizeof(uchar) * size * size, "load_tex"));
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
      eGPUTextureFormat format = col ? GPU_RGBA8 : GPU_R8;
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                               GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW;
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
      MEM_freeN(buffer);
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

static int load_tex_cursor(Brush *br, ViewContext *vc, float zoom)
{
  bool init;

  ePaintOverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
  uchar *buffer = nullptr;

  int size;
  const bool refresh = !cursor_snap.overlay_texture ||
                       (overlay_flags & PAINT_OVERLAY_INVALID_CURVE) || cursor_snap.zoom != zoom ||
                       cursor_snap.curve_preset != br->curve_preset;

  init = (cursor_snap.overlay_texture != nullptr);

  if (refresh) {
    int s, r;

    cursor_snap.zoom = zoom;

    s = BKE_brush_size_get(vc->scene, br);
    r = 1;

    for (s >>= 1; s > 0; s >>= 1) {
      r++;
    }

    size = (1 << r);

    if (size < 256) {
      size = 256;
    }

    if (size < cursor_snap.size) {
      size = cursor_snap.size;
    }

    if (cursor_snap.size != size) {
      if (cursor_snap.overlay_texture) {
        GPU_texture_free(cursor_snap.overlay_texture);
        cursor_snap.overlay_texture = nullptr;
      }

      init = false;

      cursor_snap.size = size;
    }
    buffer = static_cast<uchar *>(MEM_mallocN(sizeof(uchar) * size * size, "load_tex"));

    BKE_curvemapping_init(br->curve);

    LoadTexData data{};
    data.br = br;
    data.buffer = buffer;
    data.size = size;

    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    BLI_task_parallel_range(0, size, &data, load_tex_cursor_task_cb, &settings);

    if (!cursor_snap.overlay_texture) {
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                               GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW;
      cursor_snap.overlay_texture = GPU_texture_create_2d(
          "cursor_snap_overaly", size, size, 1, GPU_R8, usage, nullptr);
      GPU_texture_update(cursor_snap.overlay_texture, GPU_DATA_UBYTE, buffer);

      GPU_texture_swizzle_set(cursor_snap.overlay_texture, "rrrr");
    }

    if (init) {
      GPU_texture_update(cursor_snap.overlay_texture, GPU_DATA_UBYTE, buffer);
    }

    if (buffer) {
      MEM_freeN(buffer);
    }
  }
  else {
    size = cursor_snap.size;
  }

  cursor_snap.curve_preset = br->curve_preset;
  BKE_paint_reset_overlay_invalid(PAINT_OVERLAY_INVALID_CURVE);

  return 1;
}

static int project_brush_radius(ViewContext *vc, float radius, const float location[3])
{
  float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

  ED_view3d_global_to_vector(vc->rv3d, location, view);

  /* Create a vector that is not orthogonal to view. */

  if (fabsf(view[0]) < 0.1f) {
    nonortho[0] = view[0] + 1.0f;
    nonortho[1] = view[1];
    nonortho[2] = view[2];
  }
  else if (fabsf(view[1]) < 0.1f) {
    nonortho[0] = view[0];
    nonortho[1] = view[1] + 1.0f;
    nonortho[2] = view[2];
  }
  else {
    nonortho[0] = view[0];
    nonortho[1] = view[1];
    nonortho[2] = view[2] + 1.0f;
  }

  /* Get a vector in the plane of the view. */
  cross_v3_v3v3(ortho, nonortho, view);
  normalize_v3(ortho);

  /* Make a point on the surface of the brush tangent to the view. */
  mul_v3_fl(ortho, radius);
  add_v3_v3v3(offset, location, ortho);

  /* Project the center of the brush, and the tangent point to the view onto the screen. */
  if ((ED_view3d_project_float_global(vc->region, location, p1, V3D_PROJ_TEST_NOP) ==
       V3D_PROJ_RET_OK) &&
      (ED_view3d_project_float_global(vc->region, offset, p2, V3D_PROJ_TEST_NOP) ==
       V3D_PROJ_RET_OK))
  {
    /* The distance between these points is the size of the projected brush in pixels. */
    return len_v2v2(p1, p2);
  }
  /* Assert because the code that sets up the vectors should disallow this. */
  BLI_assert(0);
  return 0;
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength. */
static bool paint_draw_tex_overlay(UnifiedPaintSettings *ups,
                                   Brush *brush,
                                   ViewContext *vc,
                                   int x,
                                   int y,
                                   float zoom,
                                   const ePaintMode mode,
                                   bool col,
                                   bool primary)
{
  rctf quad;
  /* Check for overlay mode. */

  MTex *mtex = (primary) ? &brush->mtex : &brush->mask_mtex;
  bool valid = ((primary) ? (brush->overlay_flags & BRUSH_OVERLAY_PRIMARY) != 0 :
                            (brush->overlay_flags & BRUSH_OVERLAY_SECONDARY) != 0);
  int overlay_alpha = (primary) ? brush->texture_overlay_alpha : brush->mask_overlay_alpha;

  if (mode == PAINT_MODE_TEXTURE_3D) {
    if (primary && brush->imagepaint_tool != PAINT_TOOL_DRAW) {
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

  if (load_tex(brush, vc, zoom, col, primary)) {
    GPU_color_mask(true, true, true, true);
    GPU_depth_test(GPU_DEPTH_NONE);

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      GPU_matrix_push();

      float center[2] = {
          ups->draw_anchored ? ups->anchored_initial_mouse[0] : x,
          ups->draw_anchored ? ups->anchored_initial_mouse[1] : y,
      };

      /* Brush rotation. */
      GPU_matrix_translate_2fv(center);
      GPU_matrix_rotate_2d(-RAD2DEGF(primary ? ups->brush_rotation : ups->brush_rotation_sec));
      GPU_matrix_translate_2f(-center[0], -center[1]);

      /* Scale based on tablet pressure. */
      if (primary && ups->stroke_active && BKE_brush_use_size_pressure(brush)) {
        const float scale = ups->size_pressure_value;
        GPU_matrix_translate_2fv(center);
        GPU_matrix_scale_2f(scale, scale);
        GPU_matrix_translate_2f(-center[0], -center[1]);
      }

      if (ups->draw_anchored) {
        quad.xmin = center[0] - ups->anchored_size;
        quad.ymin = center[1] - ups->anchored_size;
        quad.xmax = center[0] + ups->anchored_size;
        quad.ymax = center[1] + ups->anchored_size;
      }
      else {
        const int radius = BKE_brush_size_get(vc->scene, brush) * zoom;
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
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    /* Premultiplied alpha blending. */
    GPU_blend(GPU_BLEND_ALPHA_PREMULT);

    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);

    float final_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (!col) {
      copy_v3_v3(final_color, U.sculpt_paint_overlay_col);
    }
    mul_v4_fl(final_color, overlay_alpha * 0.01f);
    immUniformColor4fv(final_color);

    GPUTexture *texture = (primary) ? primary_snap.overlay_texture :
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
static bool paint_draw_cursor_overlay(
    UnifiedPaintSettings *ups, Brush *brush, ViewContext *vc, int x, int y, float zoom)
{
  rctf quad;
  /* Check for overlay mode. */

  if (!(brush->overlay_flags & BRUSH_OVERLAY_CURSOR)) {
    return false;
  }

  if (load_tex_cursor(brush, vc, zoom)) {
    bool do_pop = false;
    float center[2];

    GPU_color_mask(true, true, true, true);
    GPU_depth_test(GPU_DEPTH_NONE);

    if (ups->draw_anchored) {
      copy_v2_v2(center, ups->anchored_initial_mouse);
      quad.xmin = ups->anchored_initial_mouse[0] - ups->anchored_size;
      quad.ymin = ups->anchored_initial_mouse[1] - ups->anchored_size;
      quad.xmax = ups->anchored_initial_mouse[0] + ups->anchored_size;
      quad.ymax = ups->anchored_initial_mouse[1] + ups->anchored_size;
    }
    else {
      const int radius = BKE_brush_size_get(vc->scene, brush) * zoom;
      center[0] = x;
      center[1] = y;

      quad.xmin = x - radius;
      quad.ymin = y - radius;
      quad.xmax = x + radius;
      quad.ymax = y + radius;
    }

    /* Scale based on tablet pressure. */
    if (ups->stroke_active && BKE_brush_use_size_pressure(brush)) {
      do_pop = true;
      GPU_matrix_push();
      GPU_matrix_translate_2fv(center);
      GPU_matrix_scale_1f(ups->size_pressure_value);
      GPU_matrix_translate_2f(-center[0], -center[1]);
    }

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

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

static bool paint_draw_alpha_overlay(UnifiedPaintSettings *ups,
                                     Brush *brush,
                                     ViewContext *vc,
                                     int x,
                                     int y,
                                     float zoom,
                                     ePaintMode mode)
{
  /* Color means that primary brush texture is colored and
   * secondary is used for alpha/mask control. */
  bool col = ELEM(mode, PAINT_MODE_TEXTURE_3D, PAINT_MODE_TEXTURE_2D, PAINT_MODE_VERTEX);

  bool alpha_overlay_active = false;

  ePaintOverlayControlFlags flags = BKE_paint_get_overlay_flags();
  eGPUBlend blend_state = GPU_blend_get();
  eGPUDepthTest depth_test = GPU_depth_test_get();

  /* Translate to region. */
  GPU_matrix_push();
  GPU_matrix_translate_2f(vc->region->winrct.xmin, vc->region->winrct.ymin);
  x -= vc->region->winrct.xmin;
  y -= vc->region->winrct.ymin;

  /* Colored overlay should be drawn separately. */
  if (col) {
    if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY)) {
      alpha_overlay_active = paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, mode, true, true);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_SECONDARY)) {
      alpha_overlay_active = paint_draw_tex_overlay(
          ups, brush, vc, x, y, zoom, mode, false, false);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR)) {
      alpha_overlay_active = paint_draw_cursor_overlay(ups, brush, vc, x, y, zoom);
    }
  }
  else {
    if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY) && (mode != PAINT_MODE_WEIGHT)) {
      alpha_overlay_active = paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, mode, false, true);
    }
    if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR)) {
      alpha_overlay_active = paint_draw_cursor_overlay(ups, brush, vc, x, y, zoom);
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
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    float selec_col[4], handle_col[4], pivot_col[4];
    UI_GetThemeColorType4fv(TH_VERTEX_SELECT, SPACE_VIEW3D, selec_col);
    UI_GetThemeColorType4fv(TH_PAINT_CURVE_HANDLE, SPACE_VIEW3D, handle_col);
    UI_GetThemeColorType4fv(TH_PAINT_CURVE_PIVOT, SPACE_VIEW3D, pivot_col);

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

      float(*v)[2] = (float(*)[2])data;

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

/* Special actions taken when paint cursor goes over mesh */
/* TODO: sculpt only for now. */
static void paint_cursor_update_unprojected_radius(UnifiedPaintSettings *ups,
                                                   Brush *brush,
                                                   ViewContext *vc,
                                                   const float location[3])
{
  float unprojected_radius, projected_radius;

  /* Update the brush's cached 3D radius. */
  if (!BKE_brush_use_locked_size(vc->scene, brush)) {
    /* Get 2D brush radius. */
    if (ups->draw_anchored) {
      projected_radius = ups->anchored_size;
    }
    else {
      if (brush->flag & BRUSH_ANCHORED) {
        projected_radius = 8;
      }
      else {
        projected_radius = BKE_brush_size_get(vc->scene, brush);
      }
    }

    /* Convert brush radius from 2D to 3D. */
    unprojected_radius = paint_calc_object_space_radius(vc, location, projected_radius);

    /* Scale 3D brush radius by pressure. */
    if (ups->stroke_active && BKE_brush_use_size_pressure(brush)) {
      unprojected_radius *= ups->size_pressure_value;
    }

    /* Set cached value in either Brush or UnifiedPaintSettings. */
    BKE_brush_unprojected_radius_set(vc->scene, brush, unprojected_radius);
  }
}

static void cursor_draw_point_screen_space(const uint gpuattr,
                                           const ARegion *region,
                                           const float true_location[3],
                                           const float obmat[4][4],
                                           const int size)
{
  float translation_vertex_cursor[3], location[3];
  copy_v3_v3(location, true_location);
  mul_m4_v3(obmat, location);
  ED_view3d_project_v3(region, location, translation_vertex_cursor);
  /* Do not draw points behind the view. Z [near, far] is mapped to [-1, 1]. */
  if (translation_vertex_cursor[2] <= 1.0f) {
    imm_draw_circle_fill_3d(
        gpuattr, translation_vertex_cursor[0], translation_vertex_cursor[1], size, 10);
  }
}

static void cursor_draw_tiling_preview(const uint gpuattr,
                                       const ARegion *region,
                                       const float true_location[3],
                                       Sculpt *sd,
                                       Object *ob,
                                       const float radius)
{
  const BoundBox *bb = BKE_object_boundbox_get(ob);
  float orgLoc[3], location[3];
  int tile_pass = 0;
  int start[3];
  int end[3];
  int cur[3];
  const float *bbMin = bb->vec[0];
  const float *bbMax = bb->vec[6];
  const float *step = sd->paint.tile_offset;

  copy_v3_v3(orgLoc, true_location);
  for (int dim = 0; dim < 3; dim++) {
    if ((sd->paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }
        tile_pass++;
        for (int dim = 0; dim < 3; dim++) {
          location[dim] = cur[dim] * step[dim] + orgLoc[dim];
        }
        cursor_draw_point_screen_space(gpuattr, region, location, ob->object_to_world, 3);
      }
    }
  }
  (void)tile_pass; /* Quiet set-but-unused warning (may be removed). */
}

static void cursor_draw_point_with_symmetry(const uint gpuattr,
                                            const ARegion *region,
                                            const float true_location[3],
                                            Sculpt *sd,
                                            Object *ob,
                                            const float radius)
{
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  float location[3], symm_rot_mat[4][4];

  for (int i = 0; i <= symm; i++) {
    if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || !ELEM(i, 3, 5)))) {

      /* Axis Symmetry. */
      flip_v3_v3(location, true_location, ePaintSymmetryFlags(i));
      cursor_draw_point_screen_space(gpuattr, region, location, ob->object_to_world, 3);

      /* Tiling. */
      cursor_draw_tiling_preview(gpuattr, region, location, sd, ob, radius);

      /* Radial Symmetry. */
      for (char raxis = 0; raxis < 3; raxis++) {
        for (int r = 1; r < sd->radial_symm[raxis]; r++) {
          float angle = 2 * M_PI * r / sd->radial_symm[int(raxis)];
          flip_v3_v3(location, true_location, ePaintSymmetryFlags(i));
          unit_m4(symm_rot_mat);
          rotate_m4(symm_rot_mat, raxis + 'X', angle);
          mul_m4_v3(symm_rot_mat, location);

          cursor_draw_tiling_preview(gpuattr, region, location, sd, ob, radius);
          cursor_draw_point_screen_space(gpuattr, region, location, ob->object_to_world, 3);
        }
      }
    }
  }
}

static void sculpt_geometry_preview_lines_draw(const uint gpuattr,
                                               Brush *brush,
                                               const bool is_multires,
                                               SculptSession *ss)
{
  if (!(brush->flag & BRUSH_GRAB_ACTIVE_VERTEX)) {
    return;
  }

  if (is_multires) {
    return;
  }

  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  if (!ss->deform_modifiers_active) {
    return;
  }

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.6f);

  /* Cursor normally draws on top, but for this part we need depth tests. */
  const eGPUDepthTest depth_test = GPU_depth_test_get();
  if (!depth_test) {
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }

  GPU_line_width(1.0f);
  if (ss->preview_vert_count > 0) {
    immBegin(GPU_PRIM_LINES, ss->preview_vert_count);
    for (int i = 0; i < ss->preview_vert_count; i++) {
      immVertex3fv(gpuattr, SCULPT_vertex_co_for_grab_active_get(ss, ss->preview_vert_list[i]));
    }
    immEnd();
  }

  /* Restore depth test value. */
  if (!depth_test) {
    GPU_depth_test(GPU_DEPTH_NONE);
  }
}

static void SCULPT_layer_brush_height_preview_draw(const uint gpuattr,
                                                   const Brush *brush,
                                                   const float rds,
                                                   const float line_width,
                                                   const float outline_col[3],
                                                   const float alpha)
{
  float cursor_trans[4][4];
  unit_m4(cursor_trans);
  translate_m4(cursor_trans, 0.0f, 0.0f, brush->height);
  GPU_matrix_push();
  GPU_matrix_mul(cursor_trans);

  GPU_line_width(line_width);
  immUniformColor3fvAlpha(outline_col, alpha * 0.5f);
  imm_draw_circle_wire_3d(gpuattr, 0, 0, rds, 80);
  GPU_matrix_pop();
}

static bool paint_use_2d_cursor(ePaintMode mode)
{
  if (mode >= PAINT_MODE_TEXTURE_3D) {
    return true;
  }
  return false;
}

enum PaintCursorDrawingType {
  PAINT_CURSOR_CURVE,
  PAINT_CURSOR_2D,
  PAINT_CURSOR_3D,
};

struct PaintCursorContext {
  bContext *C;
  ARegion *region;
  wmWindow *win;
  wmWindowManager *wm;
  Depsgraph *depsgraph;
  Scene *scene;
  UnifiedPaintSettings *ups;
  Brush *brush;
  Paint *paint;
  ePaintMode mode;
  ViewContext vc;

  /* Sculpt related data. */
  Sculpt *sd;
  SculptSession *ss;
  PBVHVertRef prev_active_vertex;
  bool is_stroke_active;
  bool is_cursor_over_mesh;
  bool is_multires;
  float radius;

  /* 3D view cursor position and normal. */
  float location[3];
  float scene_space_location[3];
  float normal[3];

  /* Cursor main colors. */
  float outline_col[3];
  float outline_alpha;

  /* GPU attribute for drawing. */
  uint pos;

  PaintCursorDrawingType cursor_type;

  /* This variable is set after drawing the overlay, not on initialization. It can't be used for
   * checking if alpha overlay is enabled before drawing it. */
  bool alpha_overlay_drawn;

  float zoomx;
  int x, y;
  float translation[2];

  float final_radius;
  int pixel_radius;
};

static bool paint_cursor_context_init(bContext *C,
                                      const int x,
                                      const int y,
                                      PaintCursorContext *pcontext)
{
  ARegion *region = CTX_wm_region(C);
  if (region && region->regiontype != RGN_TYPE_WINDOW) {
    return false;
  }

  pcontext->C = C;
  pcontext->region = region;
  pcontext->wm = CTX_wm_manager(C);
  pcontext->win = CTX_wm_window(C);
  pcontext->depsgraph = CTX_data_depsgraph_pointer(C);
  pcontext->scene = CTX_data_scene(C);
  pcontext->ups = &pcontext->scene->toolsettings->unified_paint_settings;
  pcontext->paint = BKE_paint_get_active_from_context(C);
  if (pcontext->paint == nullptr) {
    return false;
  }
  pcontext->brush = BKE_paint_brush(pcontext->paint);
  if (pcontext->brush == nullptr) {
    return false;
  }
  pcontext->mode = BKE_paintmode_get_active_from_context(C);

  ED_view3d_viewcontext_init(C, &pcontext->vc, pcontext->depsgraph);

  if (pcontext->brush->flag & BRUSH_CURVE) {
    pcontext->cursor_type = PAINT_CURSOR_CURVE;
  }
  else if (paint_use_2d_cursor(pcontext->mode)) {
    pcontext->cursor_type = PAINT_CURSOR_2D;
  }
  else {
    pcontext->cursor_type = PAINT_CURSOR_3D;
  }

  pcontext->x = x;
  pcontext->y = y;
  pcontext->translation[0] = float(x);
  pcontext->translation[1] = float(y);

  float zoomx, zoomy;
  get_imapaint_zoom(C, &zoomx, &zoomy);
  pcontext->zoomx = max_ff(zoomx, zoomy);
  pcontext->final_radius = (BKE_brush_size_get(pcontext->scene, pcontext->brush) * zoomx);

  /* There is currently no way to check if the direction is inverted before starting the stroke,
   * so this does not reflect the state of the brush in the UI. */
  if (((pcontext->ups->draw_inverted == 0) ^ ((pcontext->brush->flag & BRUSH_DIR_IN) == 0)) &&
      BKE_brush_sculpt_has_secondary_color(pcontext->brush))
  {
    copy_v3_v3(pcontext->outline_col, pcontext->brush->sub_col);
  }
  else {
    copy_v3_v3(pcontext->outline_col, pcontext->brush->add_col);
  }
  pcontext->outline_alpha = pcontext->brush->add_col[3];

  Object *active_object = pcontext->vc.obact;
  pcontext->ss = active_object ? active_object->sculpt : nullptr;

  if (pcontext->ss && pcontext->ss->draw_faded_cursor) {
    pcontext->outline_alpha = 0.3f;
    copy_v3_fl(pcontext->outline_col, 0.8f);
  }

  const bool is_brush_tool = PAINT_brush_tool_poll(C);
  if (!is_brush_tool) {
    /* Use a default color for tools that are not brushes. */
    pcontext->outline_alpha = 0.8f;
    copy_v3_fl(pcontext->outline_col, 0.8f);
  }

  pcontext->is_stroke_active = pcontext->ups->stroke_active;

  return true;
}

static void paint_cursor_update_pixel_radius(PaintCursorContext *pcontext)
{
  if (pcontext->is_cursor_over_mesh) {
    Brush *brush = BKE_paint_brush(pcontext->paint);
    pcontext->pixel_radius = project_brush_radius(
        &pcontext->vc,
        BKE_brush_unprojected_radius_get(pcontext->scene, brush),
        pcontext->location);

    if (pcontext->pixel_radius == 0) {
      pcontext->pixel_radius = BKE_brush_size_get(pcontext->scene, brush);
    }

    copy_v3_v3(pcontext->scene_space_location, pcontext->location);
    mul_m4_v3(pcontext->vc.obact->object_to_world, pcontext->scene_space_location);
  }
  else {
    Sculpt *sd = CTX_data_tool_settings(pcontext->C)->sculpt;
    Brush *brush = BKE_paint_brush(&sd->paint);

    pcontext->pixel_radius = BKE_brush_size_get(pcontext->scene, brush);
  }
}

static void paint_cursor_sculpt_session_update_and_init(PaintCursorContext *pcontext)
{
  BLI_assert(pcontext->ss != nullptr);
  BLI_assert(pcontext->mode == PAINT_MODE_SCULPT);

  bContext *C = pcontext->C;
  SculptSession *ss = pcontext->ss;
  Brush *brush = pcontext->brush;
  Scene *scene = pcontext->scene;
  UnifiedPaintSettings *ups = pcontext->ups;
  ViewContext *vc = &pcontext->vc;
  SculptCursorGeometryInfo gi;

  const float mval_fl[2] = {
      float(pcontext->x - pcontext->region->winrct.xmin),
      float(pcontext->y - pcontext->region->winrct.ymin),
  };

  /* This updates the active vertex, which is needed for most of the Sculpt/Vertex Colors tools to
   * work correctly */
  pcontext->prev_active_vertex = ss->active_vertex;
  if (!ups->stroke_active) {
    pcontext->is_cursor_over_mesh = SCULPT_cursor_geometry_info_update(
        C, &gi, mval_fl, (pcontext->brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE), false);
    copy_v3_v3(pcontext->location, gi.location);
    copy_v3_v3(pcontext->normal, gi.normal);
  }
  else {
    pcontext->is_cursor_over_mesh = ups->last_hit;
    copy_v3_v3(pcontext->location, ups->last_location);
  }

  paint_cursor_update_pixel_radius(pcontext);

  if (BKE_brush_use_locked_size(scene, brush)) {
    BKE_brush_size_set(scene, brush, pcontext->pixel_radius);
  }

  if (pcontext->is_cursor_over_mesh) {
    paint_cursor_update_unprojected_radius(ups, brush, vc, pcontext->scene_space_location);
  }

  pcontext->is_multires = ss->pbvh != nullptr && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS;

  pcontext->sd = CTX_data_tool_settings(pcontext->C)->sculpt;
}

static void paint_update_mouse_cursor(PaintCursorContext *pcontext)
{
  if (pcontext->win->grabcursor != 0) {
    /* Don't set the cursor while it's grabbed, since this will show the cursor when interacting
     * with the UI (dragging a number button for e.g.), see: #102792. */
    return;
  }
  WM_cursor_set(pcontext->win, WM_CURSOR_PAINT);
}

static void paint_draw_2D_view_brush_cursor(PaintCursorContext *pcontext)
{
  immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha);

  /* Draw brush outline. */
  if (pcontext->ups->stroke_active && BKE_brush_use_size_pressure(pcontext->brush)) {
    imm_draw_circle_wire_2d(pcontext->pos,
                            pcontext->translation[0],
                            pcontext->translation[1],
                            pcontext->final_radius * pcontext->ups->size_pressure_value,
                            40);
    /* Outer at half alpha. */
    immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha * 0.5f);
  }

  GPU_line_width(1.0f);
  imm_draw_circle_wire_2d(pcontext->pos,
                          pcontext->translation[0],
                          pcontext->translation[1],
                          pcontext->final_radius,
                          40);
}

static void paint_draw_legacy_3D_view_brush_cursor(PaintCursorContext *pcontext)
{
  GPU_line_width(1.0f);
  immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha);
  imm_draw_circle_wire_3d(pcontext->pos,
                          pcontext->translation[0],
                          pcontext->translation[1],
                          pcontext->final_radius,
                          40);
}

static void paint_draw_3D_view_inactive_brush_cursor(PaintCursorContext *pcontext)
{
  GPU_line_width(1.0f);
  /* Reduce alpha to increase the contrast when the cursor is over the mesh. */
  immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha * 0.8);
  imm_draw_circle_wire_3d(pcontext->pos,
                          pcontext->translation[0],
                          pcontext->translation[1],
                          pcontext->final_radius,
                          80);
  immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha * 0.35f);
  imm_draw_circle_wire_3d(pcontext->pos,
                          pcontext->translation[0],
                          pcontext->translation[1],
                          pcontext->final_radius * clamp_f(pcontext->brush->alpha, 0.0f, 1.0f),
                          80);
}

static void sculpt_cursor_draw_active_face_set_color_set(PaintCursorContext *pcontext)
{

  SculptSession *ss = pcontext->ss;

  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  const int active_face_set = SCULPT_active_face_set_get(ss);
  uchar color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
  Object *ob = CTX_data_active_object(pcontext->C);
  Mesh *mesh = (Mesh *)ob->data;
  if (active_face_set != mesh->face_sets_color_default) {
    BKE_paint_face_set_overlay_color_get(active_face_set, mesh->face_sets_color_seed, color);
    color[3] = UCHAR_MAX;
  }
  else {
    color[3] /= 2;
  }

  immUniformColor4ubv(color);
}

static void sculpt_cursor_draw_3D_face_set_preview(PaintCursorContext *pcontext)
{

  SculptSession *ss = pcontext->ss;

  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return;
  }

  GPU_line_width(1.0f);
  sculpt_cursor_draw_active_face_set_color_set(pcontext);

  /*
  MPoly *poly = &ss->mpoly[fi];
  MLoop *loops = ss->mloop;
  const int totpoints = poly->totloop;

  immBegin(GPU_PRIM_LINE_STRIP, totpoints + 1);
  for (int i = 0; i < totpoints; i++) {
    float co[3];
    copy_v3_v3(co, SCULPT_vertex_co_get(ss, loops[poly->loopstart + i].v));
    immVertex3fv(pcontext->pos, co);
  }
  immVertex3fv(pcontext->pos, SCULPT_vertex_co_get(ss, loops[poly->loopstart].v));
  immEnd();
  */

  /*
  int v_in_poly = 0;
  for (int i = 0; i < totpoints; i++) {
    if (ss->active_vertex == loops[poly->loopstart + i].v) {
      v_in_poly = i;
    }
  }
  const int next_v = v_in_poly == poly->totloop - 1? 0 : v_in_poly + 1;
  const int prev_v = v_in_poly == 0? poly->totloop - 1 : v_in_poly  - 1;


  immBegin(GPU_PRIM_LINES, 4);
  immVertex3fv(pcontext->pos, SCULPT_vertex_co_get(ss, ss->active_vertex));
  immVertex3fv(pcontext->pos, SCULPT_vertex_co_get(ss, loops[poly->loopstart + next_v].v));


  immVertex3fv(pcontext->pos, SCULPT_vertex_co_get(ss, ss->active_vertex));
  immVertex3fv(pcontext->pos, SCULPT_vertex_co_get(ss, loops[poly->loopstart + prev_v].v));

  immEnd();
  */

  if (!ss->pmap) {
    return;
  }

  int total = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, ss->active_vertex, ni) {
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  immBegin(GPU_PRIM_LINES, total * 2);
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, ss->active_vertex, ni) {
    immVertex3fv(pcontext->pos, SCULPT_active_vertex_co_get(ss));
    immVertex3fv(pcontext->pos, SCULPT_vertex_co_get(ss, ni.vertex));
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  immEnd();
}

static void paint_cursor_update_object_space_radius(PaintCursorContext *pcontext)
{
  if (!BKE_brush_use_locked_size(pcontext->scene, pcontext->brush)) {
    pcontext->radius = paint_calc_object_space_radius(
        &pcontext->vc, pcontext->location, BKE_brush_size_get(pcontext->scene, pcontext->brush));
  }
  else {
    pcontext->radius = BKE_brush_unprojected_radius_get(pcontext->scene, pcontext->brush);
  }
}

static void paint_cursor_drawing_setup_cursor_space(PaintCursorContext *pcontext)
{
  float cursor_trans[4][4], cursor_rot[4][4];
  const float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  copy_m4_m4(cursor_trans, pcontext->vc.obact->object_to_world);
  translate_m4(cursor_trans, pcontext->location[0], pcontext->location[1], pcontext->location[2]);
  rotation_between_vecs_to_quat(quat, z_axis, pcontext->normal);
  quat_to_mat4(cursor_rot, quat);
  GPU_matrix_mul(cursor_trans);
  GPU_matrix_mul(cursor_rot);
}

static void paint_cursor_draw_main_inactive_cursor(PaintCursorContext *pcontext)
{
  immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha);
  GPU_line_width(2.0f);
  imm_draw_circle_wire_3d(pcontext->pos, 0, 0, pcontext->radius, 80);

  GPU_line_width(1.0f);
  immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha * 0.5f);
  imm_draw_circle_wire_3d(
      pcontext->pos, 0, 0, pcontext->radius * clamp_f(pcontext->brush->alpha, 0.0f, 1.0f), 80);
}

static void paint_cursor_pose_brush_segments_draw(PaintCursorContext *pcontext)
{
  SculptSession *ss = pcontext->ss;
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  GPU_line_width(2.0f);

  immBegin(GPU_PRIM_LINES, ss->pose_ik_chain_preview->tot_segments * 2);
  for (int i = 0; i < ss->pose_ik_chain_preview->tot_segments; i++) {
    immVertex3fv(pcontext->pos, ss->pose_ik_chain_preview->segments[i].initial_orig);
    immVertex3fv(pcontext->pos, ss->pose_ik_chain_preview->segments[i].initial_head);
  }

  immEnd();
}

static void paint_cursor_pose_brush_origins_draw(PaintCursorContext *pcontext)
{

  SculptSession *ss = pcontext->ss;
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  for (int i = 0; i < ss->pose_ik_chain_preview->tot_segments; i++) {
    cursor_draw_point_screen_space(pcontext->pos,
                                   pcontext->region,
                                   ss->pose_ik_chain_preview->segments[i].initial_orig,
                                   pcontext->vc.obact->object_to_world,
                                   3);
  }
}

static void paint_cursor_preview_boundary_data_pivot_draw(PaintCursorContext *pcontext)
{

  if (!pcontext->ss->boundary_preview) {
    /* There is no guarantee that a boundary preview exists as there may be no boundaries
     * inside the brush radius. */
    return;
  }
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  cursor_draw_point_screen_space(
      pcontext->pos,
      pcontext->region,
      SCULPT_vertex_co_get(pcontext->ss, pcontext->ss->boundary_preview->pivot_vertex),
      pcontext->vc.obact->object_to_world,
      3);
}

static void paint_cursor_preview_boundary_data_update(PaintCursorContext *pcontext,
                                                      const bool update_previews)
{
  SculptSession *ss = pcontext->ss;
  if (!(update_previews || !ss->boundary_preview)) {
    return;
  }

  /* Needed for updating the necessary SculptSession data in order to initialize the
   * boundary data for the preview. */
  BKE_sculpt_update_object_for_edit(pcontext->depsgraph, pcontext->vc.obact, true, false, false);

  if (ss->boundary_preview) {
    SCULPT_boundary_data_free(ss->boundary_preview);
  }

  ss->boundary_preview = SCULPT_boundary_data_init(
      pcontext->sd, pcontext->vc.obact, pcontext->brush, ss->active_vertex, pcontext->radius);
}

static void paint_cursor_draw_3d_view_brush_cursor_inactive(PaintCursorContext *pcontext)
{
  Brush *brush = pcontext->brush;

  /* 2D falloff is better represented with the default 2D cursor,
   * there is no need to draw anything else. */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    paint_draw_legacy_3D_view_brush_cursor(pcontext);
    return;
  }

  if (pcontext->alpha_overlay_drawn) {
    paint_draw_legacy_3D_view_brush_cursor(pcontext);
    return;
  }

  if (!pcontext->is_cursor_over_mesh) {
    paint_draw_3D_view_inactive_brush_cursor(pcontext);
    return;
  }

  paint_cursor_update_object_space_radius(pcontext);

  const bool update_previews = pcontext->prev_active_vertex.i !=
                               SCULPT_active_vertex_get(pcontext->ss).i;

  /* Setup drawing. */
  wmViewport(&pcontext->region->winrct);

  /* Drawing of Cursor overlays in 2D screen space. */

  /* Cursor location symmetry points. */

  const float *active_vertex_co;
  if (brush->sculpt_tool == SCULPT_TOOL_GRAB && brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
    active_vertex_co = SCULPT_vertex_co_for_grab_active_get(
        pcontext->ss, SCULPT_active_vertex_get(pcontext->ss));
  }
  else {
    active_vertex_co = SCULPT_active_vertex_co_get(pcontext->ss);
  }
  if (len_v3v3(active_vertex_co, pcontext->location) < pcontext->radius) {
    immUniformColor3fvAlpha(pcontext->outline_col, pcontext->outline_alpha);
    sculpt_cursor_draw_active_face_set_color_set(pcontext);
    cursor_draw_point_with_symmetry(pcontext->pos,
                                    pcontext->region,
                                    active_vertex_co,
                                    pcontext->sd,
                                    pcontext->vc.obact,
                                    pcontext->radius);
  }

  const bool is_brush_tool = PAINT_brush_tool_poll(pcontext->C);

  /* Pose brush updates and rotation origins. */

  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_POSE) {
    /* Just after switching to the Pose Brush, the active vertex can be the same and the
     * cursor won't be tagged to update, so always initialize the preview chain if it is
     * nullptr before drawing it. */
    SculptSession *ss = pcontext->ss;
    if (update_previews || !ss->pose_ik_chain_preview) {
      BKE_sculpt_update_object_for_edit(
          pcontext->depsgraph, pcontext->vc.obact, true, false, false);

      /* Free the previous pose brush preview. */
      if (ss->pose_ik_chain_preview) {
        SCULPT_pose_ik_chain_free(ss->pose_ik_chain_preview);
      }

      /* Generate a new pose brush preview from the current cursor location. */
      ss->pose_ik_chain_preview = SCULPT_pose_ik_chain_init(
          pcontext->sd, pcontext->vc.obact, ss, brush, pcontext->location, pcontext->radius);
    }

    /* Draw the pose brush rotation origins. */
    paint_cursor_pose_brush_origins_draw(pcontext);
  }

  /* Expand operation origin. */
  if (pcontext->ss->expand_cache) {
    cursor_draw_point_screen_space(
        pcontext->pos,
        pcontext->region,
        SCULPT_vertex_co_get(pcontext->ss, pcontext->ss->expand_cache->initial_active_vertex),
        pcontext->vc.obact->object_to_world,
        2);
  }

  /* Transform Pivot. */
  if (pcontext->paint && pcontext->paint->flags & PAINT_SCULPT_SHOW_PIVOT) {
    cursor_draw_point_screen_space(pcontext->pos,
                                   pcontext->region,
                                   pcontext->ss->pivot_pos,
                                   pcontext->vc.obact->object_to_world,
                                   2);
  }

  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_BOUNDARY) {
    paint_cursor_preview_boundary_data_update(pcontext, update_previews);
    paint_cursor_preview_boundary_data_pivot_draw(pcontext);
  }

  /* Setup 3D perspective drawing. */
  GPU_matrix_push_projection();
  ED_view3d_draw_setup_view(pcontext->wm,
                            pcontext->win,
                            pcontext->depsgraph,
                            pcontext->scene,
                            pcontext->region,
                            CTX_wm_view3d(pcontext->C),
                            nullptr,
                            nullptr,
                            nullptr);

  GPU_matrix_push();
  GPU_matrix_mul(pcontext->vc.obact->object_to_world);

  /* Drawing Cursor overlays in 3D object space. */
  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_GRAB &&
      (brush->flag & BRUSH_GRAB_ACTIVE_VERTEX))
  {
    SCULPT_geometry_preview_lines_update(pcontext->C, pcontext->ss, pcontext->radius);
    sculpt_geometry_preview_lines_draw(
        pcontext->pos, pcontext->brush, pcontext->is_multires, pcontext->ss);
  }

  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_POSE) {
    paint_cursor_pose_brush_segments_draw(pcontext);
  }

  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_BOUNDARY) {
    SCULPT_boundary_edges_preview_draw(
        pcontext->pos, pcontext->ss, pcontext->outline_col, pcontext->outline_alpha);
    SCULPT_boundary_pivot_line_preview_draw(pcontext->pos, pcontext->ss);
  }

  /* Face Set Preview. */
  sculpt_cursor_draw_3D_face_set_preview(pcontext);

  GPU_matrix_pop();

  /* Drawing Cursor overlays in Paint Cursor space (as additional info on top of the brush cursor)
   */
  GPU_matrix_push();
  paint_cursor_drawing_setup_cursor_space(pcontext);
  /* Main inactive cursor. */
  paint_cursor_draw_main_inactive_cursor(pcontext);

  /* Cloth brush local simulation areas. */
  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_CLOTH &&
      brush->cloth_simulation_area_type != BRUSH_CLOTH_SIMULATION_AREA_GLOBAL)
  {
    const float white[3] = {1.0f, 1.0f, 1.0f};
    const float zero_v[3] = {0.0f};
    /* This functions sets its own drawing space in order to draw the simulation limits when the
     * cursor is active. When used here, this cursor overlay is already in cursor space, so its
     * position and normal should be set to 0. */
    SCULPT_cloth_simulation_limits_draw(pcontext->ss,
                                        pcontext->sd,
                                        pcontext->pos,
                                        brush,
                                        zero_v,
                                        zero_v,
                                        pcontext->radius,
                                        1.0f,
                                        white,
                                        0.25f);
  }

  /* Layer brush height. */
  if (is_brush_tool && brush->sculpt_tool == SCULPT_TOOL_LAYER) {
    SCULPT_layer_brush_height_preview_draw(pcontext->pos,
                                           brush,
                                           pcontext->radius,
                                           1.0f,
                                           pcontext->outline_col,
                                           pcontext->outline_alpha);
  }

  GPU_matrix_pop();

  /* Reset drawing. */
  GPU_matrix_pop_projection();
  wmWindowViewport(pcontext->win);
}

static void paint_cursor_cursor_draw_3d_view_brush_cursor_active(PaintCursorContext *pcontext)
{
  BLI_assert(pcontext->ss != nullptr);
  BLI_assert(pcontext->mode == PAINT_MODE_SCULPT);

  SculptSession *ss = pcontext->ss;
  Brush *brush = pcontext->brush;

  /* The cursor can be updated as active before creating the StrokeCache, so this needs to be
   * checked. */
  if (!ss->cache) {
    return;
  }

  /* Most of the brushes initialize the necessary data for the custom cursor drawing after the
   * first brush step, so make sure that it is not drawn before being initialized. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  /* Setup drawing. */
  wmViewport(&pcontext->region->winrct);
  GPU_matrix_push_projection();
  ED_view3d_draw_setup_view(pcontext->wm,
                            pcontext->win,
                            pcontext->depsgraph,
                            pcontext->scene,
                            pcontext->region,
                            CTX_wm_view3d(pcontext->C),
                            nullptr,
                            nullptr,
                            nullptr);
  GPU_matrix_push();
  GPU_matrix_mul(pcontext->vc.obact->object_to_world);

  /* Draw the special active cursors different tools may have. */

  if (brush->sculpt_tool == SCULPT_TOOL_GRAB) {
    sculpt_geometry_preview_lines_draw(pcontext->pos, brush, pcontext->is_multires, ss);
  }

  if (brush->sculpt_tool == SCULPT_TOOL_MULTIPLANE_SCRAPE) {
    SCULPT_multiplane_scrape_preview_draw(
        pcontext->pos, brush, ss, pcontext->outline_col, pcontext->outline_alpha);
  }

  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    if (brush->cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE) {
      SCULPT_cloth_plane_falloff_preview_draw(
          pcontext->pos, ss, pcontext->outline_col, pcontext->outline_alpha);
    }
    else if (brush->cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_RADIAL &&
             brush->cloth_simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_LOCAL)
    {
      /* Display the simulation limits if sculpting outside them. */
      /* This does not makes much sense of plane falloff as the falloff is infinite or global. */

      if (len_v3v3(ss->cache->true_location, ss->cache->true_initial_location) >
          ss->cache->radius * (1.0f + brush->cloth_sim_limit))
      {
        const float red[3] = {1.0f, 0.2f, 0.2f};
        SCULPT_cloth_simulation_limits_draw(pcontext->ss,
                                            pcontext->sd,
                                            pcontext->pos,
                                            brush,
                                            ss->cache->true_initial_location,
                                            ss->cache->true_initial_normal,
                                            ss->cache->radius,
                                            2.0f,
                                            red,
                                            0.8f);
      }
    }
  }

  GPU_matrix_pop();

  GPU_matrix_pop_projection();
  wmWindowViewport(pcontext->win);
}

static void paint_cursor_draw_3D_view_brush_cursor(PaintCursorContext *pcontext)
{

  /* These paint tools are not using the SculptSession, so they need to use the default 2D brush
   * cursor in the 3D view. */
  if (pcontext->mode != PAINT_MODE_SCULPT || !pcontext->ss) {
    paint_draw_legacy_3D_view_brush_cursor(pcontext);
    return;
  }

  paint_cursor_sculpt_session_update_and_init(pcontext);

  if (pcontext->is_stroke_active) {
    paint_cursor_cursor_draw_3d_view_brush_cursor_active(pcontext);
  }
  else {
    paint_cursor_draw_3d_view_brush_cursor_inactive(pcontext);
  }
}

static bool paint_cursor_is_3d_view_navigating(PaintCursorContext *pcontext)
{
  ViewContext *vc = &pcontext->vc;
  return vc->rv3d && (vc->rv3d->rflag & RV3D_NAVIGATING);
}

static bool paint_cursor_is_brush_cursor_enabled(PaintCursorContext *pcontext)
{
  if (pcontext->paint->flags & PAINT_SHOW_BRUSH) {
    if (ELEM(pcontext->mode, PAINT_MODE_TEXTURE_2D, PAINT_MODE_TEXTURE_3D) &&
        pcontext->brush->imagepaint_tool == PAINT_TOOL_FILL)
    {
      return false;
    }
    return true;
  }
  return false;
}

static void paint_cursor_update_rake_rotation(PaintCursorContext *pcontext)
{
  /* Don't calculate rake angles while a stroke is active because the rake variables are global
   * and we may get interference with the stroke itself.
   * For line strokes, such interference is visible. */
  if (!pcontext->ups->stroke_active) {
    float zero[2] = {0.0f, 0.0f};

    paint_calculate_rake_rotation(
        pcontext->ups, pcontext->brush, pcontext->translation, zero, pcontext->mode);
  }
}

static void paint_cursor_check_and_draw_alpha_overlays(PaintCursorContext *pcontext)
{
  pcontext->alpha_overlay_drawn = paint_draw_alpha_overlay(pcontext->ups,
                                                           pcontext->brush,
                                                           &pcontext->vc,
                                                           pcontext->x,
                                                           pcontext->y,
                                                           pcontext->zoomx,
                                                           pcontext->mode);
}

static void paint_cursor_update_anchored_location(PaintCursorContext *pcontext)
{
  UnifiedPaintSettings *ups = pcontext->ups;
  if (ups->draw_anchored) {
    pcontext->final_radius = ups->anchored_size;
    copy_v2_fl2(pcontext->translation,
                ups->anchored_initial_mouse[0] + pcontext->region->winrct.xmin,
                ups->anchored_initial_mouse[1] + pcontext->region->winrct.ymin);
  }
}

static void paint_cursor_setup_2D_drawing(PaintCursorContext *pcontext)
{
  GPU_line_width(2.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  pcontext->pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
}

static void paint_cursor_setup_3D_drawing(PaintCursorContext *pcontext)
{
  GPU_line_width(2.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  pcontext->pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
}

static void paint_cursor_restore_drawing_state()
{
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

static void paint_draw_cursor(bContext *C, int x, int y, void * /*unused*/)
{
  PaintCursorContext pcontext;
  if (!paint_cursor_context_init(C, x, y, &pcontext)) {
    return;
  }

  if (!paint_cursor_is_brush_cursor_enabled(&pcontext)) {
    return;
  }
  if (paint_cursor_is_3d_view_navigating(&pcontext)) {
    return;
  }

  switch (pcontext.cursor_type) {
    case PAINT_CURSOR_CURVE:
      paint_draw_curve_cursor(pcontext.brush, &pcontext.vc);
      break;
    case PAINT_CURSOR_2D:
      paint_cursor_update_rake_rotation(&pcontext);
      paint_cursor_check_and_draw_alpha_overlays(&pcontext);
      paint_cursor_update_anchored_location(&pcontext);

      paint_cursor_setup_2D_drawing(&pcontext);
      paint_draw_2D_view_brush_cursor(&pcontext);
      paint_cursor_restore_drawing_state();
      break;
    case PAINT_CURSOR_3D:
      paint_update_mouse_cursor(&pcontext);

      paint_cursor_update_rake_rotation(&pcontext);
      paint_cursor_check_and_draw_alpha_overlays(&pcontext);
      paint_cursor_update_anchored_location(&pcontext);

      paint_cursor_setup_3D_drawing(&pcontext);
      paint_cursor_draw_3D_view_brush_cursor(&pcontext);
      paint_cursor_restore_drawing_state();
      break;
  }
}

/* Public API */

void ED_paint_cursor_start(Paint *p, bool (*poll)(bContext *C))
{
  if (p && !p->paint_cursor) {
    p->paint_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, poll, paint_draw_cursor, nullptr);
  }

  /* Invalidate the paint cursors. */
  BKE_paint_invalidate_overlay_all();
}
