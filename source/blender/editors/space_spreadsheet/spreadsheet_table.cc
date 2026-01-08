/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_viewer_path.hh"

#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_array_utils.hh"

#include "BLO_read_write.hh"

#include "spreadsheet_column.hh"
#include "spreadsheet_table.hh"

namespace blender::ed::spreadsheet {

SpreadsheetTableIDGeometry *spreadsheet_table_id_new_geometry()
{
  auto *table_id = MEM_new<SpreadsheetTableIDGeometry>(__func__);
  table_id->base.type = SPREADSHEET_TABLE_ID_TYPE_GEOMETRY;
  return table_id;
}

static void copy_bundle_path(SpreadsheetBundleTreeViewPath &dst,
                             const SpreadsheetBundleTreeViewPath &src)
{
  dst.bundle_path = MEM_new_array<SpreadsheetBundlePathElem>(src.bundle_path_num, __func__);
  dst.bundle_path_num = src.bundle_path_num;
  for (const int i : IndexRange(src.bundle_path_num)) {
    dst.bundle_path[i].identifier = BLI_strdup_null(src.bundle_path[i].identifier);
  }
}

void spreadsheet_table_id_copy_content_geometry(SpreadsheetTableIDGeometry &dst,
                                                const SpreadsheetTableIDGeometry &src)
{
  BKE_viewer_path_copy(&dst.viewer_path, &src.viewer_path);
  dst.geometry_component_type = src.geometry_component_type;
  dst.attribute_domain = src.attribute_domain;
  dst.object_eval_state = src.object_eval_state;
  dst.layer_index = src.layer_index;
  dst.instance_ids = MEM_dupalloc(src.instance_ids);
  dst.instance_ids_num = src.instance_ids_num;
  copy_bundle_path(dst.viewer_item_bundle_path, src.viewer_item_bundle_path);
  copy_bundle_path(dst.geometry_bundle_path, src.geometry_bundle_path);
}

SpreadsheetTableID *spreadsheet_table_id_copy(const SpreadsheetTableID &src_table_id)
{
  switch (eSpreadsheetTableIDType(src_table_id.type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      const auto &src = *reinterpret_cast<const SpreadsheetTableIDGeometry *>(&src_table_id);
      auto *new_table_id = spreadsheet_table_id_new_geometry();
      spreadsheet_table_id_copy_content_geometry(*new_table_id, src);
      return &new_table_id->base;
    }
  }
  return nullptr;
}

void spreadsheet_bundle_path_clear(SpreadsheetBundleTreeViewPath &bundle_path)
{
  for (const int i : IndexRange(bundle_path.bundle_path_num)) {
    MEM_SAFE_DELETE(bundle_path.bundle_path[i].identifier);
  }
  MEM_SAFE_DELETE(bundle_path.bundle_path);
  bundle_path.bundle_path_num = 0;
  bundle_path.closure_input_output = SPREADSHEET_CLOSURE_NONE;
}

void spreadsheet_bundle_path_init_from(
    const Span<StringRef> keys,
    const std::optional<SpreadsheetClosureInputOutput> closure_input_output,
    SpreadsheetBundleTreeViewPath &r_bundle_path)
{
  spreadsheet_bundle_path_clear(r_bundle_path);
  r_bundle_path.bundle_path = MEM_new_array<SpreadsheetBundlePathElem>(keys.size(), __func__);
  r_bundle_path.bundle_path_num = keys.size();
  for (const int i : keys.index_range()) {
    const StringRef key = keys[i];
    r_bundle_path.bundle_path[i].identifier = BLI_strdupn(key.data(), key.size());
  }
  r_bundle_path.closure_input_output = closure_input_output.value_or(SPREADSHEET_CLOSURE_NONE);
}

static void free_bundle_path(SpreadsheetBundleTreeViewPath &bundle_path)
{
  spreadsheet_bundle_path_clear(bundle_path);
}

void spreadsheet_table_id_free_content(SpreadsheetTableID *table_id)
{
  switch (eSpreadsheetTableIDType(table_id->type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      auto *table_id_ = reinterpret_cast<SpreadsheetTableIDGeometry *>(table_id);
      BKE_viewer_path_clear(&table_id_->viewer_path);
      MEM_SAFE_DELETE(table_id_->instance_ids);
      free_bundle_path(table_id_->viewer_item_bundle_path);
      free_bundle_path(table_id_->geometry_bundle_path);
      break;
    }
  }
}

void spreadsheet_table_id_free(SpreadsheetTableID *table_id)
{
  spreadsheet_table_id_free_content(table_id);
  MEM_delete(table_id);
}

static void write_bundle_path(BlendWriter *writer,
                              const SpreadsheetBundleTreeViewPath &bundle_path)
{
  writer->write_struct_array(bundle_path.bundle_path_num, bundle_path.bundle_path);
  for (const int i : IndexRange(bundle_path.bundle_path_num)) {
    BLO_write_string(writer, bundle_path.bundle_path[i].identifier);
  }
}

void spreadsheet_table_id_blend_write_content_geometry(BlendWriter *writer,
                                                       const SpreadsheetTableIDGeometry *table_id)
{
  BKE_viewer_path_blend_write(writer, &table_id->viewer_path);
  writer->write_struct_array(table_id->instance_ids_num, table_id->instance_ids);
  write_bundle_path(writer, table_id->viewer_item_bundle_path);
  write_bundle_path(writer, table_id->geometry_bundle_path);
}

void spreadsheet_table_id_blend_write(BlendWriter *writer, const SpreadsheetTableID *table_id)
{
  switch (eSpreadsheetTableIDType(table_id->type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      const auto *table_id_ = reinterpret_cast<const SpreadsheetTableIDGeometry *>(table_id);
      writer->write_struct(table_id_);
      spreadsheet_table_id_blend_write_content_geometry(writer, table_id_);
      break;
    }
  }
}

static void read_bundle_path(BlendDataReader *reader, SpreadsheetBundleTreeViewPath &bundle_path)
{
  BLO_read_struct_array(
      reader, SpreadsheetBundlePathElem, bundle_path.bundle_path_num, &bundle_path.bundle_path);
  for (const int i : IndexRange(bundle_path.bundle_path_num)) {
    BLO_read_string(reader, &bundle_path.bundle_path[i].identifier);
  }
}

void spreadsheet_table_id_blend_read(BlendDataReader *reader, SpreadsheetTableID *table_id)
{
  switch (eSpreadsheetTableIDType(table_id->type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      auto *table_id_ = reinterpret_cast<SpreadsheetTableIDGeometry *>(table_id);
      BKE_viewer_path_blend_read_data(reader, &table_id_->viewer_path);
      BLO_read_struct_array(
          reader, SpreadsheetInstanceID, table_id_->instance_ids_num, &table_id_->instance_ids);
      read_bundle_path(reader, table_id_->viewer_item_bundle_path);
      read_bundle_path(reader, table_id_->geometry_bundle_path);
      break;
    }
  }
}

void spreadsheet_table_id_remap_id(SpreadsheetTableID &table_id,
                                   const bke::id::IDRemapper &mappings)
{
  switch (eSpreadsheetTableIDType(table_id.type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      auto *table_id_ = reinterpret_cast<SpreadsheetTableIDGeometry *>(&table_id);
      BKE_viewer_path_id_remap(&table_id_->viewer_path, mappings);
      break;
    }
  }
}

void spreadsheet_table_id_foreach_id(SpreadsheetTableID &table_id, LibraryForeachIDData *data)
{
  switch (eSpreadsheetTableIDType(table_id.type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      auto *table_id_ = reinterpret_cast<SpreadsheetTableIDGeometry *>(&table_id);
      BKE_viewer_path_foreach_id(data, &table_id_->viewer_path);
      break;
    }
  }
}

static bool bundle_path_match(const SpreadsheetBundleTreeViewPath &a,
                              const SpreadsheetBundleTreeViewPath &b)
{
  return Span<const SpreadsheetBundlePathElem>(a.bundle_path, a.bundle_path_num) ==
         Span<const SpreadsheetBundlePathElem>(b.bundle_path, b.bundle_path_num);
}

bool spreadsheet_table_id_match(const SpreadsheetTableID &a, const SpreadsheetTableID &b)
{
  if (a.type != b.type) {
    return false;
  }
  const eSpreadsheetTableIDType type = eSpreadsheetTableIDType(a.type);
  switch (type) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
      const auto &a_ = *reinterpret_cast<const SpreadsheetTableIDGeometry *>(&a);
      const auto &b_ = *reinterpret_cast<const SpreadsheetTableIDGeometry *>(&b);
      return BKE_viewer_path_equal(
                 &a_.viewer_path, &b_.viewer_path, VIEWER_PATH_EQUAL_FLAG_IGNORE_ITERATION) &&
             a_.geometry_component_type == b_.geometry_component_type &&
             a_.attribute_domain == b_.attribute_domain &&
             a_.object_eval_state == b_.object_eval_state && a_.layer_index == b_.layer_index &&
             blender::Span(a_.instance_ids, a_.instance_ids_num) ==
                 blender::Span(b_.instance_ids, b_.instance_ids_num) &&
             bundle_path_match(a_.viewer_item_bundle_path, b_.viewer_item_bundle_path) &&
             bundle_path_match(a_.geometry_bundle_path, b_.geometry_bundle_path);
    }
  }
  return true;
}

