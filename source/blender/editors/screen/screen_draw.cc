/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include "ED_screen.hh"
#include "ED_screen_types.hh"

#include "GPU_batch_presets.hh"
#include "GPU_immediate.hh"
#include "GPU_platform.hh"
#include "GPU_state.hh"

#include "BKE_global.hh"
#include "BKE_screen.hh"

#include "BLF_api.hh"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "screen_intern.hh"

#define CORNER_RESOLUTION 3

static void do_vert_pair(blender::gpu::VertBuf *vbo, uint pos, uint *vidx, int corner, int i)
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

  /* Small offset to be able to tell inner and outer vertex apart inside the shader.
   * Edge width is specified in the shader. */
  mul_v2_fl(inter, 1.0f - 0.0001f);
  mul_v2_fl(exter, 1.0f);

  GPU_vertbuf_attr_set(vbo, pos, (*vidx)++, inter);
  GPU_vertbuf_attr_set(vbo, pos, (*vidx)++, exter);
}

static blender::gpu::Batch *batch_screen_edges_get(int *corner_len)
{
  static blender::gpu::Batch *screen_edges_batch = nullptr;

  if (screen_edges_batch == nullptr) {
    GPUVertFormat format = {0};
    uint pos = GPU_vertformat_attr_add(&format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPU_vertbuf_data_alloc(*vbo, CORNER_RESOLUTION * 2 * 4 + 2);

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

/**
 * \brief Screen edges drawing.
 */
static void drawscredge_area(const ScrArea &area, float edge_thickness)
{
  rctf rect;
  BLI_rctf_rcti_copy(&rect, &area.totrct);
  BLI_rctf_pad(&rect, edge_thickness, edge_thickness);

  blender::gpu::Batch *batch = batch_screen_edges_get(nullptr);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_AREA_BORDERS);
  GPU_batch_uniform_4fv(batch, "rect", (float *)&rect);
  GPU_batch_draw(batch);
}

void ED_screen_draw_edges(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  screen->do_draw = false;

  if (screen->state != SCREENNORMAL) {
    return;
  }

  if (BLI_listbase_is_single(&screen->areabase) && win->global_areas.areabase.first == nullptr) {
    /* Do not show edges on windows without global areas and with only one editor. */
    return;
  }

  ARegion *region = screen->active_region;
  ScrArea *active_area = nullptr;

  if (region) {
    /* Find active area from active region. */
    const int pos[2] = {BLI_rcti_cent_x(&region->winrct), BLI_rcti_cent_y(&region->winrct)};
    active_area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, pos);
  }

  if (!active_area) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      AZone *zone = ED_area_actionzone_find_xy(area, win->eventstate->xy);
      /* Get area from action zone, if not scroll-bar. */
      if (zone && zone->type != AZONE_REGION_SCROLL) {
        active_area = area;
        break;
      }
    }
  }

  if (G.moving & G_TRANSFORM_WM) {
    active_area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, win->eventstate->xy);
    /* We don't want an active area when resizing, otherwise outline for active area flickers, see:
     * #136314. */
    if (active_area && !BLI_listbase_is_empty(&win->drawcalls)) {
      active_area = nullptr;
    }
  }

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
  GPU_scissor_test(true);

  float col[4];
  UI_GetThemeColor4fv(TH_EDITOR_BORDER, col);

  const float edge_thickness = float(U.border_width) * UI_SCALE_FAC;

  /* Entire width of the evaluated outline as far as the shader is concerned. */
  const float shader_scale = edge_thickness + EDITORRADIUS;
  const float corner_coverage[10] = {
      0.144f, 0.25f, 0.334f, 0.40f, 0.455, 0.5, 0.538, 0.571, 0.6, 0.625f};
  const float shader_width = corner_coverage[U.border_width - 1];

  GPU_blend(GPU_BLEND_ALPHA);

  int verts_per_corner = 0;
  blender::gpu::Batch *batch = batch_screen_edges_get(&verts_per_corner);

  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_AREA_BORDERS);
  GPU_batch_uniform_1i(batch, "cornerLen", verts_per_corner);
  GPU_batch_uniform_1f(batch, "scale", shader_scale);
  GPU_batch_uniform_1f(batch, "width", shader_width);
  GPU_batch_uniform_4fv(batch, "color", col);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    drawscredge_area(*area, edge_thickness);
  }

  float outline1[4];
  float outline2[4];
  rctf bounds;
  /* Outset by 1/2 pixel, regardless of UI scale or pixel size. #141550. */
  const float padding = 0.5f;
  UI_GetThemeColor4fv(TH_EDITOR_OUTLINE, outline1);
  UI_GetThemeColor4fv(TH_EDITOR_OUTLINE_ACTIVE, outline2);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    BLI_rctf_rcti_copy(&bounds, &area->totrct);
    BLI_rctf_pad(&bounds, padding, padding);
    UI_draw_roundbox_4fv_ex(&bounds,
                            nullptr,
                            nullptr,
                            1.0f,
                            (area == active_area) ? outline2 : outline1,
                            U.pixelsize,
                            EDITORRADIUS);
  }

  GPU_blend(GPU_BLEND_NONE);
  GPU_scissor_test(false);
}

