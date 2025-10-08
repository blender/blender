/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"

#include "BLO_read_write.hh"

#include "MEM_guardedalloc.h"

#include "BLI_color.hh"
#include "BLI_cpp_type.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

#include "NOD_geometry_nodes_bundle.hh"

#include "spreadsheet_column.hh"
#include "spreadsheet_column_values.hh"

namespace blender::ed::spreadsheet {

eSpreadsheetColumnValueType cpp_type_to_column_type(const CPPType &type)
{
  if (type.is<bool>()) {
    return SPREADSHEET_VALUE_TYPE_BOOL;
  }
  if (type.is<int8_t>()) {
    return SPREADSHEET_VALUE_TYPE_INT8;
  }
  if (type.is<int>()) {
    return SPREADSHEET_VALUE_TYPE_INT32;
  }
  if (type.is<int64_t>()) {
    return SPREADSHEET_VALUE_TYPE_INT64;
  }
  if (type.is_any<short2, int2>()) {
    return SPREADSHEET_VALUE_TYPE_INT32_2D;
  }
  if (type.is_any<int3>()) {
    return SPREADSHEET_VALUE_TYPE_INT32_3D;
  }
  if (type.is<float>()) {
    return SPREADSHEET_VALUE_TYPE_FLOAT;
  }
  if (type.is<float2>()) {
    return SPREADSHEET_VALUE_TYPE_FLOAT2;
  }
  if (type.is<float3>()) {
    return SPREADSHEET_VALUE_TYPE_FLOAT3;
  }
  if (type.is<ColorGeometry4f>()) {
    return SPREADSHEET_VALUE_TYPE_COLOR;
  }
  if (type.is<std::string>() || type.is<MStringProperty>()) {
    return SPREADSHEET_VALUE_TYPE_STRING;
  }
  if (type.is<bke::InstanceReference>()) {
    return SPREADSHEET_VALUE_TYPE_INSTANCES;
  }
  if (type.is<ColorGeometry4b>()) {
    return SPREADSHEET_VALUE_TYPE_BYTE_COLOR;
  }
  if (type.is<math::Quaternion>()) {
    return SPREADSHEET_VALUE_TYPE_QUATERNION;
  }
  if (type.is<float4x4>()) {
    return SPREADSHEET_VALUE_TYPE_FLOAT4X4;
  }
  if (type.is<nodes::BundleItemValue>()) {
    return SPREADSHEET_VALUE_TYPE_BUNDLE_ITEM;
  }

  return SPREADSHEET_VALUE_TYPE_UNKNOWN;
}

SpreadsheetColumnID *spreadsheet_column_id_new()
{
  SpreadsheetColumnID *column_id = MEM_callocN<SpreadsheetColumnID>(__func__);
  return column_id;
}

SpreadsheetColumnID *spreadsheet_column_id_copy(const SpreadsheetColumnID *src_column_id)
{
  SpreadsheetColumnID *new_column_id = spreadsheet_column_id_new();
  new_column_id->name = BLI_strdup(src_column_id->name);
  return new_column_id;
}

void spreadsheet_column_id_free(SpreadsheetColumnID *column_id)
{
  if (column_id->name != nullptr) {
    MEM_freeN(column_id->name);
  }
  MEM_freeN(column_id);
}

void spreadsheet_column_id_blend_write(BlendWriter *writer, const SpreadsheetColumnID *column_id)
{
  BLO_write_struct(writer, SpreadsheetColumnID, column_id);
  BLO_write_string(writer, column_id->name);
}

void spreadsheet_column_id_blend_read(BlendDataReader *reader, SpreadsheetColumnID *column_id)
{
  BLO_read_string(reader, &column_id->name);
}

SpreadsheetColumn *spreadsheet_column_new(SpreadsheetColumnID *column_id)
{
  SpreadsheetColumn *column = MEM_callocN<SpreadsheetColumn>(__func__);
  column->id = column_id;
  column->runtime = MEM_new<SpreadsheetColumnRuntime>(__func__);
  return column;
}

void spreadsheet_column_assign_runtime_data(SpreadsheetColumn *column,
                                            const eSpreadsheetColumnValueType data_type,
                                            const StringRefNull display_name)
{
  column->data_type = data_type;
  MEM_SAFE_FREE(column->display_name);
  column->display_name = BLI_strdup(display_name.c_str());
}

SpreadsheetColumn *spreadsheet_column_copy(const SpreadsheetColumn *src_column)
{
  SpreadsheetColumnID *new_column_id = spreadsheet_column_id_copy(src_column->id);
  SpreadsheetColumn *new_column = spreadsheet_column_new(new_column_id);
  if (src_column->display_name != nullptr) {
    new_column->display_name = BLI_strdup(src_column->display_name);
  }
  new_column->width = src_column->width;
  return new_column;
}

void spreadsheet_column_free(SpreadsheetColumn *column)
{
  spreadsheet_column_id_free(column->id);
  MEM_SAFE_FREE(column->display_name);
  MEM_delete(column->runtime);
  MEM_freeN(column);
}

void spreadsheet_column_blend_write(BlendWriter *writer, const SpreadsheetColumn *column)
{
  BLO_write_struct(writer, SpreadsheetColumn, column);
  spreadsheet_column_id_blend_write(writer, column->id);
  BLO_write_string(writer, column->display_name);
}

void spreadsheet_column_blend_read(BlendDataReader *reader, SpreadsheetColumn *column)
{
  column->runtime = MEM_new<SpreadsheetColumnRuntime>(__func__);
  BLO_read_struct(reader, SpreadsheetColumnID, &column->id);
  spreadsheet_column_id_blend_read(reader, column->id);
  BLO_read_string(reader, &column->display_name);
}

}  // namespace blender::ed::spreadsheet
