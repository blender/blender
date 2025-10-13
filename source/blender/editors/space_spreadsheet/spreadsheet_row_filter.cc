/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.hh"

#include "DNA_space_types.h"

#include "BKE_instances.hh"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_layout.hh"
#include "spreadsheet_row_filter.hh"

namespace blender::ed::spreadsheet {

template<typename T, typename OperationFn>
static IndexMask apply_filter_operation(const VArray<T> &data,
                                        OperationFn check_fn,
                                        const IndexMask &mask,
                                        IndexMaskMemory &memory)
{
  return IndexMask::from_predicate(
      mask, GrainSize(1024), memory, [&](const int64_t i) { return check_fn(data[i]); });
}

static IndexMask apply_row_filter(const SpreadsheetRowFilter &row_filter,
                                  const Map<StringRef, const ColumnValues *> &columns,
                                  const IndexMask &prev_mask,
                                  IndexMaskMemory &memory)
{
  const ColumnValues &column = *columns.lookup(row_filter.column_name);
  const GVArray &column_data = column.data();
  if (column_data.type().is<float>()) {
    const float value = row_filter.value_float;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold = row_filter.threshold;
        return apply_filter_operation(
            column_data.typed<float>(),
            [&](const float cell) { return std::abs(cell - value) < threshold; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<float>(),
            [&](const float cell) { return cell > value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<float>(),
            [&](const float cell) { return cell < value; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<bool>()) {
    const bool value = (row_filter.flag & SPREADSHEET_ROW_FILTER_BOOL_VALUE) != 0;
    return apply_filter_operation(
        column_data.typed<bool>(),
        [&](const bool cell) { return cell == value; },
        prev_mask,
        memory);
  }
  else if (column_data.type().is<int8_t>()) {
    const int value = row_filter.value_int;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        return apply_filter_operation(
            column_data.typed<int8_t>(),
            [&](const int cell) { return cell == value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<int8_t>(),
            [value](const int cell) { return cell > value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<int8_t>(),
            [&](const int cell) { return cell < value; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<int>()) {
    const int value = row_filter.value_int;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        return apply_filter_operation(
            column_data.typed<int>(),
            [&](const int cell) { return cell == value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<int>(),
            [value](const int cell) { return cell > value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<int>(),
            [&](const int cell) { return cell < value; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<int64_t>()) {
    const int64_t value = row_filter.value_int;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        return apply_filter_operation(
            column_data.typed<int64_t>(),
            [&](const int64_t cell) { return cell == value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<int64_t>(),
            [value](const int64_t cell) { return cell > value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<int64_t>(),
            [&](const int64_t cell) { return cell < value; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<int2>()) {
    const int2 value = row_filter.value_int2;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        return apply_filter_operation(
            column_data.typed<int2>(),
            [&](const int2 cell) { return cell == value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<int2>(),
            [&](const int2 cell) { return cell.x > value.x && cell.y > value.y; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<int2>(),
            [&](const int2 cell) { return cell.x < value.x && cell.y < value.y; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<int3>()) {
    const int3 value = row_filter.value_int3;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        return apply_filter_operation(
            column_data.typed<int3>(),
            [&](const int3 cell) { return cell == value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<int3>(),
            [&](const int3 cell) {
              return cell.x > value.x && cell.y > value.y && cell.z > value.z;
            },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<int3>(),
            [&](const int3 cell) {
              return cell.x < value.x && cell.y < value.y && cell.z < value.z;
            },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<short2>()) {
    const short2 value = short2(int2(row_filter.value_int2));
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        return apply_filter_operation(
            column_data.typed<short2>(),
            [&](const short2 cell) { return cell == value; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<short2>(),
            [&](const short2 cell) { return cell.x > value.x && cell.y > value.y; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<short2>(),
            [&](const short2 cell) { return cell.x < value.x && cell.y < value.y; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<float2>()) {
    const float2 value = row_filter.value_float2;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold_sq = pow2f(row_filter.threshold);
        return apply_filter_operation(
            column_data.typed<float2>(),
            [&](const float2 cell) { return math::distance_squared(cell, value) <= threshold_sq; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<float2>(),
            [&](const float2 cell) { return cell.x > value.x && cell.y > value.y; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<float2>(),
            [&](const float2 cell) { return cell.x < value.x && cell.y < value.y; },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<float3>()) {
    const float3 value = row_filter.value_float3;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold_sq = pow2f(row_filter.threshold);
        return apply_filter_operation(
            column_data.typed<float3>(),
            [&](const float3 cell) { return math::distance_squared(cell, value) <= threshold_sq; },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<float3>(),
            [&](const float3 cell) {
              return cell.x > value.x && cell.y > value.y && cell.z > value.z;
            },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<float3>(),
            [&](const float3 cell) {
              return cell.x < value.x && cell.y < value.y && cell.z < value.z;
            },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<ColorGeometry4f>()) {
    const ColorGeometry4f value = row_filter.value_color;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold_sq = pow2f(row_filter.threshold);
        return apply_filter_operation(
            column_data.typed<ColorGeometry4f>(),
            [&](const ColorGeometry4f cell) {
              return math::distance_squared(float4(cell), float4(value)) <= threshold_sq;
            },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<ColorGeometry4f>(),
            [&](const ColorGeometry4f cell) {
              return cell.r > value.r && cell.g > value.g && cell.b > value.b && cell.a > value.a;
            },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<ColorGeometry4f>(),
            [&](const ColorGeometry4f cell) {
              return cell.r < value.r && cell.g < value.g && cell.b < value.b && cell.a < value.a;
            },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<ColorGeometry4b>()) {
    const ColorGeometry4f value = row_filter.value_color;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float4 value_floats = {
            float(value.r), float(value.g), float(value.b), float(value.a)};
        const float threshold_sq = pow2f(row_filter.threshold);
        return apply_filter_operation(
            column_data.typed<ColorGeometry4b>(),
            [&](const ColorGeometry4b cell_bytes) {
              const ColorGeometry4f cell = color::decode(cell_bytes);
              const float4 cell_floats = {
                  float(cell.r), float(cell.g), float(cell.b), float(cell.a)};
              return math::distance_squared(value_floats, cell_floats) <= threshold_sq;
            },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        return apply_filter_operation(
            column_data.typed<ColorGeometry4b>(),
            [&](const ColorGeometry4b cell_bytes) {
              const ColorGeometry4f cell = color::decode(cell_bytes);
              return cell.r > value.r && cell.g > value.g && cell.b > value.b && cell.a > value.a;
            },
            prev_mask,
            memory);
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        return apply_filter_operation(
            column_data.typed<ColorGeometry4b>(),
            [&](const ColorGeometry4b cell_bytes) {
              const ColorGeometry4f cell = color::decode(cell_bytes);
              return cell.r < value.r && cell.g < value.g && cell.b < value.b && cell.a < value.a;
            },
            prev_mask,
            memory);
      }
    }
  }
  else if (column_data.type().is<bke::InstanceReference>()) {
    const StringRef value = row_filter.value_string;
    return apply_filter_operation(
        column_data.typed<bke::InstanceReference>(),
        [&](const bke::InstanceReference cell) {
          switch (cell.type()) {
            case bke::InstanceReference::Type::Object: {
              return value == (reinterpret_cast<ID &>(cell.object()).name + 2);
            }
            case bke::InstanceReference::Type::Collection: {
              return value == (reinterpret_cast<ID &>(cell.collection()).name + 2);
            }
            case bke::InstanceReference::Type::GeometrySet: {
              return value == cell.geometry_set().name;
            }
            case bke::InstanceReference::Type::None: {
              return false;
            }
          }
          BLI_assert_unreachable();
          return false;
        },
        prev_mask,
        memory);
  }
  return prev_mask;
}

static bool use_row_filters(const SpaceSpreadsheet &sspreadsheet)
{
  if (!(sspreadsheet.filter_flag & SPREADSHEET_FILTER_ENABLE)) {
    return false;
  }
  if (BLI_listbase_is_empty(&sspreadsheet.row_filters)) {
    return false;
  }
  return true;
}

static bool use_selection_filter(const SpaceSpreadsheet &sspreadsheet,
                                 const DataSource &data_source)
{
  if (!(sspreadsheet.filter_flag & SPREADSHEET_FILTER_SELECTED_ONLY)) {
    return false;
  }
  if (!data_source.has_selection_filter()) {
    return false;
  }
  return true;
}

IndexMask spreadsheet_filter_rows(const SpaceSpreadsheet &sspreadsheet,
                                  const SpreadsheetLayout &spreadsheet_layout,
                                  const DataSource &data_source,
                                  ResourceScope &scope)
{
  const int tot_rows = data_source.tot_rows();

  const bool use_selection = use_selection_filter(sspreadsheet, data_source);
  const bool use_filters = use_row_filters(sspreadsheet);

  /* Avoid allocating an array if no row filtering is necessary. */
  if (!(use_filters || use_selection)) {
    return IndexMask(tot_rows);
  }

  IndexMaskMemory &mask_memory = scope.construct<IndexMaskMemory>();
  IndexMask mask(tot_rows);

  if (use_selection) {
    const GeometryDataSource *geometry_data_source = dynamic_cast<const GeometryDataSource *>(
        &data_source);
    mask = geometry_data_source->apply_selection_filter(mask_memory);
  }

  if (use_filters) {
    Map<StringRef, const ColumnValues *> columns;
    for (const ColumnLayout &column : spreadsheet_layout.columns) {
      columns.add(column.values->name(), column.values);
    }

    LISTBASE_FOREACH (const SpreadsheetRowFilter *, row_filter, &sspreadsheet.row_filters) {
      if (row_filter->flag & SPREADSHEET_ROW_FILTER_ENABLED) {
        if (!columns.contains(row_filter->column_name)) {
          continue;
        }
        mask = apply_row_filter(*row_filter, columns, mask, mask_memory);
      }
    }
  }

  return mask;
}

SpreadsheetRowFilter *spreadsheet_row_filter_new()
{
  SpreadsheetRowFilter *row_filter = MEM_callocN<SpreadsheetRowFilter>(__func__);
  row_filter->flag = (SPREADSHEET_ROW_FILTER_UI_EXPAND | SPREADSHEET_ROW_FILTER_ENABLED);
  row_filter->operation = SPREADSHEET_ROW_FILTER_LESS;
  row_filter->threshold = 0.01f;
  row_filter->column_name[0] = '\0';

  return row_filter;
}

SpreadsheetRowFilter *spreadsheet_row_filter_copy(const SpreadsheetRowFilter *src_row_filter)
{
  SpreadsheetRowFilter *new_filter = spreadsheet_row_filter_new();

  memcpy(new_filter, src_row_filter, sizeof(SpreadsheetRowFilter));
  new_filter->next = nullptr;
  new_filter->prev = nullptr;

  return new_filter;
}

void spreadsheet_row_filter_free(SpreadsheetRowFilter *row_filter)
{
  MEM_SAFE_FREE(row_filter->value_string);
  MEM_freeN(row_filter);
}

}  // namespace blender::ed::spreadsheet