void screen_draw_move_highlight(const wmWindow *win,
                                bScreen *screen,
                                eScreenAxis dir_axis,
                                float anim_factor)
{
  rctf rect = {SHRT_MAX, SHRT_MIN, SHRT_MAX, SHRT_MIN};

  LISTBASE_FOREACH (const ScrEdge *, edge, &screen->edgebase) {
    if (edge->v1->editflag && edge->v2->editflag) {
      if (dir_axis == SCREEN_AXIS_H) {
        rect.xmin = std::min({rect.xmin, float(edge->v1->vec.x), float(edge->v2->vec.x)});
        rect.xmax = std::max({rect.xmax, float(edge->v1->vec.x), float(edge->v2->vec.x)});
        rect.ymin = rect.ymax = float(edge->v1->vec.y);
      }
      else {
        rect.ymin = std::min({rect.ymin, float(edge->v1->vec.y), float(edge->v2->vec.y)});
        rect.ymax = std::max({rect.ymax, float(edge->v1->vec.y), float(edge->v2->vec.y)});
        rect.xmin = rect.xmax = float(edge->v1->vec.x);
      }
    };
  }

  rcti window_rect;
  WM_window_screen_rect_calc(win, &window_rect);
  const float offset = U.border_width * UI_SCALE_FAC;
  const float width = std::min(2.0f * offset, 5.0f * UI_SCALE_FAC);
  if (dir_axis == SCREEN_AXIS_H) {
    BLI_rctf_pad(&rect, -offset, width);
  }
  else {
    BLI_rctf_pad(&rect, width, -offset);
  }

  float inner[4] = {1.0f, 1.0f, 1.0f, 0.4f * anim_factor};
  float outline[4];
  UI_GetThemeColor4fv(TH_EDITOR_BORDER, outline);
  outline[3] *= anim_factor;

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv_ex(
      &rect, inner, nullptr, 1.0f, outline, width - U.pixelsize, 2.5f * UI_SCALE_FAC);
}

