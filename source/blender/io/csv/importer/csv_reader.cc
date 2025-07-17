/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup csv
 */

#include <atomic>
#include <charconv>
#include <optional>
#include <variant>

#include "BLI_array_utils.hh"
#include "fast_float.h"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"

#include "BLI_csv_parse.hh"
#include "BLI_fileops.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_vector.hh"

#include "IO_csv.hh"

namespace blender::io::csv {

struct ColumnInfo {
  StringRef name;
  bool has_invalid_name = false;
  std::atomic<bool> found_invalid = false;
  std::atomic<bool> found_int = false;
  std::atomic<bool> found_float = false;
};

using ColumnData = std::variant<std::monostate, Vector<float>, Vector<int>>;

struct ChunkResult {
  int rows_num;
  Vector<ColumnData> columns;
};

struct ParseFloatColumnResult {
  Vector<float> data;
  bool found_invalid = false;
};

struct ParseIntColumnResult {
  Vector<int> data;
  bool found_invalid = false;
  bool found_float = false;
};

static ParseFloatColumnResult parse_column_as_floats(const csv_parse::CsvRecords &records,
                                                     const int column_i)
{
  ParseFloatColumnResult result;
  result.data.reserve(records.size());
  for (const int row_i : records.index_range()) {
    const Span<char> value_span = records.record(row_i).field(column_i);
    const char *value_begin = value_span.begin();
    const char *value_end = value_span.end();
    /* Skip leading white-space and plus sign. */
    while (value_begin < value_end && ELEM(*value_begin, ' ', '+')) {
      value_begin++;
    }
    float value;
    fast_float::from_chars_result res = fast_float::from_chars(value_begin, value_end, value);
    if (res.ec != std::errc()) {
      result.found_invalid = true;
      return result;
    }
    if (res.ptr < value_end) {
      /* Allow trailing white-space in the value. */
      while (res.ptr < value_end && res.ptr[0] == ' ') {
        res.ptr++;
      }
      if (res.ptr < value_end) {
        result.found_invalid = true;
        return result;
      }
    }
    result.data.append(value);
  }
  return result;
}

static ParseIntColumnResult parse_column_as_ints(const csv_parse::CsvRecords &records,
                                                 const int column_i)
{
  ParseIntColumnResult result;
  result.data.reserve(records.size());
  for (const int row_i : records.index_range()) {
    const Span<char> value_span = records.record(row_i).field(column_i);
    const char *value_begin = value_span.begin();
    const char *value_end = value_span.end();
    /* Skip leading white-space and plus sign. */
    while (value_begin < value_end && ELEM(*value_begin, ' ', '+')) {
      value_begin++;
    }
    int value;
    std::from_chars_result res = std::from_chars(value_begin, value_end, value);
    if (res.ec != std::errc()) {
      result.found_invalid = true;
      return result;
    }
    if (res.ptr < value_end) {
      /* If the next character after the value is a dot, it should be parsed again as float. */
      if (res.ptr[0] == '.') {
        result.found_float = true;
        return result;
      }
      /* Allow trailing white-space in the value. */
      while (res.ptr < value_end && res.ptr[0] == ' ') {
        res.ptr++;
      }
      if (res.ptr < value_end) {
        result.found_invalid = true;
        return result;
      }
    }
    result.data.append(value);
  }
  return result;
}

static ChunkResult parse_records_chunk(const csv_parse::CsvRecords &records,
                                       MutableSpan<ColumnInfo> columns_info)
{
  const int columns_num = columns_info.size();
  ChunkResult chunk_result;
  chunk_result.rows_num = records.size();
  chunk_result.columns.resize(columns_num);
  for (const int column_i : IndexRange(columns_num)) {
    ColumnInfo &column_info = columns_info[column_i];
    if (column_info.has_invalid_name) {
      /* Column can be ignored. */
      continue;
    }
    if (column_info.found_invalid.load(std::memory_order_relaxed)) {
      /* Invalid values have been found in this column already, skip it. */
      continue;
    }
    /* A float was found in this column already, so parse everything as floats. */
    const bool found_float = column_info.found_float.load(std::memory_order_relaxed);
    if (found_float) {
      ParseFloatColumnResult float_column_result = parse_column_as_floats(records, column_i);
      if (float_column_result.found_invalid) {
        column_info.found_invalid.store(true, std::memory_order_relaxed);
        continue;
      }
      chunk_result.columns[column_i] = std::move(float_column_result.data);
      continue;
    }
    /* No float was found so far in this column, so attempt to parse it as integers. */
    ParseIntColumnResult int_column_result = parse_column_as_ints(records, column_i);
    if (int_column_result.found_invalid) {
      column_info.found_invalid.store(true, std::memory_order_relaxed);
      continue;
    }
    if (!int_column_result.found_float) {
      chunk_result.columns[column_i] = std::move(int_column_result.data);
      column_info.found_int.store(true, std::memory_order_relaxed);
      continue;
    }
    /* While parsing it as integers, floats were detected. So parse it as floats again. */
    column_info.found_float.store(true, std::memory_order_relaxed);
    ParseFloatColumnResult float_column_result = parse_column_as_floats(records, column_i);
    if (float_column_result.found_invalid) {
      column_info.found_invalid.store(true, std::memory_order_relaxed);
      continue;
    }
    chunk_result.columns[column_i] = std::move(float_column_result.data);
  }
  return chunk_result;
}

/**
 * So far, the parsed data is still split into many chunks. This function flattens the chunks into
 * continuous buffers that can be used as attributes.
 */
static Array<std::optional<GArray<>>> flatten_valid_attribute_chunks(
    const Span<ColumnInfo> columns_info,
    OffsetIndices<int> chunk_offsets,
    MutableSpan<ChunkResult> chunks)
{
  const int points_num = chunk_offsets.total_size();
  Array<std::optional<GArray<>>> flattened_attributes(columns_info.size());

  threading::parallel_for(columns_info.index_range(), 1, [&](const IndexRange columns_range) {
    for (const int column_i : columns_range) {
      const ColumnInfo &column_info = columns_info[column_i];
      if (column_info.has_invalid_name || column_info.found_invalid) {
        /* Column can be ignored. */
        continue;
      }
      if (column_info.found_float) {
        /* Should read column as floats. */
        GArray<> attribute(CPPType::get<float>(), points_num);
        float *attribute_buffer = static_cast<float *>(attribute.data());
        threading::parallel_for(chunks.index_range(), 1, [&](const IndexRange chunks_range) {
          for (const int chunk_i : chunks_range) {
            const IndexRange dst_range = chunk_offsets[chunk_i];
            ChunkResult &chunk = chunks[chunk_i];
            ColumnData &column_data = chunk.columns[column_i];
            if (const auto *float_vec = std::get_if<Vector<float>>(&column_data)) {
              BLI_assert(float_vec->size() == dst_range.size());
              uninitialized_copy_n(
                  float_vec->data(), dst_range.size(), attribute_buffer + dst_range.first());
            }
            else if (const auto *int_vec = std::get_if<Vector<int>>(&column_data)) {
              /* This chunk was read entirely as integers, so it still has to be converted to
               * floats. */
              BLI_assert(int_vec->size() == dst_range.size());
              uninitialized_convert_n(int_vec->data(), dst_range.size(), attribute_buffer);
            }
            else {
              /* Expected data to be available, because the `found_invalid` flag was not
               * set. */
              BLI_assert_unreachable();
            }
            /* Free data for chunk. */
            column_data = std::monostate{};
          }
        });
        flattened_attributes[column_i] = std::move(attribute);
        continue;
      }
      if (column_info.found_int) {
        /* Should read column as ints. */
        GArray<> attribute(CPPType::get<int>(), points_num);
        int *attribute_buffer = static_cast<int *>(attribute.data());
        threading::parallel_for(chunks.index_range(), 1, [&](const IndexRange chunks_range) {
          for (const int chunk_i : chunks_range) {
            const IndexRange dst_range = chunk_offsets[chunk_i];
            ChunkResult &chunk = chunks[chunk_i];
            ColumnData &column_data = chunk.columns[column_i];
            if (const auto *int_vec = std::get_if<Vector<int>>(&column_data)) {
              BLI_assert(int_vec->size() == dst_range.size());
              uninitialized_copy_n(
                  int_vec->data(), dst_range.size(), attribute_buffer + dst_range.first());
            }
            else {
              /* Expected data to be available, because the `found_invalid` and
               * `found_float` flags were not set. */
              BLI_assert_unreachable();
            }
            /* Free data for chunk. */
            column_data = std::monostate{};
          }
        });
        flattened_attributes[column_i] = std::move(attribute);
        continue;
      }
    }
  });
  return flattened_attributes;
}

PointCloud *import_csv_as_pointcloud(const CSVImportParams &import_params)
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
  if (buffer_len == 0) {
    BKE_reportf(
        import_params.reports, RPT_ERROR, "CSV Import: empty file '%s'", import_params.filepath);
    return nullptr;
  }

