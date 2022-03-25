/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_space_types.h"

namespace blender::ed::spreadsheet {

SpreadsheetContext *spreadsheet_context_new(eSpaceSpreadsheet_ContextType type);
SpreadsheetContext *spreadsheet_context_copy(const SpreadsheetContext *old_context);
void spreadsheet_context_free(SpreadsheetContext *context);

}  // namespace blender::ed::spreadsheet
