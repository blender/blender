/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

struct ARegion;
struct bContext;
struct uiBlock;

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
