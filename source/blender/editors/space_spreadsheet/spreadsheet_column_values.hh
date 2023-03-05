/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_space_types.h"

#include "BLI_generic_virtual_array.hh"
#include "BLI_string_ref.hh"

namespace blender::ed::spreadsheet {

eSpreadsheetColumnValueType cpp_type_to_column_type(const CPPType &type);

/**
 * This represents a column in a spreadsheet. It has a name and provides a value for all the cells
 * in the column.
 */
class ColumnValues final {
 protected:
  std::string name_;

  GVArray data_;

 public:
  ColumnValues(std::string name, GVArray data) : name_(std::move(name)), data_(std::move(data))
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

  const GVArray &data() const
  {
    return data_;
  }

  /* The default width of newly created columns, in UI units. */
  float default_width = 0.0f;
};

}  // namespace blender::ed::spreadsheet
