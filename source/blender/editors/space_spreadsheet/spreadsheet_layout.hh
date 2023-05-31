/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "spreadsheet_column_values.hh"
#include "spreadsheet_draw.hh"

namespace blender::ed::spreadsheet {

/* Layout information for a single column. */
struct ColumnLayout {
  const ColumnValues *values;
  int width;
};

/* Layout information for the entire spreadsheet. */
struct SpreadsheetLayout {
  Vector<ColumnLayout> columns;
  IndexMask row_indices;
  int index_column_width = 100;
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_layout(
    const SpreadsheetLayout &spreadsheet_layout);

}  // namespace blender::ed::spreadsheet
