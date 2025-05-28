/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_lib_remap.hh"

#include "DNA_space_types.h"

namespace blender::ed::spreadsheet {

SpreadsheetTableIDGeometry *spreadsheet_table_id_new_geometry();
SpreadsheetTableID *spreadsheet_table_id_copy(const SpreadsheetTableID &src_table_id);
void spreadsheet_table_id_copy_content_geometry(SpreadsheetTableIDGeometry &dst,
                                                const SpreadsheetTableIDGeometry &src);
void spreadsheet_table_id_free(SpreadsheetTableID *table_id);
void spreadsheet_table_id_free_content(SpreadsheetTableID *table_id);
void spreadsheet_table_id_blend_write(BlendWriter *writer, const SpreadsheetTableID *table_id);
void spreadsheet_table_id_blend_write_content_geometry(BlendWriter *writer,
                                                       const SpreadsheetTableIDGeometry *table_id);
void spreadsheet_table_id_blend_read(BlendDataReader *reader, SpreadsheetTableID *table_id);
void spreadsheet_table_id_remap_id(SpreadsheetTableID &table_id,
                                   const bke::id::IDRemapper &mappings);
void spreadsheet_table_id_foreach_id(SpreadsheetTableID &table_id, LibraryForeachIDData *data);

/**
 * Checks if two table ids refer to the same table. This is not the same as a full equality check,
 * because e.g. the iteration index for Geometry Nodes loops is ignored.
 */
bool spreadsheet_table_id_match(const SpreadsheetTableID &a, const SpreadsheetTableID &b);

SpreadsheetTable *spreadsheet_table_new(SpreadsheetTableID *table_id);
SpreadsheetTable *spreadsheet_table_copy(const SpreadsheetTable &src_table);
void spreadsheet_table_free(SpreadsheetTable *table);
void spreadsheet_table_blend_write(BlendWriter *writer, const SpreadsheetTable *table);
void spreadsheet_table_blend_read(BlendDataReader *reader, SpreadsheetTable *table);
void spreadsheet_table_remap_id(SpreadsheetTable &table, const bke::id::IDRemapper &mappings);
void spreadsheet_table_foreach_id(SpreadsheetTable &table, LibraryForeachIDData *data);

SpreadsheetTable *spreadsheet_table_find(SpaceSpreadsheet &sspreadsheet,
                                         const SpreadsheetTableID &table_id);
const SpreadsheetTable *spreadsheet_table_find(const SpaceSpreadsheet &sspreadsheet,
                                               const SpreadsheetTableID &table_id);
void spreadsheet_table_add(SpaceSpreadsheet &sspreadsheet, SpreadsheetTable *table);
void spreadsheet_table_remove_unused(SpaceSpreadsheet &sspreadsheet);
void spreadsheet_table_remove_unused_columns(SpreadsheetTable &table);
void spreadsheet_table_move_to_front(SpaceSpreadsheet &sspreadsheet, SpreadsheetTable &table);

}  // namespace blender::ed::spreadsheet