SpreadsheetTable *spreadsheet_table_new(SpreadsheetTableID *table_id)
{
  SpreadsheetTable *spreadsheet_table = MEM_new<SpreadsheetTable>(__func__);
  spreadsheet_table->id = table_id;
  return spreadsheet_table;
}

SpreadsheetTable *spreadsheet_table_copy(const SpreadsheetTable &src_table)
{
  SpreadsheetTable *new_table = spreadsheet_table_new(spreadsheet_table_id_copy(*src_table.id));
  new_table->num_columns = src_table.num_columns;
  new_table->columns = MEM_new_array<SpreadsheetColumn *>(src_table.num_columns, __func__);
  for (const int i : IndexRange(src_table.num_columns)) {
    new_table->columns[i] = spreadsheet_column_copy(src_table.columns[i]);
  }
  return new_table;
}

void spreadsheet_table_free(SpreadsheetTable *table)
{
  spreadsheet_table_id_free(table->id);
  for (const int i : IndexRange(table->num_columns)) {
    spreadsheet_column_free(table->columns[i]);
  }
  MEM_SAFE_DELETE(table->columns);
  MEM_delete(table);
}

void spreadsheet_table_blend_write(BlendWriter *writer, const SpreadsheetTable *table)
{
  writer->write_struct(table);
  spreadsheet_table_id_blend_write(writer, table->id);
  BLO_write_pointer_array(writer, table->num_columns, table->columns);
  for (const int i : IndexRange(table->num_columns)) {
    spreadsheet_column_blend_write(writer, table->columns[i]);
  }
}