void screen_draw_region_scale_highlight(ARegion *region)
{
  rctf rect;
  BLI_rctf_rcti_copy(&rect, &region->winrct);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);

  switch (region->alignment) {
    case RGN_ALIGN_RIGHT:
      rect.xmax = rect.xmin - U.pixelsize;
      rect.xmin = rect.xmax - (4.0f * U.pixelsize);
      rect.ymax -= EDITORRADIUS;
      rect.ymin += EDITORRADIUS;
      break;
    case RGN_ALIGN_LEFT:
      rect.xmin = rect.xmax + U.pixelsize;
      rect.xmax = rect.xmin + (4.0f * U.pixelsize);
      rect.ymax -= EDITORRADIUS;
      rect.ymin += EDITORRADIUS;
      break;
    case RGN_ALIGN_TOP:
      rect.ymax = rect.ymin - U.pixelsize;
      rect.ymin = rect.ymax - (4.0f * U.pixelsize);
      rect.xmax -= EDITORRADIUS;
      rect.xmin += EDITORRADIUS;
      break;
    case RGN_ALIGN_BOTTOM:
      rect.ymin = rect.ymax + U.pixelsize;
      rect.ymax = rect.ymin + (4.0f * U.pixelsize);
      rect.xmax -= EDITORRADIUS;
      rect.xmin += EDITORRADIUS;
      break;
    default:
      return;
  }

  float inner[4] = {1.0f, 1.0f, 1.0f, 0.4f};
  float outline[4] = {0.0f, 0.0f, 0.0f, 0.3f};
  UI_draw_roundbox_4fv_ex(
      &rect, inner, nullptr, 1.0f, outline, 1.0f * U.pixelsize, 2.5f * UI_SCALE_FAC);
}

static void screen_draw_area_drag_tip(
    const wmWindow *win, int x, int y, const ScrArea *source, const std::string &hint)
{
  const char *area_name = IFACE_(ED_area_name(source).c_str());
  const uiFontStyle *fstyle = UI_FSTYLE_TOOLTIP;
  const bTheme *btheme = UI_GetTheme();
  const uiWidgetColors *wcol = &btheme->tui.wcol_tooltip;
  float col_fg[4], col_bg[4];
  rgba_uchar_to_float(col_fg, wcol->text);
  rgba_uchar_to_float(col_bg, wcol->inner);

  float scale = fstyle->points * UI_SCALE_FAC / UI_DEFAULT_TOOLTIP_POINTS;
  BLF_size(fstyle->uifont_id, UI_DEFAULT_TOOLTIP_POINTS * scale);

  const float margin = scale * 4.0f;
  const float icon_width = (scale * ICON_DEFAULT_WIDTH / 1.4f);
  const float icon_gap = scale * 3.0f;
  const float line_gap = scale * 5.0f;
  const int lheight = BLF_height_max(fstyle->uifont_id);
  const int descent = BLF_descender(fstyle->uifont_id);
  const float line1_len = BLF_width(fstyle->uifont_id, hint.c_str(), hint.size());
  const float line2_len = BLF_width(fstyle->uifont_id, area_name, BLF_DRAW_STR_DUMMY_MAX);
  const float width = margin + std::max(line1_len, line2_len + icon_width + icon_gap) + margin;
  const float height = margin + lheight + line_gap + lheight + margin;

  /* Position of this hint relative to the mouse position. */
  const int left = std::min(x + int(5.0f * UI_SCALE_FAC),
                            WM_window_native_pixel_x(win) - int(width));
  const int top = std::max(y - int(7.0f * UI_SCALE_FAC), int(height));

  rctf rect;
  rect.xmin = left;
  rect.xmax = left + width;
  rect.ymax = top;
  rect.ymin = top - height;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(&rect, true, wcol->roundness * U.widget_unit, col_bg);

  UI_icon_draw_ex(left + margin,
                  top - height + margin + (1.0f * scale),
                  ED_area_icon(source),
                  1.4f / scale,
                  1.0f,
                  0.0f,
                  wcol->text,
                  true,
                  UI_NO_ICON_OVERLAY_TEXT);

  BLF_size(fstyle->uifont_id, UI_DEFAULT_TOOLTIP_POINTS * scale);
  BLF_color4fv(fstyle->uifont_id, col_fg);

  BLF_position(fstyle->uifont_id, left + margin, top - margin - lheight + (2.0f * scale), 0.0f);
  BLF_draw(fstyle->uifont_id, hint.c_str(), hint.size());

  BLF_position(fstyle->uifont_id,
               left + margin + icon_width + icon_gap,
               top - height + margin - descent,
               0.0f);
  BLF_draw(fstyle->uifont_id, area_name, BLF_DRAW_STR_DUMMY_MAX);
}

