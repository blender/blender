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

#pragma once

struct SpreadsheetContext;
struct SpaceSpreadsheet;
struct SpaceNode;
struct ID;
struct bNode;

#ifdef __cplusplus
extern "C" {
#endif

struct SpreadsheetContext *ED_spreadsheet_context_new(int type);
void ED_spreadsheet_context_free(struct SpreadsheetContext *context);
void ED_spreadsheet_context_path_clear(struct SpaceSpreadsheet *sspreadsheet);
void ED_spreadsheet_context_path_update_tag(struct SpaceSpreadsheet *sspreadsheet);
uint64_t ED_spreadsheet_context_path_hash(struct SpaceSpreadsheet *sspreadsheet);

struct ID *ED_spreadsheet_get_current_id(struct SpaceSpreadsheet *sspreadsheet);

void ED_spreadsheet_set_geometry_node_context(struct SpaceSpreadsheet *sspreadsheet,
                                              struct SpaceNode *snode,
                                              struct bNode *node);

#ifdef __cplusplus
}
#endif
