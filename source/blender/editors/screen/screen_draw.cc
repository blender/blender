/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include "ED_screen.h"

#include "GPU_batch_presets.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_state.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"

#include "WM_api.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "screen_intern.h"

#define CORNER_RESOLUTION 3

static void do_vert_pair(GPUVertBuf *vbo, uint pos, uint *vidx, int corner, int i)
{
  float inter[2];
  inter[0] = cosf(corner * M_PI_2 + (i * M_PI_2 / (CORNER_RESOLUTION - 1.0f)));
  inter[1] = sinf(corner * M_PI_2 + (i * M_PI_2 / (CORNER_RESOLUTION - 1.0f)));

  /* Snap point to edge */
  float div = 1.0f / max_ff(fabsf(inter[0]), fabsf(inter[1]));
  float exter[2];
  mul_v2_v2fl(exter, inter, div);
  exter[0] = roundf(exter[0]);
  exter[1] = roundf(exter[1]);

  if (i == 0 || i == (CORNER_RESOLUTION - 1)) {
    copy_v2_v2(inter, exter);
  }

  /* Line width is 20% of the entire corner size. */
  const float line_width = 0.2f; /* Keep in sync with shader */
  mul_v2_fl(inter, 1.0f - line_width);
  mul_v2_fl(exter, 1.0f + line_width);

  switch (corner) {
    case 0:
      add_v2_v2(inter, blender::float2{-1.0f, -1.0f});
      add_v2_v2(exter, blender::float2{-1.0f, -1.0f});
      break;
    case 1:
      add_v2_v2(inter, blender::float2{1.0f, -1.0f});
      add_v2_v2(exter, blender::float2{1.0f, -1.0f});
      break;
    case 2:
      add_v2_v2(inter, blender::float2{1.0f, 1.0f});
      add_v2_v2(exter, blender::float2{1.0f, 1.0f});
      break;
    case 3:
      add_v2_v2(inter, blender::float2{-1.0f, 1.0f});
      add_v2_v2(exter, blender::float2{-1.0f, 1.0f});
      break;
  }

  GPU_vertbuf_attr_set(vbo, pos, (*vidx)++, inter);
  GPU_vertbuf_attr_set(vbo, pos, (*vidx)++, exter);
}

static GPUBatch *batch_screen_edges_get(int *corner_len)
{
  static GPUBatch *screen_edges_batch = nullptr;

  if (screen_edges_batch == nullptr) {
    GPUVertFormat format = {0};
    uint pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, CORNER_RESOLUTION * 2 * 4 + 2);

    uint vidx = 0;
    for (int corner = 0; corner < 4; corner++) {
      for (int c = 0; c < CORNER_RESOLUTION; c++) {
        do_vert_pair(vbo, pos, &vidx, corner, c);
      }
    }
    /* close the loop */
    do_vert_pair(vbo, pos, &vidx, 0, 0);

    screen_edges_batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
    gpu_batch_presets_register(screen_edges_batch);
  }

  if (corner_len) {
    *corner_len = CORNER_RESOLUTION * 2;
  }
  return screen_edges_batch;
}

#undef CORNER_RESOLUTION

static void drawscredge_area_draw(
    int sizex, int sizey, short x1, short y1, short x2, short y2, float edge_thickness)
{
  rctf rect;
  BLI_rctf_init(&rect, float(x1), float(x2), float(y1), float(y2));

  /* right border area */
  if (x2 >= sizex - 1) {
    rect.xmax += edge_thickness * 0.5f;
  }

  /* left border area */
  if (x1 <= 0) { /* otherwise it draws the emboss of window over */
    rect.xmin -= edge_thickness * 0.5f;
  }

  /* top border area */
  if (y2 >= sizey - 1) {
    rect.ymax += edge_thickness * 0.5f;
  }

  /* bottom border area */
  if (y1 <= 0) {
    rect.ymin -= edge_thickness * 0.5f;
  }

  GPUBatch *batch = batch_screen_edges_get(nullptr);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_AREA_BORDERS);
  GPU_batch_uniform_4fv(batch, "rect", (float *)&rect);
  GPU_batch_draw(batch);
}

/**
 * \brief Screen edges drawing.
 */
static void drawscredge_area(ScrArea *area, int sizex, int sizey, float edge_thickness)
{
  short x1 = area->v1->vec.x;
  short y1 = area->v1->vec.y;
  short x2 = area->v3->vec.x;
  short y2 = area->v3->vec.y;

  drawscredge_area_draw(sizex, sizey, x1, y1, x2, y2, edge_thickness);
}