void spreadsheet_table_blend_read(BlendDataReader *reader, SpreadsheetTable *table)
{
  BLO_read_struct(reader, SpreadsheetTableID, &table->id);
  spreadsheet_table_id_blend_read(reader, table->id);
  BLO_read_pointer_array(reader, table->num_columns, reinterpret_cast<void **>(&table->columns));
  for (const int i : IndexRange(table->num_columns)) {
    BLO_read_struct(reader, SpreadsheetColumn, &table->columns[i]);
    spreadsheet_column_blend_read(reader, table->columns[i]);
  }
}

void spreadsheet_table_remap_id(SpreadsheetTable &table, const bke::id::IDRemapper &mappings)
{
  spreadsheet_table_id_remap_id(*table.id, mappings);
}

void spreadsheet_table_foreach_id(SpreadsheetTable &table, LibraryForeachIDData *data)
{
  spreadsheet_table_id_foreach_id(*table.id, data);
}

SpreadsheetTable *spreadsheet_table_find(SpaceSpreadsheet &sspreadsheet,
                                         const SpreadsheetTableID &table_id)
{
  return const_cast<SpreadsheetTable *>(
      spreadsheet_table_find(const_cast<const SpaceSpreadsheet &>(sspreadsheet), table_id));
}

const SpreadsheetTable *spreadsheet_table_find(const SpaceSpreadsheet &sspreadsheet,
                                               const SpreadsheetTableID &table_id)
{
  for (const SpreadsheetTable *table : Span{sspreadsheet.tables, sspreadsheet.num_tables}) {
    if (spreadsheet_table_id_match(table_id, *table->id)) {
      return table;
    }
  }
  return nullptr;
}

