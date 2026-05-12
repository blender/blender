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

namespace blender {

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
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformThemeColor(TH_TIME_SCRUB_BACKGROUND);

  GPU_blend(GPU_BLEND_ALPHA);

  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

static void get_current_time_str(
    const Scene *scene, bool display_seconds, const float frame, char *r_str, uint str_maxncpy)
{
  if (display_seconds) {
    const float frame_len = scene->r.framelen > 0 ? scene->r.framelen : 1.0;
    const float seconds = (frame / float(scene->frames_per_second())) / frame_len;
    BLI_timecode_string_from_time(
        r_str, str_maxncpy, -1, seconds, scene->frames_per_second(), U.timecode_style);
  }
  else if (scene->r.flag & SCER_SHOW_SUBFRAME) {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%.02f", frame);
  }
  else {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%d", int(frame));
  }
}

struct PlayheadDimensions {
  float text_width;
  float text_padding;

  float box_width;
  float box_margin;
  float shadow_width;

  float tri_top;
  float tri_half_width;
  float tri_height;
};

static PlayheadDimensions get_playhead_dimensions(const Scene *scene,
                                                  const rcti *scrub_region_rect,
                                                  const float current_frame,
                                                  const bool display_seconds)
{
  PlayheadDimensions dimensions;
  constexpr int max_frame_string_len = 64;
  char frame_str[max_frame_string_len];
  get_current_time_str(scene, display_seconds, current_frame, frame_str, max_frame_string_len);

  dimensions.text_width = ui::fontstyle_string_width(UI_FSTYLE_WIDGET, frame_str);
  dimensions.text_padding = 4.0f * UI_SCALE_FAC;
  const float box_min_width = 24.0f * UI_SCALE_FAC;
  dimensions.box_width = std::max(dimensions.text_width + (2.0f * dimensions.text_padding),
                                  box_min_width);
  dimensions.box_margin = 2.0f * UI_SCALE_FAC;
  dimensions.shadow_width = UI_SCALE_FAC;
  dimensions.tri_top = ceil(scrub_region_rect->ymin + dimensions.box_margin);
  dimensions.tri_half_width = 6.0f * UI_SCALE_FAC;
  dimensions.tri_height = 6.0f * UI_SCALE_FAC;
  return dimensions;
}

static void draw_playhead_stalk(const float region_x,
                                const rcti *scrub_region_rect,
                                const PlayheadDimensions &dimensions,
                                const float fg_color[4],
                                const float bg_color[4])
{
  float shadow_width = dimensions.shadow_width;

  /* Shadow for triangle below frame box. */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_polygon_smooth(true);
  immUniformColor4fv(bg_color);
  immBegin(GPU_PRIM_TRIS, 3);
  const float diag_offset = 0.4f * UI_SCALE_FAC;
  immVertex2f(pos,
              floor(region_x - dimensions.tri_half_width - shadow_width - diag_offset),
              dimensions.shadow_width);
  immVertex2f(pos,
              floor(region_x + dimensions.tri_half_width + shadow_width + 1.0f + diag_offset),
              dimensions.shadow_width);
  immVertex2f(pos,
              region_x + 0.5f,
              dimensions.shadow_width - dimensions.tri_height - diag_offset - shadow_width);
  immEnd();
  immUnbindProgram();
  GPU_polygon_smooth(false);
  GPU_blend(GPU_BLEND_NONE);

  rctf rect{};
  /* Vertical line. */
  if (UI_SCALE_FAC < 0.91f) {
    shadow_width = 1.0f;
    rect.xmin = floor(region_x) - shadow_width;
    rect.xmax = rect.xmin + U.pixelsize + shadow_width + shadow_width;
  }
  else {
    rect.xmin = floor(region_x - U.pixelsize) - shadow_width;
    rect.xmax = floor(region_x + U.pixelsize + 1.0f) + shadow_width;
  }
  rect.ymin = 0.0f;
  rect.ymax = scrub_region_rect->ymin;
  ui::draw_roundbox_4fv_ex(&rect, fg_color, nullptr, 1.0f, bg_color, shadow_width, 0.0f);
}

static void draw_playhead_box(const float region_x,
                              const char frame_str[64],
                              const rcti *scrub_region_rect,
                              const PlayheadDimensions &dimensions,
                              const float fg_color[4],
                              const float bg_color[4])
{
  rctf rect{};
  draw_roundbox_corner_set(ui::CNR_ALL);
  const float box_corner_radius = 4.0f * UI_SCALE_FAC;
  rect.xmin = region_x - (dimensions.box_width / 2.0f);
  rect.xmax = region_x + (dimensions.box_width / 2.0f) + 1.0f;
  rect.ymin = floor(scrub_region_rect->ymin + (dimensions.box_margin - dimensions.shadow_width));
  rect.ymax = ceil(scrub_region_rect->ymax - dimensions.box_margin + dimensions.shadow_width);
  ui::draw_roundbox_4fv_ex(
      &rect, fg_color, nullptr, 1.0f, bg_color, dimensions.shadow_width, box_corner_radius);

  /* Frame number text. */
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  uchar text_color[4];
  ui::theme::get_color_4ubv(TH_HEADER_TEXT_HI, text_color);
  const int y = BLI_rcti_cent_y(scrub_region_rect) - int(fstyle->points * UI_SCALE_FAC * 0.38f);
  ui::fontstyle_draw_simple(
      fstyle, region_x - (dimensions.text_width / 2.0f), y, frame_str, text_color);
}

/* Draws the little triangle at the bottom of the playhead. */
static void draw_playhead_tip(const float region_x,
                              const PlayheadDimensions &dimensions,
                              const float fg_color[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  /* Triangular base under frame number. */
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_polygon_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);
  immBegin(GPU_PRIM_TRIS, 3);
  immUniformColor4fv(fg_color);
  immVertex2f(pos, region_x - dimensions.tri_half_width, dimensions.tri_top);
  immVertex2f(pos, region_x + dimensions.tri_half_width + 1, dimensions.tri_top);
  immVertex2f(pos, region_x + 0.5f, dimensions.tri_top - dimensions.tri_height);
  immEnd();
  immUnbindProgram();
  GPU_polygon_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}

/**
 * Draw a playhead with reduced opacity at the given frame.
 */
static void draw_playhead_ghost(const float frame,
                                const Scene *scene,
                                const View2D *v2d,
                                const rcti *scrub_region_rect,
                                const bool display_seconds,
                                const bool display_stalk)
{
  const float region_x = ui::view2d_view_to_region_x(v2d, frame);

  PlayheadDimensions dimensions = get_playhead_dimensions(
      scene, scrub_region_rect, frame, display_seconds);
  float fg_color[4];
  ui::theme::get_color_4fv(TH_CFRAME, fg_color);
  float bg_color[4];
  ui::theme::get_color_shade_4fv(TH_BACK, -20, bg_color);
  fg_color[3] /= 2;
  bg_color[3] /= 2;
  if (display_stalk) {
    draw_playhead_stalk(region_x, scrub_region_rect, dimensions, fg_color, bg_color);
  }

  constexpr int max_frame_string_len = 64;
  char frame_str[max_frame_string_len];
  get_current_time_str(scene, display_seconds, frame, frame_str, max_frame_string_len);
  draw_playhead_box(region_x, frame_str, scrub_region_rect, dimensions, fg_color, bg_color);

  if (display_stalk) {
    draw_playhead_tip(region_x, dimensions, fg_color);
  }
}

static void draw_current_frame(const Scene *scene,
                               bool display_seconds,
                               const View2D *v2d,
                               const rcti *scrub_region_rect,
                               bool display_stalk = true)
{
  const float current_frame = BKE_scene_frame_get(scene);
  const float region_x = ui::view2d_view_to_region_x(v2d, current_frame);

  constexpr int max_frame_string_len = 64;
  char frame_str[max_frame_string_len];
  get_current_time_str(scene, display_seconds, current_frame, frame_str, max_frame_string_len);

  PlayheadDimensions dimensions = get_playhead_dimensions(
      scene, scrub_region_rect, current_frame, display_seconds);

  float fg_color[4];
  ui::theme::get_color_4fv(TH_CFRAME, fg_color);
  float bg_color[4];
  ui::theme::get_color_shade_4fv(TH_BACK, -20, bg_color);

  if (display_stalk) {
    draw_playhead_stalk(region_x, scrub_region_rect, dimensions, fg_color, bg_color);
  }

  draw_playhead_box(region_x, frame_str, scrub_region_rect, dimensions, fg_color, bg_color);

  if (display_stalk) {
    draw_playhead_tip(region_x, dimensions, fg_color);
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

  if (scene->r.framelen != 1.0) {
    /* In case the time scale feature is active, we draw a second playhead with less opacity to
     * indicate the remapped time. */
    const float ctime = BKE_scene_ctime_get(scene);
    draw_playhead_ghost(ctime, scene, v2d, &scrub_region_rect, display_seconds, display_stalk);
  }

  draw_current_frame(scene, display_seconds, v2d, &scrub_region_rect, display_stalk);
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
  ui::view2d_draw_scale_x(region,
                          v2d,
                          &numbers_rect,
                          scene,
                          display_seconds,
                          !discrete_frames,
                          TH_TIME_SCRUB_TEXT,
                          base);

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

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_BACK);
  immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  immUnbindProgram();

  PointerRNA ptr = RNA_pointer_create_discrete(&CTX_wm_screen(C)->id, RNA_DopeSheet, dopesheet);

  const uiStyle *style = ui::style_get_dpi();
  const float padding_x = 2 * UI_SCALE_FAC;
  const float padding_y = UI_SCALE_FAC;

  ui::Block *block = block_begin(C, region, __func__, ui::EmbossType::Emboss);
  ui::Layout &layout = ui::block_layout(block,
                                        ui::LayoutDirection::Vertical,
                                        ui::LayoutType::Header,
                                        rect.xmin + padding_x,
                                        rect.ymin + UI_UNIT_Y + padding_y,
                                        BLI_rcti_size_x(&rect) - 2 * padding_x,
                                        1,
                                        0,
                                        style);
  layout.scale_y_set((UI_UNIT_Y - padding_y) / UI_UNIT_Y);
  ui::block_layout_set_current(block, &layout);
  block_align_begin(block);
  layout.prop(&ptr, "filter_text", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(&ptr, "use_filter_invert", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  block_align_end(block);
  ui::block_layout_resolve(block);

  /* Make sure the events are consumed from the search and don't reach other UI blocks since this
   * is drawn on top of animation-channels. */
  block_flag_enable(block, ui::BLOCK_CLIP_EVENTS);
  block_bounds_set_normal(block, 0);
  block_end(C, block);
  block_draw(C, block);

  GPU_matrix_pop_projection();
}

}  // namespace blender
