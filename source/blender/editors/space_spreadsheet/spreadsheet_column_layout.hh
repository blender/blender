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

#include <variant>

#include "spreadsheet_draw.hh"

struct Object;
struct Collection;

namespace blender::ed::spreadsheet {

struct ObjectCellValue {
  const Object *object;
};

struct CollectionCellValue {
  const Collection *collection;
};

/**
 * This is a small type that can hold the value of a cell in a spreadsheet. This type allows us to
 * decouple the drawing of individual cells from the code that generates the data to be displayed.
 */
class CellValue {
 public:
  /* The implementation just uses a `std::variant` for simplicity. It can be encapsulated better,
   * but it's not really worth the complixity for now. */
  using VariantType =
      std::variant<std::monostate, int, float, bool, ObjectCellValue, CollectionCellValue>;

  VariantType value;
};

/**
 * This represents a column in a spreadsheet. It has a name and provides a value for all the cells
 * in the column.
 */
class SpreadsheetColumn {
 protected:
  std::string name_;

 public:
  SpreadsheetColumn(std::string name) : name_(std::move(name))
  {
  }

  virtual ~SpreadsheetColumn() = default;

  virtual void get_value(int index, CellValue &r_cell_value) const = 0;

  StringRefNull name() const
  {
    return name_;
  }

  /* The default width of newly created columns, in UI units. */
  float default_width = 0.0f;
};

/* Utility class for the function below. */
template<typename GetValueF> class LambdaSpreadsheetColumn : public SpreadsheetColumn {
 private:
  GetValueF get_value_;

 public:
  LambdaSpreadsheetColumn(std::string name, GetValueF get_value)
      : SpreadsheetColumn(std::move(name)), get_value_(std::move(get_value))
  {
  }

  void get_value(int index, CellValue &r_cell_value) const final
  {
    get_value_(index, r_cell_value);
  }
};

/* Utility function that simplifies creating a spreadsheet column from a lambda function. */
template<typename GetValueF>
std::unique_ptr<SpreadsheetColumn> spreadsheet_column_from_function(std::string name,
                                                                    GetValueF get_value)
{
  return std::make_unique<LambdaSpreadsheetColumn<GetValueF>>(std::move(name),
                                                              std::move(get_value));
}

/* This contains information required to create a spreadsheet drawer from columns. */
struct SpreadsheetColumnLayout {
  Vector<const SpreadsheetColumn *> columns;
  Span<int64_t> row_indices;
  int tot_rows = 0;
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_column_layout(
    const SpreadsheetColumnLayout &column_layout);

}  // namespace blender::ed::spreadsheet
