/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include "BKE_context.hh"
#include "BKE_scene.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "ED_time_scrub_ui.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "DNA_scene_types.h"

#include "BLI_math_base.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_timecode.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

void ED_time_scrub_region_rect_get(const ARegion *region, rcti *r_rect)
{
  r_rect->xmin = 0;
  r_rect->xmax = region->winx;
  r_rect->ymax = region->winy;
  r_rect->ymin = r_rect->ymax - UI_TIME_SCRUB_MARGIN_Y;
}

static int get_centered_text_y(const rcti *rect)
{
  return BLI_rcti_cent_y(rect) - UI_SCALE_FAC * 4;
}

static void draw_background(const rcti *rect)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformThemeColor(TH_TIME_SCRUB_BACKGROUND);

  GPU_blend(GPU_BLEND_ALPHA);

  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

static void get_current_time_str(
    const Scene *scene, bool display_seconds, int frame, char *r_str, uint str_maxncpy)
{
  if (display_seconds) {
    BLI_timecode_string_from_time(
        r_str, str_maxncpy, -1, FRA2TIME(frame), scene->frames_per_second(), U.timecode_style);
  }
  else {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%d", frame);
  }
}

static void draw_current_frame(const Scene *scene,
                               bool display_seconds,
                               const View2D *v2d,
                               const rcti *scrub_region_rect,
                               int current_frame,
                               bool display_stalk = true)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  const int frame_x = UI_view2d_view_to_region_x(v2d, current_frame);
  const float subframe_x = UI_view2d_view_to_region_x(v2d, BKE_scene_ctime_get(scene));
  char frame_str[64];
  get_current_time_str(scene, display_seconds, current_frame, frame_str, sizeof(frame_str));
  const float text_width = UI_fontstyle_string_width(fstyle, frame_str);
  const float text_padding = 4.0f * UI_SCALE_FAC;
  const float box_min_width = 24.0f * UI_SCALE_FAC;
  const float box_width = std::max(text_width + (2.0f * text_padding), box_min_width);
  const float box_margin = 2.0f * UI_SCALE_FAC;
  const float shadow_width = UI_SCALE_FAC;
  const float tri_top = ceil(scrub_region_rect->ymin + box_margin);
  const float tri_half_width = 6.0f * UI_SCALE_FAC;
  const float tri_height = 6.0f * UI_SCALE_FAC;
  rctf rect{};
  uint pos;

  float fg_color[4];
  UI_GetThemeColor4fv(TH_CFRAME, fg_color);
  float bg_color[4];
  UI_GetThemeColorShade4fv(TH_BACK, -20, bg_color);

  if (display_stalk) {
    /* Shadow for triangle below frame box. */
    GPUVertFormat *format = immVertexFormat();
    pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    GPU_blend(GPU_BLEND_ALPHA);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_polygon_smooth(true);
    immUniformColor4fv(bg_color);
    immBegin(GPU_PRIM_TRIS, 3);
    const float diag_offset = 0.4f * UI_SCALE_FAC;
    immVertex2f(pos, floor(frame_x - tri_half_width - shadow_width - diag_offset), tri_top);
    immVertex2f(pos, floor(frame_x + tri_half_width + shadow_width + 1.0f + diag_offset), tri_top);
    immVertex2f(pos, frame_x + 0.5f, tri_top - tri_height - diag_offset - shadow_width);
    GPU_polygon_smooth(false);
    immEnd();
    immUnbindProgram();

    /* Vertical line. */
    rect.xmin = floor(subframe_x - U.pixelsize) - shadow_width;
    rect.xmax = floor(subframe_x + U.pixelsize + 1.0f) + shadow_width;
    rect.ymin = 0.0f;
    rect.ymax = ceil(scrub_region_rect->ymax - box_margin + shadow_width);
    UI_draw_roundbox_4fv_ex(&rect, fg_color, nullptr, 1.0f, bg_color, shadow_width, 0.0f);
  }

  /* Box. */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  const float box_corner_radius = 4.0f * UI_SCALE_FAC;
  rect.xmin = frame_x - (box_width / 2.0f);
  rect.xmax = frame_x + (box_width / 2.0f) + 1.0f;
  rect.ymin = floor(scrub_region_rect->ymin + (box_margin - shadow_width));
  rect.ymax = ceil(scrub_region_rect->ymax - box_margin + shadow_width);
  UI_draw_roundbox_4fv_ex(
      &rect, fg_color, nullptr, 1.0f, bg_color, shadow_width, box_corner_radius);

  /* Frame number text. */
  uchar text_color[4];
  UI_GetThemeColor4ubv(TH_HEADER_TEXT_HI, text_color);
  const int y = BLI_rcti_cent_y(scrub_region_rect) - int(fstyle->points * UI_SCALE_FAC * 0.38f);
  UI_fontstyle_draw_simple(fstyle, frame_x - (text_width / 2.0f), y, frame_str, text_color);

  if (display_stalk) {
    /* Triangular base under frame number. */
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_polygon_smooth(true);
    immBegin(GPU_PRIM_TRIS, 3);
    immUniformColor4fv(fg_color);
    immVertex2f(pos, frame_x - tri_half_width, tri_top);
    immVertex2f(pos, frame_x + tri_half_width + 1, tri_top);
    immVertex2f(pos, frame_x + 0.5f, tri_top - tri_height);
    immEnd();
    immUnbindProgram();
    GPU_polygon_smooth(false);
    GPU_blend(GPU_BLEND_NONE);
  }
}

