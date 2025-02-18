/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup csv
 */

#include <optional>

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"

#include "BLI_fileops.hh"
#include "BLI_generic_span.hh"
#include "BLI_vector.hh"

#include "IO_csv.hh"
#include "IO_string_utils.hh"

namespace blender::io::csv {

static Vector<StringRef> parse_column_names(const StringRef line)
{
  Vector<StringRef> columns;
  const char delim = ',';
  const char *start = line.begin(), *end = line.end();
  const char *cell_start = start, *cell_end = start;

  int64_t delim_index = line.find_first_of(delim);

  while (delim_index != StringRef::not_found) {
    cell_end = start + delim_index;

    columns.append_as(cell_start, cell_end);

    cell_start = cell_end + 1;
    delim_index = line.find_first_of(delim, delim_index + 1);
  }

  columns.append_as(cell_start, end);

  return columns;
}

static std::optional<eCustomDataType> get_column_type(const char *start, const char *end)
{
  bool success = false;

  int _val_int = 0;
  try_parse_int(start, end, 0, success, _val_int);

  if (success) {
    return CD_PROP_INT32;
  }

  float _val_float = 0.0f;
  try_parse_float(start, end, 0.0f, success, _val_float);

  if (success) {
    return CD_PROP_FLOAT;
  }

  return std::nullopt;
}

static bool get_column_types(const StringRef line, Vector<eCustomDataType> &column_types)
{
  const char delim = ',';
  const char *start = line.begin(), *end = line.end();
  const char *cell_start = start, *cell_end = start;

  int64_t delim_index = line.find_first_of(delim);

  while (delim_index != StringRef::not_found) {
    cell_end = start + delim_index;

    std::optional<eCustomDataType> column_type = get_column_type(cell_start, cell_end);
    if (!column_type.has_value()) {
      return false;
    }
    column_types.append(column_type.value());

    cell_start = cell_end + 1;
    delim_index = line.find_first_of(delim, delim_index + 1);
  }

  std::optional<eCustomDataType> column_type = get_column_type(cell_start, end);
  if (!column_type.has_value()) {
    return false;
  }
  column_types.append(column_type.value());

  return true;
}

static int64_t get_row_count(StringRef buffer)
{
  int64_t row_count = 1;

  while (!buffer.is_empty()) {
    read_next_line(buffer);
    row_count++;
  }

  return row_count;
}

static void parse_csv_cell(const Span<GMutableSpan> data,
                           const Span<eCustomDataType> types,
                           const Span<StringRef> column_names,
                           const int64_t row_index,
                           const int64_t col_index,
                           const char *start,
                           const char *end,
                           const CSVImportParams &import_params)
{
  bool success = false;

  switch (types[col_index]) {
    case CD_PROP_INT32: {
      int value = 0;
      try_parse_int(start, end, 0, success, value);
      data[col_index].typed<int>()[row_index] = value;
      if (!success) {
        StringRef column_name = column_names[col_index];
        BKE_reportf(import_params.reports,
                    RPT_ERROR,
                    "CSV Import: file '%s' has an unexpected value at row %d for column %s of "
                    "type Integer",
                    import_params.filepath,
                    int(row_index),
                    std::string(column_name).c_str());
      }
      break;
    }
    case CD_PROP_FLOAT: {
      float value = 0.0f;
      try_parse_float(start, end, 0.0f, success, value);
      data[col_index].typed<float>()[row_index] = value;
      if (!success) {
        StringRef column_name = column_names[col_index];
        BKE_reportf(import_params.reports,
                    RPT_ERROR,
                    "CSV Import: file '%s' has an unexpected value at row %d for column %s of "
                    "type Float",
                    import_params.filepath,
                    int(row_index),
                    std::string(column_name).c_str());
      }
      break;
    }
    default: {
      StringRef column_name = column_names[col_index];
      BKE_reportf(import_params.reports,
                  RPT_ERROR,
                  "CSV Import: file '%s' has an unsupported value at row %d for column %s",
                  import_params.filepath,
                  int(row_index),
                  std::string(column_name).c_str());
      break;
    }
  }
}

static void parse_csv_line(const Span<GMutableSpan> data,
                           const Span<eCustomDataType> types,
                           const Span<StringRef> column_names,
                           int64_t row_index,
                           const StringRef line,
                           const CSVImportParams &import_params)
{
  const char delim = ',';
  const char *start = line.begin(), *end = line.end();
  const char *cell_start = start, *cell_end = start;

  int64_t col_index = 0;

  int64_t delim_index = line.find_first_of(delim);

  while (delim_index != StringRef::not_found) {
    cell_end = start + delim_index;

    parse_csv_cell(
        data, types, column_names, row_index, col_index, cell_start, cell_end, import_params);
    col_index++;

    cell_start = cell_end + 1;
    delim_index = line.find_first_of(delim, delim_index + 1);
  }

  parse_csv_cell(data, types, column_names, row_index, col_index, cell_start, end, import_params);
}

static void parse_csv_data(const Span<GMutableSpan> data,
                           const Span<eCustomDataType> types,
                           const Span<StringRef> column_names,
                           StringRef buffer,
                           const CSVImportParams &import_params)
{
  int64_t row_index = 0;
  while (!buffer.is_empty()) {
    const StringRef line = read_next_line(buffer);

    parse_csv_line(data, types, column_names, row_index, line, import_params);

    row_index++;
  }
}

PointCloud *import_csv_as_point_cloud(const CSVImportParams &import_params)
{
  size_t buffer_len;
  void *buffer = BLI_file_read_text_as_mem(import_params.filepath, 0, &buffer_len);
  if (buffer == nullptr) {
    BKE_reportf(import_params.reports,
                RPT_ERROR,
                "CSV Import: Cannot open file '%s'",
                import_params.filepath);
    return nullptr;
  }

  BLI_SCOPED_DEFER([&]() { MEM_freeN(buffer); });

  StringRef buffer_str{static_cast<char *>(buffer), int64_t(buffer_len)};
  if (buffer_str.is_empty()) {
    BKE_reportf(
        import_params.reports, RPT_ERROR, "CSV Import: empty file '%s'", import_params.filepath);
    return nullptr;
  }

  const StringRef header = read_next_line(buffer_str);
  const Vector<StringRef> names = parse_column_names(header);

  if (buffer_str.is_empty()) {
    BKE_reportf(import_params.reports,
                RPT_ERROR,
                "CSV Import: no rows in file '%s'",
                import_params.filepath);
    return nullptr;
  }

  /* Shallow copy buffer to preserve pointers from first row for parsing */
  const StringRef data_buffer(buffer_str.begin(), buffer_str.end());

  const StringRef first_row = read_next_line(buffer_str);

  Vector<eCustomDataType> column_types;
  if (!get_column_types(first_row, column_types)) {
    std::string column_name = names[column_types.size()];
    BKE_reportf(import_params.reports,
                RPT_ERROR,
                "CSV Import: file '%s', Column %s is of unsupported data type",
                import_params.filepath,
                column_name.c_str());
    return nullptr;
  }

  const int64_t rows_num = get_row_count(buffer_str);

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(rows_num);
  pointcloud->positions_for_write().fill(float3(0));

  Array<bke::GSpanAttributeWriter> attribute_writers(names.size());
  Array<GMutableSpan> attribute_data(names.size());

  bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
  for (const int i : names.index_range()) {
    attribute_writers[i] = attributes.lookup_or_add_for_write_span(
        names[i], bke::AttrDomain::Point, column_types[i]);
    attribute_data[i] = attribute_writers[i].span;
  }

  parse_csv_data(attribute_data, column_types, names, data_buffer, import_params);

  for (bke::GSpanAttributeWriter &attr : attribute_writers) {
    attr.finish();
  }

  return pointcloud;
}

}  // namespace blender::io::csv
