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

#include "BLI_resource_scope.hh"

#include "spreadsheet_data_source.hh"
#include "spreadsheet_layout.hh"

namespace blender::ed::spreadsheet {

Span<int64_t> spreadsheet_filter_rows(const SpaceSpreadsheet &sspreadsheet,
                                      const SpreadsheetLayout &spreadsheet_layout,
                                      const DataSource &data_source,
                                      ResourceScope &scope);

SpreadsheetRowFilter *spreadsheet_row_filter_new();
SpreadsheetRowFilter *spreadsheet_row_filter_copy(const SpreadsheetRowFilter *src_row_filter);
void spreadsheet_row_filter_free(SpreadsheetRowFilter *row_filter);

}  // namespace blender::ed::spreadsheet
