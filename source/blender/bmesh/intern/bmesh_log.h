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

/** \file
 * \ingroup bmesh
 */

struct BMFace;
struct BMVert;
struct BMesh;
struct RangeTreeUInt;

typedef struct BMLog BMLog;
typedef struct BMLogEntry BMLogEntry;

typedef struct BMLogCallbacks {
  void (*on_vert_add)(struct BMVert *v, void *userdata);
  void (*on_vert_kill)(struct BMVert *v, void *userdata);
  void (*on_vert_change)(struct BMVert *v, void *userdata, void *old_customdata);

  void (*on_face_add)(struct BMFace *f, void *userdata);
  void (*on_face_kill)(struct BMFace *f, void *userdata);
  void (*on_face_change)(struct BMFace *f, void *userdata, void *old_customdata);

  void (*on_full_mesh_load)(void *userdata);
  void (*on_mesh_id_restore)(void *userdata);
  void *userdata;
} BMLogCallbacks;

/* Allocate and initialize a new BMLog */
BMLog *BM_log_create(BMesh *bm, int cd_dyn_vert);
void BM_log_set_cd_offsets(BMLog *log, int cd_dyn_vert);

/* Allocate and initialize a new BMLog using existing BMLogEntries */
BMLog *BM_log_from_existing_entries_create(BMesh *bm, BMLogEntry *entry);

/* Free all the data in a BMLog including the log itself */
bool BM_log_free(BMLog *log, bool safe_mode);

BMLog *BM_log_unfreeze(BMesh *bm, BMLogEntry *entry);

void BM_log_set_bm(BMesh *bm, BMLog *log);

/* Get the number of log entries */
int BM_log_length(const BMLog *log);

/* Apply a consistent ordering to BMesh vertices and faces */
void BM_log_mesh_elems_reorder(BMesh *bm, BMLog *log);

/* Start a new log entry and update the log entry list */
BMLogEntry *BM_log_entry_add(BMesh *bm, BMLog *log);
BMLogEntry *BM_log_entry_add_ex(BMesh *bm, BMLog *log, bool combine_with_last);
BMLogEntry *BM_log_all_ids(BMesh *bm, BMLog *log, BMLogEntry *entry);

BMLogEntry *BM_log_entry_check_customdata(BMesh *bm, BMLog *log);

/* Mark all used ids as unused for this node */
void BM_log_cleanup_entry(BMLogEntry *entry);

/* Remove an entry from the log */
void BM_log_entry_drop(BMLogEntry *entry);

/* Undo one BMLogEntry.  node_layer_id is necassary to preserve node idxs with customdata, whose
 * layout might have changed */
void BM_log_undo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks, const char *node_layer_id);

/* Redo one BMLogEntry */
void BM_log_redo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks, const char *node_layer_id);

/* Log a vertex before it is modified */
void BM_log_vert_before_modified(BMLog *log,
                                 struct BMVert *v,
                                 const int cd_vert_mask_offset,
                                 bool log_customdata);

/* Log a new vertex as added to the BMesh */
void BM_log_vert_added(BMLog *log, struct BMVert *v, const int cd_vert_mask_offset);

/* Log a face before it is modified */
void BM_log_face_modified(BMLog *log, struct BMFace *f);

/* Log a new face as added to the BMesh */
void BM_log_face_added(BMLog *log, struct BMFace *f);

/* Log a vertex as removed from the BMesh */
void BM_log_vert_removed(BMLog *log, struct BMVert *v, const int cd_vert_mask_offset);

/* Log a face as removed from the BMesh */
void BM_log_face_removed(BMLog *log, struct BMFace *f);

/* Log all vertices/faces in the BMesh as added */
void BM_log_all_added(BMesh *bm, BMLog *log);

void BM_log_full_mesh(BMesh *bm, BMLog *log);

/* Log all vertices/faces in the BMesh as removed */
void BM_log_before_all_removed(BMesh *bm, BMLog *log);

/* Get the logged coordinates of a vertex */
const float *BM_log_original_vert_co(BMLog *log, BMVert *v);

/* Get the logged normal of a vertex */
const float *BM_log_original_vert_no(BMLog *log, BMVert *v);

/* Get the logged mask of a vertex */
float BM_log_original_mask(BMLog *log, BMVert *v);

/* Get the logged data of a vertex (avoid multiple lookups) */
void BM_log_original_vert_data(BMLog *log, BMVert *v, const float **r_co, const float **r_no);

/* For internal use only (unit testing) */
BMLogEntry *BM_log_current_entry(BMLog *log);
void BM_log_set_current_entry(BMLog *log, BMLogEntry *entry);
BMLogEntry *BM_log_entry_prev(BMLogEntry *entry);
BMLogEntry *BM_log_entry_next(BMLogEntry *entry);

uint BM_log_vert_id_get(BMLog *log, BMVert *v);
BMVert *BM_log_id_vert_get(BMLog *log, uint id);
uint BM_log_face_id_get(BMLog *log, BMFace *f);
BMFace *BM_log_id_face_get(BMLog *log, uint id);

void BM_log_print_entry(BMLog *log, BMLogEntry *entry);
void BM_log_redo_skip(BMesh *bm, BMLog *log);
void BM_log_undo_skip(BMesh *bm, BMLog *log);
