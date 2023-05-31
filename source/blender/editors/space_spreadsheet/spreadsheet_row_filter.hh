/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_resource_scope.hh"

#include "spreadsheet_data_source.hh"
#include "spreadsheet_layout.hh"

namespace blender::ed::spreadsheet {

IndexMask spreadsheet_filter_rows(const SpaceSpreadsheet &sspreadsheet,
                                  const SpreadsheetLayout &spreadsheet_layout,
                                  const DataSource &data_source,
                                  ResourceScope &scope);

SpreadsheetRowFilter *spreadsheet_row_filter_new();
SpreadsheetRowFilter *spreadsheet_row_filter_copy(const SpreadsheetRowFilter *src_row_filter);
void spreadsheet_row_filter_free(SpreadsheetRowFilter *row_filter);

}  // namespace blender::ed::spreadsheet
