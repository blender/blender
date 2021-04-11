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

#include "BLI_function_ref.hh"

#include "spreadsheet_column.hh"
#include "spreadsheet_column_values.hh"

namespace blender::ed::spreadsheet {

/**
 * This class is subclassed to implement different data sources for the spreadsheet. A data source
 * provides the information that should be displayed. It is not concerned with how data is laid
 * out in the spreadsheet editor exactly.
 */
class DataSource {
 public:
  virtual ~DataSource();

  /**
   * Calls the callback with all the column ids that should be displayed as long as the user does
   * not manually add or remove columns. The column id can be stack allocated. Therefore, the
   * callback should not keep a reference to it (and copy it instead).
   */
  virtual void foreach_default_column_ids(FunctionRef<void(const SpreadsheetColumnID &)> fn) const
  {
    UNUSED_VARS(fn);
  }

  /**
   * Returns the column values the given column id. If no data exists for this id, null is
   * returned.
   */
  virtual std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID &column_id) const
  {
    UNUSED_VARS(column_id);
    return {};
  }

  /**
   * Returns the number of rows in columns returned by #get_column_values.
   */
  virtual int tot_rows() const
  {
    return 0;
  }
};

}  // namespace blender::ed::spreadsheet
