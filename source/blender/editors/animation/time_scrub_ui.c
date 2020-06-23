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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edanimation
 */

#include "BKE_context.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_time_scrub_ui.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_timecode.h"

#include "RNA_access.h"

static void get_time_scrub_region_rect(const ARegion *region, rcti *rect)
{
  rect->xmin = 0;
  rect->xmax = region->winx;
  rect->ymax = region->winy;
  rect->ymin = rect->ymax - UI_TIME_SCRUB_MARGIN_Y;
}

static int get_centered_text_y(const rcti *rect)
{
  return BLI_rcti_cent_y(rect) - UI_DPI_FAC * 4;
}

static void draw_background(const rcti *rect)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformThemeColor(TH_TIME_SCRUB_BACKGROUND);

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  GPU_blend(false);

  immUnbindProgram();
}

static void get_current_time_str(
    const Scene *scene, bool display_seconds, int frame, uint max_len, char *r_str)
{
  if (display_seconds) {
    BLI_timecode_string_from_time(r_str, max_len, 0, FRA2TIME(frame), FPS, U.timecode_style);
  }
  else {
    BLI_snprintf(r_str, max_len, "%d", frame);
  }
}

static void draw_current_frame(const Scene *scene,
                               bool display_seconds,
                               const View2D *v2d,
                               const rcti *scrub_region_rect,
                               int current_frame,
                               float sub_frame,
                               bool draw_line)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  int frame_x = UI_view2d_view_to_region_x(v2d, current_frame);

  char frame_str[64];
  get_current_time_str(scene, display_seconds, current_frame, sizeof(frame_str), frame_str);
  float text_width = UI_fontstyle_string_width(fstyle, frame_str);
  float box_width = MAX2(text_width + 8 * UI_DPI_FAC, 24 * UI_DPI_FAC);
  float box_padding = 3 * UI_DPI_FAC;

  float bg_color[4];
  UI_GetThemeColorShade4fv(TH_CFRAME, -5, bg_color);

  if (draw_line) {
    /* Draw vertical line to from the bottom of the current frame box to the bottom of the screen.
     */
    const float subframe_x = UI_view2d_view_to_region_x(v2d, current_frame + sub_frame);
    GPU_line_width(2.0f);
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColor(TH_CFRAME);
    immBegin(GPU_PRIM_LINES, 2);

    immVertex2f(pos, subframe_x, scrub_region_rect->ymax - box_padding);
    immVertex2f(pos, subframe_x, 0.0f);
    immEnd();
    immUnbindProgram();
  }

  UI_draw_roundbox_corner_set(UI_CNR_ALL);

  UI_draw_roundbox_3fv_alpha(true,
                             frame_x - box_width / 2 + U.pixelsize / 2,
                             scrub_region_rect->ymin + box_padding,
                             frame_x + box_width / 2 + U.pixelsize / 2,
                             scrub_region_rect->ymax - box_padding,
                             4 * UI_DPI_FAC,
                             bg_color,
                             1.0f);

  UI_GetThemeColorShade4fv(TH_CFRAME, 5, bg_color);
  UI_draw_roundbox_aa(false,
                      frame_x - box_width / 2 + U.pixelsize / 2,
                      scrub_region_rect->ymin + box_padding,
                      frame_x + box_width / 2 + U.pixelsize / 2,
                      scrub_region_rect->ymax - box_padding,
                      4 * UI_DPI_FAC,
                      bg_color);

  uchar text_color[4];
  UI_GetThemeColor4ubv(TH_HEADER_TEXT_HI, text_color);
  UI_fontstyle_draw_simple(fstyle,
                           frame_x - text_width / 2 + U.pixelsize / 2,
                           get_centered_text_y(scrub_region_rect),
                           frame_str,
                           text_color);
}

void ED_time_scrub_draw_current_frame(const ARegion *region,
                                      const Scene *scene,
                                      bool display_seconds,
                                      bool draw_line)
{
  const View2D *v2d = &region->v2d;
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  rcti scrub_region_rect;
  get_time_scrub_region_rect(region, &scrub_region_rect);

  draw_current_frame(scene,
                     display_seconds,
                     v2d,
                     &scrub_region_rect,
                     scene->r.cfra,
                     scene->r.subframe,
                     draw_line);
  GPU_matrix_pop_projection();
}

void ED_time_scrub_draw(const ARegion *region,
                        const Scene *scene,
                        bool display_seconds,
                        bool discrete_frames)
{
  const View2D *v2d = &region->v2d;

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  rcti scrub_region_rect;
  get_time_scrub_region_rect(region, &scrub_region_rect);

  draw_background(&scrub_region_rect);

  rcti numbers_rect = scrub_region_rect;
  numbers_rect.ymin = get_centered_text_y(&scrub_region_rect) - 4 * UI_DPI_FAC;
  if (discrete_frames) {
    UI_view2d_draw_scale_x__discrete_frames_or_seconds(
        region, v2d, &numbers_rect, scene, display_seconds, TH_TEXT);
  }
  else {
    UI_view2d_draw_scale_x__frames_or_seconds(
        region, v2d, &numbers_rect, scene, display_seconds, TH_TEXT);
  }

  GPU_matrix_pop_projection();
}

bool ED_time_scrub_event_in_region(const ARegion *region, const wmEvent *event)
{
  rcti rect = region->winrct;
  rect.ymin = rect.ymax - UI_TIME_SCRUB_MARGIN_Y;
  return BLI_rcti_isect_pt(&rect, event->x, event->y);
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

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColor(TH_BACK);
  immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  immUnbindProgram();

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);

  PointerRNA ptr;
  RNA_pointer_create(&CTX_wm_screen(C)->id, &RNA_DopeSheet, dopesheet, &ptr);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "filter_text");

  int padding = 2 * UI_DPI_FAC;
  uiDefAutoButR(block,
                &ptr,
                prop,
                -1,
                "",
                ICON_NONE,
                rect.xmin + padding,
                rect.ymin + padding,
                BLI_rcti_size_x(&rect) - 2 * padding,
                BLI_rcti_size_y(&rect) - 2 * padding);

  UI_block_end(C, block);
  UI_block_draw(C, block);

  GPU_matrix_pop_projection();
}