  LinearAllocator<> allocator;
  Array<ColumnInfo> columns_info;
  csv_parse::CsvParseOptions parse_options;
  parse_options.delimiter = import_params.delimiter;

  const auto parse_header = [&](const csv_parse::CsvRecord &record) {
    columns_info.reinitialize(record.size());
    for (const int i : record.index_range()) {
      ColumnInfo &column_info = columns_info[i];
      const StringRef name = csv_parse::unescape_field(
          record.field_str(i), parse_options, allocator);
      column_info.name = name;
      if (!bke::allow_procedural_attribute_access(name) ||
          bke::attribute_name_is_anonymous(name) || name.is_empty())
      {
        column_info.has_invalid_name = true;
        continue;
      }
    }
  };
  const auto parse_data_chunk = [&](const csv_parse::CsvRecords &records) {
    return parse_records_chunk(records, columns_info);
  };

  const Span<char> buffer_span{static_cast<char *>(buffer), int64_t(buffer_len)};
  std::optional<Vector<ChunkResult>> parsed_chunks = csv_parse::parse_csv_in_chunks<ChunkResult>(
      buffer_span, parse_options, parse_header, parse_data_chunk);

  if (!parsed_chunks.has_value()) {
    BKE_reportf(import_params.reports,
                RPT_ERROR,
                "CSV import: failed to parse file '%s'",
                import_params.filepath);
    return nullptr;
  }

