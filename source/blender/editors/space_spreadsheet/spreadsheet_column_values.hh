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

#include "BLI_string_ref.hh"

#include "spreadsheet_cell_value.hh"

namespace blender::ed::spreadsheet {

/**
 * This represents a column in a spreadsheet. It has a name and provides a value for all the cells
 * in the column.
 */
class ColumnValues {
 protected:
  std::string name_;
  int size_;

 public:
  ColumnValues(std::string name, const int size) : name_(std::move(name)), size_(size)
  {
  }

  virtual ~ColumnValues() = default;

  virtual void get_value(int index, CellValue &r_cell_value) const = 0;

  StringRefNull name() const
  {
    return name_;
  }

  int size() const
  {
    return size_;
  }

  /* The default width of newly created columns, in UI units. */
  float default_width = 0.0f;
};

/* Utility class for the function below. */
template<typename GetValueF> class LambdaColumnValues : public ColumnValues {
 private:
  GetValueF get_value_;

 public:
  LambdaColumnValues(std::string name, int size, GetValueF get_value)
      : ColumnValues(std::move(name), size), get_value_(std::move(get_value))
  {
  }

  void get_value(int index, CellValue &r_cell_value) const final
  {
    get_value_(index, r_cell_value);
  }
};

/* Utility function that simplifies creating a spreadsheet column from a lambda function. */
template<typename GetValueF>
std::unique_ptr<ColumnValues> column_values_from_function(std::string name,
                                                          const int size,
                                                          GetValueF get_value,
                                                          const float default_width = 0.0f)
{
  std::unique_ptr<ColumnValues> column_values = std::make_unique<LambdaColumnValues<GetValueF>>(
      std::move(name), size, std::move(get_value));
  column_values->default_width = default_width;
  return column_values;
}

static constexpr float default_float_column_width = 3;
static constexpr float default_float2_column_width = 2 * default_float_column_width;
static constexpr float default_float3_column_width = 3 * default_float_column_width;
static constexpr float default_color_column_width = 4 * default_float_column_width;

}  // namespace blender::ed::spreadsheet
