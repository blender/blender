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

#pragma once

#include "BLI_vector.hh"

struct uiBlock;
struct rcti;
struct bContext;
struct ARegion;

namespace blender::ed::spreadsheet {

struct CellDrawParams {
  uiBlock *block;
  int xmin, ymin;
  int width, height;
};

class SpreadsheetDrawer {
 public:
  int left_column_width;
  int top_row_height;
  int row_height;
  int tot_rows = 0;
  int tot_columns = 0;

  SpreadsheetDrawer();
  virtual ~SpreadsheetDrawer();

  virtual void draw_top_row_cell(int column_index, const CellDrawParams &params) const;

  virtual void draw_left_column_cell(int row_index, const CellDrawParams &params) const;

  virtual void draw_content_cell(int row_index,
                                 int column_index,
                                 const CellDrawParams &params) const;

  virtual int column_width(int column_index) const;
};

void draw_spreadsheet_in_region(const bContext *C,
                                ARegion *region,
                                const SpreadsheetDrawer &spreadsheet_drawer);

}  // namespace blender::ed::spreadsheet
