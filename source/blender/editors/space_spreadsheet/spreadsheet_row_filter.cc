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

#include "DNA_collection_types.h"
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

template<typename OperationFn>
static void apply_filter_operation(const ColumnValues &values,
                                   OperationFn check_fn,
                                   MutableSpan<bool> rows_included)
{
  for (const int i : rows_included.index_range()) {
    if (!rows_included[i]) {
      continue;
    }
    CellValue cell_value;
    values.get_value(i, cell_value);
    if (!check_fn(cell_value)) {
      rows_included[i] = false;
    }
  }
}

static void apply_row_filter(const SpreadsheetLayout &spreadsheet_layout,
                             const SpreadsheetRowFilter &row_filter,
                             MutableSpan<bool> rows_included)
{
  for (const ColumnLayout &column : spreadsheet_layout.columns) {
    const ColumnValues &values = *column.values;
    if (values.name() != row_filter.column_name) {
      continue;
    }

    switch (values.type()) {
      case SPREADSHEET_VALUE_TYPE_INT32: {
        const int value = row_filter.value_int;
        switch (row_filter.operation) {
          case SPREADSHEET_ROW_FILTER_EQUAL: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return *cell_value.value_int == value;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_GREATER: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return *cell_value.value_int > value;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_LESS: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return *cell_value.value_int < value;
                },
                rows_included);
            break;
          }
        }
        break;
      }
      case SPREADSHEET_VALUE_TYPE_FLOAT: {
        const float value = row_filter.value_float;
        switch (row_filter.operation) {
          case SPREADSHEET_ROW_FILTER_EQUAL: {
            const float threshold = row_filter.threshold;
            apply_filter_operation(
                values,
                [value, threshold](const CellValue &cell_value) -> bool {
                  return std::abs(*cell_value.value_float - value) < threshold;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_GREATER: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return *cell_value.value_float > value;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_LESS: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return *cell_value.value_float < value;
                },
                rows_included);
            break;
          }
        }
        break;
      }
      case SPREADSHEET_VALUE_TYPE_FLOAT2: {
        const float2 value = row_filter.value_float2;
        switch (row_filter.operation) {
          case SPREADSHEET_ROW_FILTER_EQUAL: {
            const float threshold_squared = row_filter.threshold * row_filter.threshold;
            apply_filter_operation(
                values,
                [value, threshold_squared](const CellValue &cell_value) -> bool {
                  return float2::distance_squared(*cell_value.value_float2, value) <
                         threshold_squared;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_GREATER: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return cell_value.value_float2->x > value.x &&
                         cell_value.value_float2->y > value.y;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_LESS: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return cell_value.value_float2->x < value.x &&
                         cell_value.value_float2->y < value.y;
                },
                rows_included);
            break;
          }
        }
        break;
      }
      case SPREADSHEET_VALUE_TYPE_FLOAT3: {
        const float3 value = row_filter.value_float3;
        switch (row_filter.operation) {
          case SPREADSHEET_ROW_FILTER_EQUAL: {
            const float threshold_squared = row_filter.threshold * row_filter.threshold;
            apply_filter_operation(
                values,
                [value, threshold_squared](const CellValue &cell_value) -> bool {
                  return float3::distance_squared(*cell_value.value_float3, value) <
                         threshold_squared;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_GREATER: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return cell_value.value_float3->x > value.x &&
                         cell_value.value_float3->y > value.y &&
                         cell_value.value_float3->z > value.z;
                },
                rows_included);
            break;
          }
          case SPREADSHEET_ROW_FILTER_LESS: {
            apply_filter_operation(
                values,
                [value](const CellValue &cell_value) -> bool {
                  return cell_value.value_float3->x < value.x &&
                         cell_value.value_float3->y < value.y &&
                         cell_value.value_float3->z < value.z;
                },
                rows_included);
            break;
          }
        }
        break;
      }
      case SPREADSHEET_VALUE_TYPE_COLOR: {
        const ColorGeometry4f value = row_filter.value_color;
        switch (row_filter.operation) {
          case SPREADSHEET_ROW_FILTER_EQUAL: {
            const float threshold_squared = row_filter.threshold * row_filter.threshold;
            apply_filter_operation(
                values,
                [value, threshold_squared](const CellValue &cell_value) -> bool {
                  return len_squared_v4v4(value, *cell_value.value_color) < threshold_squared;
                },
                rows_included);
            break;
          }
        }
        break;
      }
      case SPREADSHEET_VALUE_TYPE_BOOL: {
        const bool value = (row_filter.flag & SPREADSHEET_ROW_FILTER_BOOL_VALUE) != 0;
        apply_filter_operation(
            values,
            [value](const CellValue &cell_value) -> bool {
              return *cell_value.value_bool == value;
            },
            rows_included);
        break;
      }
      case SPREADSHEET_VALUE_TYPE_INSTANCES: {
        const StringRef value = row_filter.value_string;
        apply_filter_operation(
            values,
            [value](const CellValue &cell_value) -> bool {
              const ID *id = nullptr;
              if (cell_value.value_object) {
                id = &cell_value.value_object->object->id;
              }
              else if (cell_value.value_collection) {
                id = &cell_value.value_collection->collection->id;
              }
              if (id == nullptr) {
                return false;
              }

              return value == id->name + 2;
            },
            rows_included);
        break;
      }
      default:
        break;
    }

    /* Only one column should have this name. */
    break;
  }
}

static void index_vector_from_bools(Span<bool> selection, Vector<int64_t> &indices)
{
  for (const int i : selection.index_range()) {
    if (selection[i]) {
      indices.append(i);
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

Span<int64_t> spreadsheet_filter_rows(const SpaceSpreadsheet &sspreadsheet,
                                      const SpreadsheetLayout &spreadsheet_layout,
                                      const DataSource &data_source,
                                      ResourceScope &scope)
{
  const int tot_rows = data_source.tot_rows();

  const bool use_selection = use_selection_filter(sspreadsheet, data_source);
  const bool use_filters = use_row_filters(sspreadsheet);

  /* Avoid allocating an array if no row filtering is necessary. */
  if (!(use_filters || use_selection)) {
    return IndexRange(tot_rows).as_span();
  }

  Array<bool> rows_included(tot_rows, true);

  if (use_filters) {
    LISTBASE_FOREACH (const SpreadsheetRowFilter *, row_filter, &sspreadsheet.row_filters) {
      if (row_filter->flag & SPREADSHEET_ROW_FILTER_ENABLED) {
        apply_row_filter(spreadsheet_layout, *row_filter, rows_included);
      }
    }
  }

  if (use_selection) {
    const GeometryDataSource *geometry_data_source = dynamic_cast<const GeometryDataSource *>(
        &data_source);
    geometry_data_source->apply_selection_filter(rows_included);
  }

  Vector<int64_t> &indices = scope.construct<Vector<int64_t>>(__func__);
  index_vector_from_bools(rows_included, indices);

  return indices;
}

SpreadsheetRowFilter *spreadsheet_row_filter_new()
{
  SpreadsheetRowFilter *row_filter = (SpreadsheetRowFilter *)MEM_callocN(
      sizeof(SpreadsheetRowFilter), __func__);
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
