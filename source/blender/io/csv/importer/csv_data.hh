/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup csv
 */

#pragma once

#include <string>

#include "BLI_array.hh"
#include "BLI_generic_array.hh"

#include "DNA_customdata_types.h"

struct PointCloud;

namespace blender::io::csv {

class CsvData {
 private:
  Array<GArray<>> data;

  int64_t rows_num;
  int64_t columns_num;

  Array<std::string> column_names;
  Array<eCustomDataType> column_types;

 public:
  CsvData(int64_t rows_num, Span<std::string> column_names, Span<eCustomDataType> column_types);

  PointCloud *to_point_cloud() const;

  template<typename T> void set_data(int64_t row_index, int64_t col_index, const T value)
  {
    GMutableSpan mutable_span = data[col_index].as_mutable_span();
    MutableSpan typed_mutable_span = mutable_span.typed<T>();
    typed_mutable_span[row_index] = value;
  }

  eCustomDataType get_column_type(int64_t col_index) const
  {
    return column_types[col_index];
  }

  StringRef get_column_name(int64_t col_index) const
  {
    return column_names[col_index];
  }
};

}  // namespace blender::io::csv