void ED_screen_draw_edges(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  screen->do_draw = false;

  if (screen->state == SCREENFULL) {
    return;
  }

  if (screen->temp && BLI_listbase_is_single(&screen->areabase)) {
    return;
  }

  const int winsize_x = WM_window_pixels_x(win);
  const int winsize_y = WM_window_pixels_y(win);
  float col[4], corner_scale, edge_thickness;
  int verts_per_corner = 0;

  rcti scissor_rect;
  BLI_rcti_init_minmax(&scissor_rect);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    BLI_rcti_do_minmax_v(&scissor_rect, blender::int2{area->v1->vec.x, area->v1->vec.y});
    BLI_rcti_do_minmax_v(&scissor_rect, blender::int2{area->v3->vec.x, area->v3->vec.y});
  }

  if (GPU_type_matches_ex(GPU_DEVICE_INTEL_UHD, GPU_OS_UNIX, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    /* For some reason, on linux + Intel UHD Graphics 620 the driver
     * hangs if we don't flush before this. (See #57455) */
    GPU_flush();
  }

  GPU_scissor(scissor_rect.xmin,
              scissor_rect.ymin,
              BLI_rcti_size_x(&scissor_rect) + 1,
              BLI_rcti_size_y(&scissor_rect) + 1);

  /* It seems that all areas gets smaller when pixelsize is > 1.
   * So in order to avoid missing pixels we just disable de scissors. */
  if (U.pixelsize <= 1.0f) {
    GPU_scissor_test(true);
  }

  UI_GetThemeColor4fv(TH_EDITOR_OUTLINE, col);
  col[3] = 1.0f;
  corner_scale = U.pixelsize * 8.0f;
  edge_thickness = corner_scale * 0.21f;

  GPU_blend(GPU_BLEND_ALPHA);

  GPUBatch *batch = batch_screen_edges_get(&verts_per_corner);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_AREA_BORDERS);
  GPU_batch_uniform_1i(batch, "cornerLen", verts_per_corner);
  GPU_batch_uniform_1f(batch, "scale", corner_scale);
  GPU_batch_uniform_4fv(batch, "color", col);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    drawscredge_area(area, winsize_x, winsize_y, edge_thickness);
  }

  GPU_blend(GPU_BLEND_NONE);

  if (U.pixelsize <= 1.0f) {
    GPU_scissor_test(false);
  }
}

void screen_draw_join_highlight(ScrArea *sa1, ScrArea *sa2)
{
  const eScreenDir dir = area_getorientation(sa1, sa2);
  if (dir == SCREEN_DIR_NONE) {
    return;
  }

  /* Rect of the combined areas. */
  const bool vertical = SCREEN_DIR_IS_VERTICAL(dir);
  rctf combined{};
  combined.xmin = vertical ? MAX2(sa1->totrct.xmin, sa2->totrct.xmin) :
                             MIN2(sa1->totrct.xmin, sa2->totrct.xmin);
  combined.xmax = vertical ? MIN2(sa1->totrct.xmax, sa2->totrct.xmax) :
                             MAX2(sa1->totrct.xmax, sa2->totrct.xmax);
  combined.ymin = vertical ? MIN2(sa1->totrct.ymin, sa2->totrct.ymin) :
                             MAX2(sa1->totrct.ymin, sa2->totrct.ymin);
  combined.ymax = vertical ? MAX2(sa1->totrct.ymax, sa2->totrct.ymax) :
                             MIN2(sa1->totrct.ymax, sa2->totrct.ymax);

  uint pos_id = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_blend(GPU_BLEND_ALPHA);

  /* Highlight source (sa1) within combined area. */
  immUniformColor4fv(blender::float4{1.0f, 1.0f, 1.0f, 0.10f});
  immRectf(pos_id,
           MAX2(sa1->totrct.xmin, combined.xmin),
           MAX2(sa1->totrct.ymin, combined.ymin),
           MIN2(sa1->totrct.xmax, combined.xmax),
           MIN2(sa1->totrct.ymax, combined.ymax));

  /* Highlight destination (sa2) within combined area. */
  immUniformColor4fv(blender::float4{0.0f, 0.0f, 0.0f, 0.25f});
  immRectf(pos_id,
           MAX2(sa2->totrct.xmin, combined.xmin),
           MAX2(sa2->totrct.ymin, combined.ymin),
           MIN2(sa2->totrct.xmax, combined.xmax),
           MIN2(sa2->totrct.ymax, combined.ymax));

  int offset1;
  int offset2;
  area_getoffsets(sa1, sa2, dir, &offset1, &offset2);
  if (offset1 < 0 || offset2 > 0) {
    /* Show partial areas that will be closed. */
    immUniformColor4fv(blender::float4{0.0f, 0.0f, 0.0f, 0.8f});
    if (vertical) {
      if (sa1->totrct.xmin < combined.xmin) {
        immRectf(pos_id, sa1->totrct.xmin, sa1->totrct.ymin, combined.xmin, sa1->totrct.ymax);
      }
      if (sa2->totrct.xmin < combined.xmin) {
        immRectf(pos_id, sa2->totrct.xmin, sa2->totrct.ymin, combined.xmin, sa2->totrct.ymax);
      }
      if (sa1->totrct.xmax > combined.xmax) {
        immRectf(pos_id, combined.xmax, sa1->totrct.ymin, sa1->totrct.xmax, sa1->totrct.ymax);
      }
      if (sa2->totrct.xmax > combined.xmax) {
        immRectf(pos_id, combined.xmax, sa2->totrct.ymin, sa2->totrct.xmax, sa2->totrct.ymax);
      }
    }
    else {
      if (sa1->totrct.ymin < combined.ymin) {
        immRectf(pos_id, sa1->totrct.xmin, combined.ymin, sa1->totrct.xmax, sa1->totrct.ymin);
      }
      if (sa2->totrct.ymin < combined.ymin) {
        immRectf(pos_id, sa2->totrct.xmin, combined.ymin, sa2->totrct.xmax, sa2->totrct.ymin);
      }
      if (sa1->totrct.ymax > combined.ymax) {
        immRectf(pos_id, sa1->totrct.xmin, sa1->totrct.ymax, sa1->totrct.xmax, combined.ymax);
      }
      if (sa2->totrct.ymax > combined.ymax) {
        immRectf(pos_id, sa2->totrct.xmin, sa2->totrct.ymax, sa2->totrct.xmax, combined.ymax);
      }
    }
  }

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  /* Outline the combined area. */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(&combined, false, 7 * U.pixelsize, blender::float4{1.0f, 1.0f, 1.0f, 0.8f});
}