void spreadsheet_table_add(SpaceSpreadsheet &sspreadsheet, SpreadsheetTable *table)
{
  SpreadsheetTable **new_tables = MEM_new_array<SpreadsheetTable *>(sspreadsheet.num_tables + 1,
                                                                    __func__);
  std::copy_n(sspreadsheet.tables, sspreadsheet.num_tables, new_tables);
  new_tables[sspreadsheet.num_tables] = table;
  MEM_SAFE_DELETE(sspreadsheet.tables);
  sspreadsheet.tables = new_tables;
  sspreadsheet.num_tables++;
}

void spreadsheet_table_remove_unused(SpaceSpreadsheet &sspreadsheet)
{
  uint32_t min_last_used = 0;
  const int max_tables = 50;
  if (sspreadsheet.num_tables > max_tables) {
    Vector<uint32_t> last_used_times;
    for (const SpreadsheetTable *table : Span(sspreadsheet.tables, sspreadsheet.num_tables)) {
      last_used_times.append(table->last_used);
    }
    std::sort(last_used_times.begin(), last_used_times.end());
    min_last_used = last_used_times[sspreadsheet.num_tables - max_tables];
  }

  dna::array::remove_if<SpreadsheetTable *>(
      &sspreadsheet.tables,
      &sspreadsheet.num_tables,
      [&](const SpreadsheetTable *table) {
        if (!(table->flag & SPREADSHEET_TABLE_FLAG_MANUALLY_EDITED)) {
          /* Remove tables that have never been modified manually. Those can be rebuilt from
           * scratch if necessary. */
          return true;
        }
        if (table->last_used < min_last_used) {
          /* The table has not been used for a while and there are too many unused tables. So
           * garbage collect this table. This does remove user-edited column widths and orders, but
           * doesn't remove any actual data. */
          return true;
        }
        switch (eSpreadsheetTableIDType(table->id->type)) {
          case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY: {
            const SpreadsheetTableIDGeometry &table_id =
                *reinterpret_cast<const SpreadsheetTableIDGeometry *>(table->id);
            for (ViewerPathElem &elem : table_id.viewer_path.path) {
              if (elem.type == VIEWER_PATH_ELEM_TYPE_ID) {
                const IDViewerPathElem &id_elem = reinterpret_cast<const IDViewerPathElem &>(elem);
                if (!id_elem.id) {
                  /* Remove tables which reference an ID that does not exist anymore. */
                  return true;
                }
              }
            }
            break;
          }
        }
        return false;
      },
      [](SpreadsheetTable **table) { spreadsheet_table_free(*table); });
}

void spreadsheet_table_remove_unused_columns(SpreadsheetTable &table)
{
  /* Might not be reached exactly if there are many columns with the same last used time. */
  const int max_unavailable_columns_target = 50;
  int num_unavailable_columns = 0;
  for (SpreadsheetColumn *column : Span(table.columns, table.num_columns)) {
    if (!column->is_available()) {
      num_unavailable_columns++;
    }
  }
  if (num_unavailable_columns <= max_unavailable_columns_target) {
    /* No need to remove columns. */
    return;
  }

  /* Find the threshold time for unavailable columns to remove. */
  Vector<uint32_t> last_used_times;
  for (SpreadsheetColumn *column : Span(table.columns, table.num_columns)) {
    if (!column->is_available()) {
      last_used_times.append(column->last_used);
    }
  }
  std::sort(last_used_times.begin(), last_used_times.end());
  const int min_last_used = last_used_times[max_unavailable_columns_target];

  dna::array::remove_if<SpreadsheetColumn *>(
      &table.columns,
      &table.num_columns,
      [&](const SpreadsheetColumn *column) {
        if (column->is_available()) {
          /* Available columns should never be removed here. */
          return false;
        }
        if (column->last_used > min_last_used) {
          /* Columns that have been used recently are not removed. */
          return false;
        }
        return true;
      },
      [](SpreadsheetColumn **column) { spreadsheet_column_free(*column); });
}

void spreadsheet_table_move_to_front(SpaceSpreadsheet &sspreadsheet, SpreadsheetTable &table)
{
  const int old_index = Span(sspreadsheet.tables, sspreadsheet.num_tables).first_index(&table);
  dna::array::move_index(sspreadsheet.tables, sspreadsheet.num_tables, old_index, 0);
}

}  // namespace blender::ed::spreadsheet