static void screen_draw_area_closed(int xmin, int xmax, int ymin, int ymax, float anim_factor)
{
  /* Darken the area. */
  rctf rect = {float(xmin), float(xmax), float(ymin), float(ymax)};
  float darken[4] = {0.0f, 0.0f, 0.0f, 0.7f * anim_factor};
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv_ex(&rect, darken, nullptr, 1.0f, nullptr, U.pixelsize, EDITORRADIUS);
}

void screen_draw_join_highlight(
    const wmWindow *win, ScrArea *sa1, ScrArea *sa2, eScreenDir dir, float anim_factor)
{
  if (dir == SCREEN_DIR_NONE || !sa2) {
    /* Darken source if docking. Done here because it might be a different window.
     * Do not animate this as we don't want to reset every time we change areas. */
    screen_draw_area_closed(
        sa1->totrct.xmin, sa1->totrct.xmax, sa1->totrct.ymin, sa1->totrct.ymax, 1.0f);
    return;
  }

  /* Rect of the combined areas. */
  const bool vertical = SCREEN_DIR_IS_VERTICAL(dir);
  rctf combined{};
  combined.xmin = vertical ? std::max(sa1->totrct.xmin, sa2->totrct.xmin) :
                             std::min(sa1->totrct.xmin, sa2->totrct.xmin);
  combined.xmax = vertical ? std::min(sa1->totrct.xmax, sa2->totrct.xmax) :
                             std::max(sa1->totrct.xmax, sa2->totrct.xmax);
  combined.ymin = vertical ? std::min(sa1->totrct.ymin, sa2->totrct.ymin) :
                             std::max(sa1->totrct.ymin, sa2->totrct.ymin);
  combined.ymax = vertical ? std::max(sa1->totrct.ymax, sa2->totrct.ymax) :
                             std::min(sa1->totrct.ymax, sa2->totrct.ymax);

  int offset1;
  int offset2;
  area_getoffsets(sa1, sa2, dir, &offset1, &offset2);
  if (offset1 < 0 || offset2 > 0) {
    /* Show partial areas that will be closed. */
    if (vertical) {
      if (sa1->totrct.xmin < combined.xmin) {
        screen_draw_area_closed(
            sa1->totrct.xmin, combined.xmin, sa1->totrct.ymin, sa1->totrct.ymax, anim_factor);
      }
      if (sa2->totrct.xmin < combined.xmin) {
        screen_draw_area_closed(
            sa2->totrct.xmin, combined.xmin, sa2->totrct.ymin, sa2->totrct.ymax, anim_factor);
      }
      if (sa1->totrct.xmax > combined.xmax) {
        screen_draw_area_closed(
            combined.xmax, sa1->totrct.xmax, sa1->totrct.ymin, sa1->totrct.ymax, anim_factor);
      }
      if (sa2->totrct.xmax > combined.xmax) {
        screen_draw_area_closed(
            combined.xmax, sa2->totrct.xmax, sa2->totrct.ymin, sa2->totrct.ymax, anim_factor);
      }
    }
    else {
      if (sa1->totrct.ymin < combined.ymin) {
        screen_draw_area_closed(
            sa1->totrct.xmin, sa1->totrct.xmax, sa1->totrct.ymin, combined.ymin, anim_factor);
      }
      if (sa2->totrct.ymin < combined.ymin) {
        screen_draw_area_closed(
            sa2->totrct.xmin, sa2->totrct.xmax, sa2->totrct.ymin, combined.ymin, anim_factor);
      }
      if (sa1->totrct.ymax > combined.ymax) {
        screen_draw_area_closed(
            sa1->totrct.xmin, sa1->totrct.xmax, combined.ymax, sa1->totrct.ymax, anim_factor);
      }
      if (sa2->totrct.ymax > combined.ymax) {
        screen_draw_area_closed(
            sa2->totrct.xmin, sa2->totrct.xmax, combined.ymax, sa2->totrct.ymax, anim_factor);
      }
    }
  }

  /* Outline the combined area. */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  float outline[4] = {1.0f, 1.0f, 1.0f, 0.4f * anim_factor};
  float inner[4] = {1.0f, 1.0f, 1.0f, 0.10f * anim_factor};
  UI_draw_roundbox_4fv_ex(&combined, inner, nullptr, 1.0f, outline, U.pixelsize, EDITORRADIUS);

  screen_draw_area_drag_tip(
      win, win->eventstate->xy[0], win->eventstate->xy[1], sa1, IFACE_("Join Areas"));
}

