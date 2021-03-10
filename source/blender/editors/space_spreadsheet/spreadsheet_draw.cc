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
 */

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_immediate.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_rect.h"

#include "spreadsheet_draw.hh"

namespace blender::ed::spreadsheet {

SpreadsheetDrawer::SpreadsheetDrawer()
{
  left_column_width = UI_UNIT_X * 2;
  top_row_height = UI_UNIT_Y * 1.1f;
  row_height = UI_UNIT_Y;
}

SpreadsheetDrawer::~SpreadsheetDrawer() = default;

void SpreadsheetDrawer::draw_top_row_cell(int UNUSED(column_index),
                                          const CellDrawParams &UNUSED(params)) const
{
}

void SpreadsheetDrawer::draw_left_column_cell(int UNUSED(row_index),
                                              const CellDrawParams &UNUSED(params)) const
{
}

void SpreadsheetDrawer::draw_content_cell(int UNUSED(row_index),
                                          int UNUSED(column_index),
                                          const CellDrawParams &UNUSED(params)) const
{
}

int SpreadsheetDrawer::column_width(int UNUSED(column_index)) const
{
  return 5 * UI_UNIT_X;
}

static void draw_index_column_background(const uint pos,
                                         const ARegion *region,
                                         const SpreadsheetDrawer &drawer)
{
  immUniformThemeColorShade(TH_BACK, 11);
  immRecti(pos, 0, region->winy - drawer.top_row_height, drawer.left_column_width, 0);
}

static void draw_alternating_row_overlay(const uint pos,
                                         const int scroll_offset_y,
                                         const ARegion *region,
                                         const SpreadsheetDrawer &drawer)
{
  immUniformThemeColor(TH_ROW_ALTERNATE);
  GPU_blend(GPU_BLEND_ALPHA);
  BLI_assert(drawer.row_height > 0);
  const int row_pair_height = drawer.row_height * 2;
  const int row_top_y = region->winy - drawer.top_row_height - scroll_offset_y % row_pair_height;
  for (const int i : IndexRange(region->winy / row_pair_height + 1)) {
    int x_left = 0;
    int x_right = region->winx;
    int y_top = row_top_y - i * row_pair_height - drawer.row_height;
    int y_bottom = y_top - drawer.row_height;
    y_top = std::min(y_top, region->winy - drawer.top_row_height);
    y_bottom = std::min(y_bottom, region->winy - drawer.top_row_height);
    immRecti(pos, x_left, y_top, x_right, y_bottom);
  }
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_top_row_background(const uint pos,
                                    const ARegion *region,
                                    const SpreadsheetDrawer &drawer)
{
  immUniformThemeColorShade(TH_BACK, 11);
  immRecti(pos, 0, region->winy, region->winx, region->winy - drawer.top_row_height);
}

static void draw_separator_lines(const uint pos,
                                 const int scroll_offset_x,
                                 const ARegion *region,
                                 const SpreadsheetDrawer &drawer)
{
  immUniformThemeColorShade(TH_BACK, -11);

  immBeginAtMost(GPU_PRIM_LINES, drawer.tot_columns * 2 + 4);

  /* Left column line. */
  immVertex2i(pos, drawer.left_column_width, region->winy);
  immVertex2i(pos, drawer.left_column_width, 0);

  /* Top row line. */
  immVertex2i(pos, 0, region->winy - drawer.top_row_height);
  immVertex2i(pos, region->winx, region->winy - drawer.top_row_height);

  /* Column separator lines. */
  int line_x = drawer.left_column_width - scroll_offset_x;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    const int column_width = drawer.column_width(column_index);
    line_x += column_width;
    if (line_x >= drawer.left_column_width) {
      immVertex2i(pos, line_x, region->winy);
      immVertex2i(pos, line_x, 0);
    }
  }
  immEnd();
}

static void get_visible_rows(const SpreadsheetDrawer &drawer,
                             const ARegion *region,
                             const int scroll_offset_y,
                             int *r_first_row,
                             int *r_max_visible_rows)
{
  *r_first_row = -scroll_offset_y / drawer.row_height;
  *r_max_visible_rows = region->winy / drawer.row_height + 1;
}

static void draw_left_column_content(const int scroll_offset_y,
                                     const bContext *C,
                                     ARegion *region,
                                     const SpreadsheetDrawer &drawer)
{
  GPU_scissor_test(true);
  GPU_scissor(0, 0, drawer.left_column_width, region->winy - drawer.top_row_height);

  uiBlock *left_column_block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);
  int first_row, max_visible_rows;
  get_visible_rows(drawer, region, scroll_offset_y, &first_row, &max_visible_rows);
  for (const int row_index : IndexRange(first_row, max_visible_rows)) {
    if (row_index >= drawer.tot_rows) {
      break;
    }
    CellDrawParams params;
    params.block = left_column_block;
    params.xmin = 0;
    params.ymin = region->winy - drawer.top_row_height - (row_index + 1) * drawer.row_height -
                  scroll_offset_y;
    params.width = drawer.left_column_width;
    params.height = drawer.row_height;
    drawer.draw_left_column_cell(row_index, params);
  }

