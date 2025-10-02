/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_space_types.h"

#include "BLI_generic_virtual_array.hh"
#include "BLI_string_ref.hh"

namespace blender::ed::spreadsheet {

eSpreadsheetColumnValueType cpp_type_to_column_type(const CPPType &type);

enum class ColumnValueDisplayHint {
  None,
  Bytes,
};

/**
 * This represents a column in a spreadsheet. It has a name and provides a value for all the cells
 * in the column.
 */
class ColumnValues final {
 protected:
  std::string name_;

  GVArray data_;
  ColumnValueDisplayHint display_hint_;

 public:
  ColumnValues(std::string name,
               GVArray data,
               const ColumnValueDisplayHint display_hint = ColumnValueDisplayHint::None)
      : name_(std::move(name)), data_(std::move(data)), display_hint_(display_hint)
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

  ColumnValueDisplayHint display_hint() const
  {
    return display_hint_;
  }

  /**
   * Get a good column width for the column name and values.
   *
   * \param max_sample_size: If provided, only a subset of the column values are inspected to
   * determine the width. This is useful when there are lots of rows to avoid unnecessarily long
   * computations in drawing code. If provided, there is also an enforced minimum width to avoid
   * very narrow columns when the sampled values all happen to be very short.
   */
  float fit_column_width_px(const std::optional<int64_t> &max_sample_size = std::nullopt) const;

  /** Same as above, but only takes the values into account (ignoring the name). */
  float fit_column_values_width_px(
      const std::optional<int64_t> &max_sample_size = std::nullopt) const;
};

}  // namespace blender::ed::spreadsheet
