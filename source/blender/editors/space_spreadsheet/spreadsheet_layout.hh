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
  Span<int64_t> row_indices;
  int index_column_width = 100;
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_layout(
    const SpreadsheetLayout &spreadsheet_layout);

}  // namespace blender::ed::spreadsheet
