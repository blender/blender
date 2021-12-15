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

#include "BLI_string_ref.hh"

#include "FN_generic_virtual_array.hh"

namespace blender::ed::spreadsheet {

struct CellDrawParams;

eSpreadsheetColumnValueType cpp_type_to_column_type(const fn::CPPType &type);

/**
 * This represents a column in a spreadsheet. It has a name and provides a value for all the cells
 * in the column.
 */
class ColumnValues final {
 protected:
  std::string name_;

  fn::GVArray data_;

 public:
  ColumnValues(std::string name, fn::GVArray data) : name_(std::move(name)), data_(std::move(data))
  {
    /* The array should not be empty. */
    BLI_assert(data_);
  }

  virtual ~ColumnValues() = default;

  eSpreadsheetColumnValueType type() const
  {
    return cpp_type_to_column_type(data_.type());
  }

  StringRefNull name() const
  {
    return name_;
  }

  int size() const
  {
    return data_.size();
  }

  const fn::GVArray &data() const
  {
    return data_;
  }

  /* The default width of newly created columns, in UI units. */
  float default_width = 0.0f;
};

}  // namespace blender::ed::spreadsheet