void screen_draw_split_preview(ScrArea *area, const eScreenAxis dir_axis, const float fac)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Split-point. */
  GPU_blend(GPU_BLEND_ALPHA);

  immUniformColor4ub(255, 255, 255, 100);

  immBegin(GPU_PRIM_LINES, 2);

  if (dir_axis == SCREEN_AXIS_H) {
    const float y = (1 - fac) * area->totrct.ymin + fac * area->totrct.ymax;

    immVertex2f(pos, area->totrct.xmin, y);
    immVertex2f(pos, area->totrct.xmax, y);

    immEnd();

    immUniformColor4ub(0, 0, 0, 100);

    immBegin(GPU_PRIM_LINES, 2);

    immVertex2f(pos, area->totrct.xmin, y + 1);
    immVertex2f(pos, area->totrct.xmax, y + 1);

    immEnd();
  }
  else {
    BLI_assert(dir_axis == SCREEN_AXIS_V);
    const float x = (1 - fac) * area->totrct.xmin + fac * area->totrct.xmax;

    immVertex2f(pos, x, area->totrct.ymin);
    immVertex2f(pos, x, area->totrct.ymax);

    immEnd();

    immUniformColor4ub(0, 0, 0, 100);

    immBegin(GPU_PRIM_LINES, 2);

    immVertex2f(pos, x + 1, area->totrct.ymin);
    immVertex2f(pos, x + 1, area->totrct.ymax);

    immEnd();
  }

  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

/* -------------------------------------------------------------------- */
/* Screen Thumbnail Preview */

/**
 * Calculates a scale factor to squash the preview for \a screen into a rectangle
 * of given size and aspect.
 */
static void screen_preview_scale_get(
    const bScreen *screen, float size_x, float size_y, const float asp[2], float r_scale[2])
{
  float max_x = 0, max_y = 0;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    max_x = MAX2(max_x, area->totrct.xmax);
    max_y = MAX2(max_y, area->totrct.ymax);
  }
  r_scale[0] = (size_x * asp[0]) / max_x;
  r_scale[1] = (size_y * asp[1]) / max_y;
}

static void screen_preview_draw_areas(const bScreen *screen,
                                      const float scale[2],
                                      const float col[4],
                                      const float ofs_between_areas)
{
  const float ofs_h = ofs_between_areas * 0.5f;
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(col);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    rctf rect{};
    rect.xmin = area->totrct.xmin * scale[0] + ofs_h;
    rect.xmax = area->totrct.xmax * scale[0] - ofs_h;
    rect.ymin = area->totrct.ymin * scale[1] + ofs_h;
    rect.ymax = area->totrct.ymax * scale[1] - ofs_h;

    immBegin(GPU_PRIM_TRI_FAN, 4);
    immVertex2f(pos, rect.xmin, rect.ymin);
    immVertex2f(pos, rect.xmax, rect.ymin);
    immVertex2f(pos, rect.xmax, rect.ymax);
    immVertex2f(pos, rect.xmin, rect.ymax);
    immEnd();
  }

  immUnbindProgram();
}

static void screen_preview_draw(const bScreen *screen, int size_x, int size_y)
{
  const float asp[2] = {1.0f, 0.8f}; /* square previews look a bit ugly */
  /* could use theme color (tui.wcol_menu_item.text),
   * but then we'd need to regenerate all previews when changing. */
  const float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float scale[2];

  wmOrtho2(0.0f, size_x, 0.0f, size_y);
  /* center */
  GPU_matrix_push();
  GPU_matrix_identity_set();
  GPU_matrix_translate_2f(size_x * (1.0f - asp[0]) * 0.5f, size_y * (1.0f - asp[1]) * 0.5f);

  screen_preview_scale_get(screen, size_x, size_y, asp, scale);
  screen_preview_draw_areas(screen, scale, col, 1.5f);

  GPU_matrix_pop();
}