static void rounded_corners(rctf rect, float color[4], int corners)
{
  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  const float rad = EDITORRADIUS;

  float vec[4][2] = {
      {0.195, 0.02},
      {0.55, 0.169},
      {0.831, 0.45},
      {0.98, 0.805},
  };
  for (int a = 0; a < 4; a++) {
    mul_v2_fl(vec[a], rad);
  }

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color);

  if (corners & UI_CNR_TOP_LEFT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmin - 1, rect.ymax);
    immVertex2f(pos, rect.xmin, rect.ymax - rad);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmin + vec[a][1], rect.ymax - rad + vec[a][0]);
    }
    immVertex2f(pos, rect.xmin + rad, rect.ymax);
    immEnd();
  }

  if (corners & UI_CNR_TOP_RIGHT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmax + 1, rect.ymax);
    immVertex2f(pos, rect.xmax - rad, rect.ymax);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmax - rad + vec[a][0], rect.ymax - vec[a][1]);
    }
    immVertex2f(pos, rect.xmax, rect.ymax - rad);
    immEnd();
  }

  if (corners & UI_CNR_BOTTOM_RIGHT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmax + 1, rect.ymin);
    immVertex2f(pos, rect.xmax, rect.ymin + rad);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmax - vec[a][1], rect.ymin + rad - vec[a][0]);
    }
    immVertex2f(pos, rect.xmax - rad, rect.ymin);
    immEnd();
  }

  if (corners & UI_CNR_BOTTOM_LEFT) {
    immBegin(GPU_PRIM_TRI_FAN, 7);
    immVertex2f(pos, rect.xmin - 1, rect.ymin);
    immVertex2f(pos, rect.xmin + rad, rect.ymin);
    for (int a = 0; a < 4; a++) {
      immVertex2f(pos, rect.xmin + rad - vec[a][0], rect.ymin + vec[a][1]);
    }
    immVertex2f(pos, rect.xmin, rect.ymin + rad);
    immEnd();
  }

  immUnbindProgram();
}