  UI_block_end(C, left_column_block);
  UI_block_draw(C, left_column_block);

  GPU_scissor_test(false);
}

static void draw_top_row_content(const bContext *C,
                                 ARegion *region,
                                 const SpreadsheetDrawer &drawer,
                                 const int scroll_offset_x)
{
  GPU_scissor_test(true);
  GPU_scissor(drawer.left_column_width + 1,
              region->winy - drawer.top_row_height,
              region->winx - drawer.left_column_width,
              drawer.top_row_height);

  uiBlock *first_row_block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);

  int left_x = drawer.left_column_width - scroll_offset_x;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    const int column_width = drawer.column_width(column_index);
    const int right_x = left_x + column_width;

    CellDrawParams params;
    params.block = first_row_block;
    params.xmin = left_x;
    params.ymin = region->winy - drawer.top_row_height;
    params.width = column_width;
    params.height = drawer.top_row_height;
    drawer.draw_top_row_cell(column_index, params);

    left_x = right_x;
  }

  UI_block_end(C, first_row_block);
  UI_block_draw(C, first_row_block);

  GPU_scissor_test(false);
}

static void draw_cell_contents(const bContext *C,
                               ARegion *region,
                               const SpreadsheetDrawer &drawer,
                               const int scroll_offset_x,
                               const int scroll_offset_y)
{
  GPU_scissor_test(true);
  GPU_scissor(drawer.left_column_width + 1,
              0,
              region->winx - drawer.left_column_width,
              region->winy - drawer.top_row_height);

  uiBlock *cells_block = UI_block_begin(C, region, __func__, UI_EMBOSS_NONE);

  int first_row, max_visible_rows;
  get_visible_rows(drawer, region, scroll_offset_y, &first_row, &max_visible_rows);

  int left_x = drawer.left_column_width - scroll_offset_x;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    const int column_width = drawer.column_width(column_index);
    const int right_x = left_x + column_width;

    if (right_x >= drawer.left_column_width && left_x <= region->winx) {
      for (const int row_index : IndexRange(first_row, max_visible_rows)) {
        if (row_index >= drawer.tot_rows) {
          break;
        }

        CellDrawParams params;
        params.block = cells_block;
        params.xmin = left_x;
        params.ymin = region->winy - drawer.top_row_height - (row_index + 1) * drawer.row_height -
                      scroll_offset_y;
        params.width = column_width;
        params.height = drawer.row_height;
        drawer.draw_content_cell(row_index, column_index, params);
      }
    }

    left_x = right_x;
  }

  UI_block_end(C, cells_block);
  UI_block_draw(C, cells_block);

  GPU_scissor_test(false);
}

static void update_view2d_tot_rect(const SpreadsheetDrawer &drawer,
                                   ARegion *region,
                                   const int row_amount)
{
  int column_width_sum = 0;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    column_width_sum += drawer.column_width(column_index);
  }

  UI_view2d_totRect_set(&region->v2d,
                        column_width_sum + drawer.left_column_width,
                        row_amount * drawer.row_height + drawer.top_row_height);
}

void draw_spreadsheet_in_region(const bContext *C,
                                ARegion *region,
                                const SpreadsheetDrawer &drawer)
{
  update_view2d_tot_rect(drawer, region, drawer.tot_rows);

  UI_ThemeClearColor(TH_BACK);

  View2D *v2d = &region->v2d;
  const int scroll_offset_y = v2d->cur.ymax;
  const int scroll_offset_x = v2d->cur.xmin;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  draw_index_column_background(pos, region, drawer);
  draw_alternating_row_overlay(pos, scroll_offset_y, region, drawer);
  draw_top_row_background(pos, region, drawer);
  draw_separator_lines(pos, scroll_offset_x, region, drawer);

  immUnbindProgram();

  draw_left_column_content(scroll_offset_y, C, region, drawer);
  draw_top_row_content(C, region, drawer, scroll_offset_x);
  draw_cell_contents(C, region, drawer, scroll_offset_x, scroll_offset_y);

  rcti scroller_mask;
  BLI_rcti_init(&scroller_mask,
                drawer.left_column_width,
                region->winx,
                0,
                region->winy - drawer.top_row_height);
  UI_view2d_scrollers_draw(v2d, &scroller_mask);
}

}  // namespace blender::ed::spreadsheet
