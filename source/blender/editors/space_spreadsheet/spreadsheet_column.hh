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
