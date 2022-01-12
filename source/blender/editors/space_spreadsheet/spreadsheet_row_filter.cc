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

#include <cstring>

#include "BLI_listbase.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "spreadsheet_intern.hh"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_intern.hh"
#include "spreadsheet_layout.hh"
#include "spreadsheet_row_filter.hh"

namespace blender::ed::spreadsheet {

template<typename T, typename OperationFn>
static void apply_filter_operation(const VArray<T> &data,
                                   OperationFn check_fn,
                                   const IndexMask mask,
                                   Vector<int64_t> &new_indices)
{
  for (const int64_t i : mask) {
    if (check_fn(data[i])) {
      new_indices.append(i);
    }
  }
}

static void apply_row_filter(const SpreadsheetRowFilter &row_filter,
                             const Map<StringRef, const ColumnValues *> &columns,
                             const IndexMask prev_mask,
                             Vector<int64_t> &new_indices)
{
  const ColumnValues &column = *columns.lookup(row_filter.column_name);
  const fn::GVArray &column_data = column.data();
  if (column_data.type().is<float>()) {
    const float value = row_filter.value_float;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold = row_filter.threshold;
        apply_filter_operation(
            column_data.typed<float>(),
            [&](const float cell) { return std::abs(cell - value) < threshold; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        apply_filter_operation(
            column_data.typed<float>(),
            [&](const float cell) { return cell > value; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        apply_filter_operation(
            column_data.typed<float>(),
            [&](const float cell) { return cell < value; },
            prev_mask,
            new_indices);
        break;
      }
    }
  }
  else if (column_data.type().is<int>()) {
    const int value = row_filter.value_int;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        apply_filter_operation(
            column_data.typed<int>(),
            [&](const int cell) { return cell == value; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        apply_filter_operation(
            column_data.typed<int>(),
            [value](const int cell) { return cell > value; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        apply_filter_operation(
            column_data.typed<int>(),
            [&](const int cell) { return cell < value; },
            prev_mask,
            new_indices);
        break;
      }
    }
  }
  else if (column_data.type().is<float2>()) {
    const float2 value = row_filter.value_float2;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold_sq = row_filter.threshold;
        apply_filter_operation(
            column_data.typed<float2>(),
            [&](const float2 cell) { return math::distance_squared(cell, value) > threshold_sq; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        apply_filter_operation(
            column_data.typed<float2>(),
            [&](const float2 cell) { return cell.x > value.x && cell.y > value.y; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        apply_filter_operation(
            column_data.typed<float2>(),
            [&](const float2 cell) { return cell.x < value.x && cell.y < value.y; },
            prev_mask,
            new_indices);
        break;
      }
    }
  }
  else if (column_data.type().is<float3>()) {
    const float3 value = row_filter.value_float3;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold_sq = row_filter.threshold;
        apply_filter_operation(
            column_data.typed<float3>(),
            [&](const float3 cell) { return math::distance_squared(cell, value) > threshold_sq; },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_GREATER: {
        apply_filter_operation(
            column_data.typed<float3>(),
            [&](const float3 cell) {
              return cell.x > value.x && cell.y > value.y && cell.z > value.z;
            },
            prev_mask,
            new_indices);
        break;
      }
      case SPREADSHEET_ROW_FILTER_LESS: {
        apply_filter_operation(
            column_data.typed<float3>(),
            [&](const float3 cell) {
              return cell.x < value.x && cell.y < value.y && cell.z < value.z;
            },
            prev_mask,
            new_indices);
        break;
      }
    }
  }
  else if (column_data.type().is<ColorGeometry4f>()) {
    const ColorGeometry4f value = row_filter.value_color;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        const float threshold_sq = row_filter.threshold;
        apply_filter_operation(
            column_data.typed<ColorGeometry4f>(),
            [&](const ColorGeometry4f cell) {
              return len_squared_v4v4(cell, value) > threshold_sq;
            },
            prev_mask,
            new_indices);
        break;
      }
    }
  }
  else if (column_data.type().is<InstanceReference>()) {
    const StringRef value = row_filter.value_string;
    switch (row_filter.operation) {
      case SPREADSHEET_ROW_FILTER_EQUAL: {
        apply_filter_operation(
            column_data.typed<InstanceReference>(),
            [&](const InstanceReference cell) {
              switch (cell.type()) {
                case InstanceReference::Type::Object: {
                  return value == (reinterpret_cast<ID &>(cell.object()).name + 2);
                }
                case InstanceReference::Type::Collection: {
                  return value == (reinterpret_cast<ID &>(cell.collection()).name + 2);
                }
                case InstanceReference::Type::GeometrySet: {
                  return false;
                }
                case InstanceReference::Type::None: {
                  return false;
                }
              }
              BLI_assert_unreachable();
              return false;
            },
            prev_mask,
            new_indices);
        break;
      }
    }
  }
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

  IndexMask mask(tot_rows);

  Vector<int64_t> mask_indices;
  mask_indices.reserve(tot_rows);

  if (use_selection) {
    const GeometryDataSource *geometry_data_source = dynamic_cast<const GeometryDataSource *>(
        &data_source);
    mask = geometry_data_source->apply_selection_filter(mask_indices);
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
        Vector<int64_t> new_indices;
        new_indices.reserve(mask_indices.size());
        apply_row_filter(*row_filter, columns, mask, new_indices);
        std::swap(new_indices, mask_indices);
        mask = IndexMask(mask_indices);
      }
    }
  }

  if (mask_indices.is_empty()) {
    BLI_assert(mask.is_empty() || mask.is_range());
    return mask;
  }

  return IndexMask(scope.add_value(std::move(mask_indices)));
}

SpreadsheetRowFilter *spreadsheet_row_filter_new()
{
  SpreadsheetRowFilter *row_filter = MEM_cnew<SpreadsheetRowFilter>(__func__);
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
