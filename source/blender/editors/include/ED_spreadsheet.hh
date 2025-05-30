/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct ID;
struct SpaceSpreadsheet;
struct SpreadsheetTable;

namespace blender::ed::spreadsheet {

ID *get_current_id(const SpaceSpreadsheet *sspreadsheet);

SpreadsheetTable *get_active_table(SpaceSpreadsheet &sspreadsheet);
const SpreadsheetTable *get_active_table(const SpaceSpreadsheet &sspreadsheet);

}  // namespace blender::ed::spreadsheet