void screen_draw_dock_preview(const wmWindow *win,
                              ScrArea *source,
                              ScrArea *target,
                              AreaDockTarget dock_target,
                              float factor,
                              int x,
                              int y,
                              float anim_factor)
{
  if (dock_target == AreaDockTarget::None) {
    return;
  }

  float outline[4] = {1.0f, 1.0f, 1.0f, 0.4f * anim_factor};
  float inner[4] = {1.0f, 1.0f, 1.0f, 0.1f * anim_factor};
  float border[4];
  UI_GetThemeColor4fv(TH_EDITOR_BORDER, border);
  border[3] *= anim_factor;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  float half_line_width = float(U.border_width) * UI_SCALE_FAC;

  rctf dest;
  rctf remainder;
  BLI_rctf_rcti_copy(&dest, &target->totrct);
  BLI_rctf_rcti_copy(&remainder, &target->totrct);

  float split;
  int corners = UI_CNR_NONE;

  if (dock_target == AreaDockTarget::Right) {
    split = std::min(dest.xmin + target->winx * (1.0f - factor),
                     dest.xmax - AREAMINX * UI_SCALE_FAC);
    dest.xmin = split + half_line_width;
    remainder.xmax = split - half_line_width;
    corners = UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
  }
  else if (dock_target == AreaDockTarget::Left) {
    split = std::max(dest.xmax - target->winx * (1.0f - factor),
                     dest.xmin + AREAMINX * UI_SCALE_FAC);
    dest.xmax = split - half_line_width;
    remainder.xmin = split + half_line_width;
    corners = UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT;
  }
  else if (dock_target == AreaDockTarget::Top) {
    split = std::min(dest.ymin + target->winy * (1.0f - factor),
                     dest.ymax - HEADERY * UI_SCALE_FAC);
    dest.ymin = split + half_line_width;
    remainder.ymax = split - half_line_width;
    corners = UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT;
  }
  else if (dock_target == AreaDockTarget::Bottom) {
    split = std::max(dest.ymax - target->winy * (1.0f - factor),
                     dest.ymin + HEADERY * UI_SCALE_FAC);
    dest.ymax = split - half_line_width;
    remainder.ymin = split + half_line_width;
    corners = UI_CNR_TOP_RIGHT | UI_CNR_TOP_LEFT;
  }

  rounded_corners(dest, border, corners);
  UI_draw_roundbox_4fv_ex(&dest, inner, nullptr, 1.0f, outline, U.pixelsize, EDITORRADIUS);

  if (dock_target != AreaDockTarget::Center) {
    /* Darken the split position itself. */
    if (ELEM(dock_target, AreaDockTarget::Right, AreaDockTarget::Left)) {
      dest.xmin = split - half_line_width;
      dest.xmax = split + half_line_width;
    }
    else {
      dest.ymin = split - half_line_width;
      dest.ymax = split + half_line_width;
    }
    UI_draw_roundbox_4fv(&dest, true, 0.0f, border);
  }

  screen_draw_area_drag_tip(win,
                            x,
                            y,
                            source,
                            dock_target == AreaDockTarget::Center ? IFACE_("Replace this area") :
                                                                    IFACE_("Move area here"));
}

void screen_draw_split_preview(ScrArea *area, const eScreenAxis dir_axis, const float factor)
{
  float outline[4] = {1.0f, 1.0f, 1.0f, 0.4f};
  float inner[4] = {1.0f, 1.0f, 1.0f, 0.10f};
  float border[4];
  UI_GetThemeColor4fv(TH_EDITOR_BORDER, border);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);

  rctf rect;
  BLI_rctf_rcti_copy(&rect, &area->totrct);

  if (factor < 0.0001 || factor > 0.9999) {
    /* Highlight the entire area. */
    UI_draw_roundbox_4fv_ex(&rect, inner, nullptr, 1.0f, outline, U.pixelsize, EDITORRADIUS);
    return;
  }

  float x = (1 - factor) * rect.xmin + factor * rect.xmax;
  float y = (1 - factor) * rect.ymin + factor * rect.ymax;
  x = std::clamp(x, rect.xmin, rect.xmax);
  y = std::clamp(y, rect.ymin, rect.ymax);
  float half_line_width = float(U.border_width) * UI_SCALE_FAC;

  /* Outlined rectangle to left/above split position. */
  rect.xmax = (dir_axis == SCREEN_AXIS_V) ? x - half_line_width : rect.xmax;
  rect.ymax = (dir_axis == SCREEN_AXIS_H) ? y - half_line_width : rect.ymax;

  rounded_corners(rect,
                  border,
                  (dir_axis == SCREEN_AXIS_H) ? UI_CNR_TOP_RIGHT | UI_CNR_TOP_LEFT :
                                                UI_CNR_BOTTOM_RIGHT | UI_CNR_TOP_RIGHT);
  UI_draw_roundbox_4fv_ex(&rect, inner, nullptr, 1.0f, outline, U.pixelsize, EDITORRADIUS);

  /* Outlined rectangle to right/below split position. */
  if (dir_axis == SCREEN_AXIS_H) {
    rect.ymin = y + half_line_width;
    rect.ymax = area->totrct.ymax;
  }
  else {
    rect.xmin = x + half_line_width;
    rect.xmax = area->totrct.xmax;
  }

  rounded_corners(rect,
                  border,
                  (dir_axis == SCREEN_AXIS_H) ? UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT :
                                                UI_CNR_BOTTOM_LEFT | UI_CNR_TOP_LEFT);
  UI_draw_roundbox_4fv_ex(&rect, inner, nullptr, 1.0f, outline, U.pixelsize, EDITORRADIUS);

  /* Darken the split position itself. */
  if (dir_axis == SCREEN_AXIS_H) {
    rect.ymin = y - half_line_width;
    rect.ymax = y + half_line_width;
  }
  else {
    rect.xmin = x - half_line_width;
    rect.xmax = x + half_line_width;
  }
  UI_draw_roundbox_4fv(&rect, true, 0.0f, border);
}

