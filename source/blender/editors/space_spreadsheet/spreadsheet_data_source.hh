/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
   *
   * The `is_extra` argument indicates that this column is special and should be drawn as the first
   * column. (This can be made a bit more generic in the future when necessary.)
   */
  virtual void foreach_default_column_ids(
      FunctionRef<void(const SpreadsheetColumnID &, bool is_extra)> /*fn*/) const
  {
  }

  /**
   * Returns the column values the given column id. If no data exists for this id, null is
   * returned.
   */
  virtual std::unique_ptr<ColumnValues> get_column_values(
      const SpreadsheetColumnID & /*column_id*/) const
  {
    return {};
  }

  /**
   * Returns true if the data source has the ability to limit visible rows
   * by user interface selection status.
   */
  virtual bool has_selection_filter() const
  {
    return false;
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