void ED_time_scrub_draw_current_frame(const ARegion *region,
                                      const Scene *scene,
                                      bool display_seconds,
                                      bool display_stalk)
{
  const View2D *v2d = &region->v2d;
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  rcti scrub_region_rect;
  ED_time_scrub_region_rect_get(region, &scrub_region_rect);

  draw_current_frame(
      scene, display_seconds, v2d, &scrub_region_rect, scene->r.cfra, display_stalk);
  GPU_matrix_pop_projection();
}

void ED_time_scrub_draw(const ARegion *region,
                        const Scene *scene,
                        bool display_seconds,
                        bool discrete_frames,
                        const int base)
{
  const View2D *v2d = &region->v2d;

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  rcti scrub_region_rect;
  ED_time_scrub_region_rect_get(region, &scrub_region_rect);

  draw_background(&scrub_region_rect);

  rcti numbers_rect = scrub_region_rect;
  numbers_rect.ymin = get_centered_text_y(&scrub_region_rect) - 4 * UI_SCALE_FAC;
  if (discrete_frames) {
    UI_view2d_draw_scale_x__discrete_frames_or_seconds(
        region, v2d, &numbers_rect, scene, display_seconds, TH_TIME_SCRUB_TEXT, base);
  }
  else {
    UI_view2d_draw_scale_x__frames_or_seconds(
        region, v2d, &numbers_rect, scene, display_seconds, TH_TIME_SCRUB_TEXT, base);
  }

  GPU_matrix_pop_projection();
}

rcti ED_time_scrub_clamp_scroller_mask(const rcti &scroller_mask)
{
  rcti clamped_mask = scroller_mask;
  clamped_mask.ymax -= UI_TIME_SCRUB_MARGIN_Y;
  return clamped_mask;
}

bool ED_time_scrub_event_in_region(const ARegion *region, const wmEvent *event)
{
  rcti rect = region->winrct;
  rect.ymin = rect.ymax - UI_TIME_SCRUB_MARGIN_Y;
  return BLI_rcti_isect_pt_v(&rect, event->xy);
}

bool ED_time_scrub_event_in_region_poll(const wmWindow * /*win*/,
                                        const ScrArea * /*area*/,
                                        const ARegion *region,
                                        const wmEvent *event)
{
  return ED_time_scrub_event_in_region(region, event);
}

void ED_time_scrub_channel_search_draw(const bContext *C, ARegion *region, bDopeSheet *dopesheet)
{
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  rcti rect;
  rect.xmin = 0;
  rect.xmax = region->winx;
  rect.ymin = region->winy - UI_TIME_SCRUB_MARGIN_Y;
  rect.ymax = region->winy;

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_BACK);
  immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  immUnbindProgram();

  PointerRNA ptr = RNA_pointer_create_discrete(&CTX_wm_screen(C)->id, &RNA_DopeSheet, dopesheet);

  const uiStyle *style = UI_style_get_dpi();
  const float padding_x = 2 * UI_SCALE_FAC;
  const float padding_y = UI_SCALE_FAC;

  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Header,
                                               rect.xmin + padding_x,
                                               rect.ymin + UI_UNIT_Y + padding_y,
                                               BLI_rcti_size_x(&rect) - 2 * padding_x,
                                               1,
                                               0,
                                               style);
  layout.scale_y_set((UI_UNIT_Y - padding_y) / UI_UNIT_Y);
  blender::ui::block_layout_set_current(block, &layout);
  UI_block_align_begin(block);
  layout.prop(&ptr, "filter_text", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(&ptr, "use_filter_invert", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  UI_block_align_end(block);
  blender::ui::block_layout_resolve(block);

  /* Make sure the events are consumed from the search and don't reach other UI blocks since this
   * is drawn on top of animation-channels. */
  UI_block_flag_enable(block, UI_BLOCK_CLIP_EVENTS);
  UI_block_bounds_set_normal(block, 0);
  UI_block_end(C, block);
  UI_block_draw(C, block);

  GPU_matrix_pop_projection();
}