struct AreaAnimateHighlightData {
  wmWindow *win;
  bScreen *screen;
  rctf rect;
  float inner[4];
  float outline[4];
  double start_time;
  double end_time;
  void *draw_callback;
};

static void area_animate_highlight_cb(const wmWindow * /*win*/, void *userdata)
{
  const AreaAnimateHighlightData *data = static_cast<const AreaAnimateHighlightData *>(userdata);

  double now = BLI_time_now_seconds();
  if (now > data->end_time) {
    WM_draw_cb_exit(data->win, data->draw_callback);
    MEM_freeN(const_cast<AreaAnimateHighlightData *>(data));
    data = nullptr;
    return;
  }

  const float factor = pow((now - data->start_time) / (data->end_time - data->start_time), 2);
  const bool do_inner = data->inner[3] > 0.0f;
  const bool do_outline = data->outline[3] > 0.0f;

  float inner_color[4];
  if (do_inner) {
    inner_color[0] = data->inner[0];
    inner_color[1] = data->inner[1];
    inner_color[2] = data->inner[2];
    inner_color[3] = (1.0f - factor) * data->inner[3];
  }

  float outline_color[4];
  if (do_outline) {
    outline_color[0] = data->outline[0];
    outline_color[1] = data->outline[1];
    outline_color[2] = data->outline[2];
    outline_color[3] = (1.0f - factor) * data->outline[3];
  }

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv_ex(&data->rect,
                          do_inner ? inner_color : nullptr,
                          nullptr,
                          1.0f,
                          do_outline ? outline_color : nullptr,
                          U.pixelsize,
                          EDITORRADIUS);

  data->screen->do_refresh = true;
}

void screen_animate_area_highlight(wmWindow *win,
                                   bScreen *screen,
                                   const rcti *rect,
                                   float inner[4],
                                   float outline[4],
                                   float seconds)
{
  /* Disabling for now, see #147487. This can cause memory leaks since the
   * data is only freed when the animation completes, which might not happen
   * during automated tests. Freeing wmWindow->drawcalls on window close might
   * be enough, but will have to be investigated. */
  return;

  AreaAnimateHighlightData *data = MEM_callocN<AreaAnimateHighlightData>(
      "screen_animate_area_highlight");
  data->win = win;
  data->screen = screen;
  BLI_rctf_rcti_copy(&data->rect, rect);
  if (inner) {
    copy_v4_v4(data->inner, inner);
  }
  if (outline) {
    copy_v4_v4(data->outline, outline);
  }
  data->start_time = BLI_time_now_seconds();
  data->end_time = data->start_time + seconds;
  data->draw_callback = WM_draw_cb_activate(win, area_animate_highlight_cb, data);
}
