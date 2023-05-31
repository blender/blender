/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_space_types.h"

#include "BLI_hash.hh"

namespace blender {
template<> struct DefaultHash<SpreadsheetColumnID> {
  uint64_t operator()(const SpreadsheetColumnID &column_id) const
  {
    return get_default_hash(StringRef(column_id.name));
  }
};
}  // namespace blender

inline bool operator==(const SpreadsheetColumnID &a, const SpreadsheetColumnID &b)
{
  using blender::StringRef;
  return StringRef(a.name) == StringRef(b.name);
}

namespace blender::ed::spreadsheet {

SpreadsheetColumnID *spreadsheet_column_id_new();
SpreadsheetColumnID *spreadsheet_column_id_copy(const SpreadsheetColumnID *src_column_id);
void spreadsheet_column_id_free(SpreadsheetColumnID *column_id);

SpreadsheetColumn *spreadsheet_column_new(SpreadsheetColumnID *column_id);
SpreadsheetColumn *spreadsheet_column_copy(const SpreadsheetColumn *src_column);
void spreadsheet_column_assign_runtime_data(SpreadsheetColumn *column,
                                            eSpreadsheetColumnValueType data_type,
                                            const StringRefNull display_name);
void spreadsheet_column_free(SpreadsheetColumn *column);

}  // namespace blender::ed::spreadsheet