  /* Count the total number of records and compute the offset of each chunk which is used when
   * flattening the parsed data. */
  Vector<int> chunk_offsets_vec;
  chunk_offsets_vec.append(0);
  for (const ChunkResult &chunk : *parsed_chunks) {
    chunk_offsets_vec.append(chunk_offsets_vec.last() + chunk.rows_num);
  }
  const OffsetIndices<int> chunk_offsets(chunk_offsets_vec);
  const int points_num = chunk_offsets_vec.last();

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(points_num);

  Array<std::optional<GArray<>>> flattened_attributes;
  threading::memory_bandwidth_bound_task(points_num * 16, [&]() {
    threading::parallel_invoke(
        [&]() {
          array_utils::copy(VArray<float3>::from_single(float3(0), points_num),
                            pointcloud->positions_for_write());
        },
        [&]() {
          flattened_attributes = flatten_valid_attribute_chunks(
              columns_info, chunk_offsets, *parsed_chunks);
        });
  });

  /* Add all valid attributes to the pointcloud. */
  bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
  for (const int column_i : columns_info.index_range()) {
    std::optional<GArray<>> &attribute = flattened_attributes[column_i];
    if (!attribute.has_value()) {
      continue;
    }
    const auto *data = new ImplicitSharedValue<GArray<>>(std::move(*attribute));
    const bke::AttrType type = bke::cpp_type_to_attribute_type(attribute->type());
    const ColumnInfo &column_info = columns_info[column_i];
    attributes.add(column_info.name,
                   bke::AttrDomain::Point,
                   type,
                   bke::AttributeInitShared{data->data.data(), *data});
    data->remove_user_and_delete_if_last();
  }

  /* Since all positions are set to zero, the bounding box can be updated eagerly to avoid
   * computing it later. */
  pointcloud->runtime->bounds_cache.ensure([](Bounds<float3> &r_bounds) {
    r_bounds.min = float3(0);
    r_bounds.max = float3(0);
  });

  return pointcloud;
}

}  // namespace blender::io::csv
