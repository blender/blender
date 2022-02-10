/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct ID;
struct Main;
struct Object;
struct SpaceNode;
struct SpaceSpreadsheet;
struct SpreadsheetContext;
struct bContext;
struct bNode;

#ifdef __cplusplus
extern "C" {
#endif

struct SpreadsheetContext *ED_spreadsheet_context_new(int type);
void ED_spreadsheet_context_free(struct SpreadsheetContext *context);
void ED_spreadsheet_context_path_clear(struct SpaceSpreadsheet *sspreadsheet);
bool ED_spreadsheet_context_path_update_tag(struct SpaceSpreadsheet *sspreadsheet);
uint64_t ED_spreadsheet_context_path_hash(const struct SpaceSpreadsheet *sspreadsheet);

struct ID *ED_spreadsheet_get_current_id(const struct SpaceSpreadsheet *sspreadsheet);

void ED_spreadsheet_context_path_set_geometry_node(struct SpaceSpreadsheet *sspreadsheet,
                                                   struct SpaceNode *snode,
                                                   struct bNode *node);
void ED_spreadsheet_context_paths_set_geometry_node(struct Main *bmain,
                                                    struct SpaceNode *snode,
                                                    struct bNode *node);
void ED_spreadsheet_context_path_set_evaluated_object(struct SpaceSpreadsheet *sspreadsheet,
                                                      struct Object *object);

void ED_spreadsheet_context_path_guess(const struct bContext *C,
                                       struct SpaceSpreadsheet *sspreadsheet);
bool ED_spreadsheet_context_path_is_active(const struct bContext *C,
                                           struct SpaceSpreadsheet *sspreadsheet);
bool ED_spreadsheet_context_path_exists(struct Main *bmain, struct SpaceSpreadsheet *sspreadsheet);

#ifdef __cplusplus
}
#endif
