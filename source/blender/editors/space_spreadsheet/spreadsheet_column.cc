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

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_hash.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "spreadsheet_column.hh"

namespace blender::ed::spreadsheet {

SpreadsheetColumnID *spreadsheet_column_id_new()
{
  SpreadsheetColumnID *column_id = (SpreadsheetColumnID *)MEM_callocN(sizeof(SpreadsheetColumnID),
                                                                      __func__);
  return column_id;
}

SpreadsheetColumnID *spreadsheet_column_id_copy(const SpreadsheetColumnID *src_column_id)
{
  SpreadsheetColumnID *new_column_id = spreadsheet_column_id_new();
  new_column_id->name = BLI_strdup(src_column_id->name);
  return new_column_id;
}

void spreadsheet_column_id_free(SpreadsheetColumnID *column_id)
{
  if (column_id->name != nullptr) {
    MEM_freeN(column_id->name);
  }
  MEM_freeN(column_id);
}

SpreadsheetColumn *spreadsheet_column_new(SpreadsheetColumnID *column_id)
{
  SpreadsheetColumn *column = (SpreadsheetColumn *)MEM_callocN(sizeof(SpreadsheetColumn),
                                                               __func__);
  column->id = column_id;
  return column;
}

SpreadsheetColumn *spreadsheet_column_copy(const SpreadsheetColumn *src_column)
{
  SpreadsheetColumnID *new_column_id = spreadsheet_column_id_copy(src_column->id);
  SpreadsheetColumn *new_column = spreadsheet_column_new(new_column_id);
  return new_column;
}

void spreadsheet_column_free(SpreadsheetColumn *column)
{
  spreadsheet_column_id_free(column->id);
  MEM_freeN(column);
}

}  // namespace blender::ed::spreadsheet
