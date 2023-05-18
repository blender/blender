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

//#define BM_LOG_TRACE
#ifdef BM_LOG_TRACE
#  define BMLOG_DEBUG_ARGS , const char *func, int line
#  define BMLOG_DEBUG_ARGS_VALUES , func, line
#  define BMLOG_DEBUG_ARGS_INVOKE , __func__, __LINE__
#else
#  define BMLOG_DEBUG_ARGS
#  define BMLOG_DEBUG_ARGS_VALUES
#  define BMLOG_DEBUG_ARGS_INVOKE
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct BMFace;
struct BMVert;
struct BMesh;
struct RangeTreeUInt;
struct BMIdMap;
struct CustomData;

#ifdef __cplusplus
namespace blender {
struct BMLog;
struct BMLogEntry;
}  // namespace blender

using BMLog = blender::BMLog;
using BMLogEntry = blender::BMLogEntry;
#else
typedef struct BMLog BMLog;
typedef struct BMLogEntry BMLogEntry;
#endif

typedef struct BMLogCallbacks {
  void (*on_vert_add)(struct BMVert *v, void *userdata);
  void (*on_vert_kill)(struct BMVert *v, void *userdata);
  void (*on_vert_change)(struct BMVert *v, void *userdata, void *old_customdata);

  void (*on_edge_add)(struct BMEdge *e, void *userdata);
  void (*on_edge_kill)(struct BMEdge *e, void *userdata);
  void (*on_edge_change)(struct BMEdge *e, void *userdata, void *old_customdata);

  void (*on_face_add)(struct BMFace *f, void *userdata);
  void (*on_face_kill)(struct BMFace *f, void *userdata);
  void (*on_face_change)(struct BMFace *f, void *userdata, void *old_customdata, char old_hflag);

  void (*on_full_mesh_load)(void *userdata);
  void (*on_mesh_customdata_change)(struct CustomData *domain, char htype, void *userdata);
  void *userdata;
} BMLogCallbacks;

//#define DEBUG_LOG_CALL_STACKS

#ifdef DEBUG_LOG_CALL_STACKS
void _bm_logstack_pop();
const char *_bm_logstack_head();
void _bm_logstack_push(const char *name);
#  define bm_logstack_push() _bm_logstack_push(__func__)
#  define bm_logstack_pop() _bm_logstack_pop()
#else
#  define bm_logstack_push()
#  define bm_logstack_head ""
#  define bm_logstack_pop()
#endif

struct BMIdMap;

/* Allocate and initialize a new BMLog */
BMLog *BM_log_create(BMesh *bm, struct BMIdMap *idmap);

/* Allocate and initialize a new BMLog using existing BMLogEntries */
/* Allocate and initialize a new BMLog using existing BMLogEntries
 *
 * The 'entry' should be the last entry in the BMLog. Its prev pointer
 * will be followed back to find the first entry.
 *
 * The unused IDs field of the log will be initialized by taking all
 * keys from all GHashes in the log entry.
 */
BMLog *BM_log_from_existing_entries_create(BMesh *bm, struct BMIdMap *idmap, BMLogEntry *entry);

/* Free all the data in a BMLog including the log itself */
bool BM_log_free(BMLog *log, bool safe_mode);

BMLog *BM_log_unfreeze(BMesh *bm, BMLogEntry *entry);

/* Get the number of log entries */
/* Get the number of log entries */
int BM_log_length(const BMLog *log);

/* Apply a consistent ordering to BMesh vertices and faces */
/* Apply a consistent ordering to BMesh vertices */
void BM_log_mesh_elems_reorder(BMesh *bm, BMLog *log);

/* Start a new log entry and update the log entry list */
BMLogEntry *BM_log_entry_add(BMesh *bm, BMLog *log);
BMLogEntry *BM_log_entry_add_ex(BMesh *bm, BMLog *log, bool combine_with_last);
BMLogEntry *BM_log_all_ids(BMesh *bm, BMLog *log, BMLogEntry *entry);

BMLogEntry *BM_log_entry_check_customdata(BMesh *bm, BMLog *log);

/* Mark all used ids as unused for this node */
void BM_log_cleanup_entry(BMLogEntry *entry);

/* Remove an entry from the log.
   returns true if the log's refcount
   reached zero and was freed*/
bool BM_log_entry_drop(BMLogEntry *entry);
bool BM_log_is_dead(BMLog *log);

/* Undo one BMLogEntry. */
void BM_log_undo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks);

/* Redo one BMLogEntry */
void BM_log_redo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks);

/* Log a vertex before it is modified */
void BM_log_vert_before_modified(BMesh *bm, BMLog *log, BMVert *v);

/* Log a new vertex as added to the BMesh
 *
 * The new vertex gets a unique ID assigned. It is then added to a map
 * of added vertices, with the key being its ID and the value
 * containing everything needed to reconstruct that vertex.
 */
void BM_log_vert_added(BMesh *bm, BMLog *log, struct BMVert *v);

/* Log a new edge as added to the BMesh */
void BM_log_edge_added(BMesh *bm, BMLog *log, BMEdge *e);

void BM_log_edge_modified(BMesh *bm, BMLog *log, BMEdge *e);

/* Log a face before it is modified
 *
 * This is intended to handle only header flags and we always
 * assume face has been added before
 */
void BM_log_face_modified(BMesh *bm, BMLog *log, struct BMFace *f);

/* Log a new face as added to the BMesh */
/* Log a new face as added to the BMesh
 *
 * The new face gets a unique ID assigned. It is then added to a map
 * of added faces, with the key being its ID and the value containing
 * everything needed to reconstruct that face.
 */
void BM_log_face_added(BMesh *bm, BMLog *log, struct BMFace *f);

/* Log a vertex as removed from the BMesh */
/* Log a vertex as removed from the BMesh
 *
 * A couple things can happen here:
 *
 * If the vertex was added as part of the current log entry, then it's
 * deleted and forgotten about entirely. Its unique ID is returned to
 * the unused pool.
 *
 * If the vertex was already part of the BMesh before the current log
 * entry, it is added to a map of deleted vertices, with the key being
 * its ID and the value containing everything needed to reconstruct
 * that vertex.
 *
 * If there's a move record for the vertex, that's used as the
 * vertices original location, then the move record is deleted.
 */
void BM_log_vert_removed(BMesh *bm, BMLog *log, struct BMVert *v);

/* Log an edge as removed from the BMesh */
void BM_log_edge_removed(BMesh *bm, BMLog *log, BMEdge *e);

/* Log a face as removed from the BMesh */
/* Log a face as removed from the BMesh
 *
 * A couple things can happen here:
 *
 * If the face was added as part of the current log entry, then it's
 * deleted and forgotten about entirely. Its unique ID is returned to
 * the unused pool.
 *
 * If the face was already part of the BMesh before the current log
 * entry, it is added to a map of deleted faces, with the key being
 * its ID and the value containing everything needed to reconstruct
 * that face.
 */
void BM_log_face_removed(BMesh *bm, BMLog *log, struct BMFace *f);
void BM_log_face_removed_no_check(BMesh *bm, BMLog *log, struct BMFace *f);

/* Log all vertices/faces in the BMesh as added */
/* Log all vertices/faces in the BMesh as added */
void BM_log_all_added(BMesh *bm, BMLog *log);

void BM_log_full_mesh(BMesh *bm, BMLog *log);

/* Log all vertices/faces in the BMesh as removed */
/* Log all vertices/faces in the BMesh as removed */
void BM_log_before_all_removed(BMesh *bm, BMLog *log);

/* Get the logged coordinates of a vertex */
/* Get the logged coordinates of a vertex
 *
 * Does not modify the log or the vertex */
const float *BM_log_original_vert_co(BMLog *log, BMVert *v);

/* Get the logged normal of a vertex
 *
 * Does not modify the log or the vertex */
const float *BM_log_original_vert_no(BMLog *log, BMVert *v);

/* Get the logged mask of a vertex */
/* Get the logged mask of a vertex
 *
 * Does not modify the log or the vertex */
float BM_log_original_mask(BMLog *log, BMVert *v);

/* Get the logged data of a vertex (avoid multiple lookups) */
void BM_log_original_vert_data(BMLog *log, BMVert *v, const float **r_co, const float **r_no);

/* For internal use only (unit testing) */
/* For internal use only (unit testing) */
BMLogEntry *BM_log_current_entry(BMLog *log);
void BM_log_set_current_entry(BMLog *log, BMLogEntry *entry);
BMLogEntry *BM_log_entry_prev(BMLogEntry *entry);
BMLogEntry *BM_log_entry_next(BMLogEntry *entry);

uint BM_log_vert_id_get(BMesh *bm, BMLog *log, BMVert *v);
BMVert *BM_log_id_vert_get(BMesh *bm, BMLog *log, uint id);
uint BM_log_face_id_get(BMesh *bm, BMLog *log, BMFace *f);
BMFace *BM_log_id_face_get(BMesh *bm, BMLog *log, uint id);

void BM_log_print_entry(BMLog *log, BMLogEntry *entry);
void BM_log_redo_skip(BMesh *bm, BMLog *log);
void BM_log_undo_skip(BMesh *bm, BMLog *log);
BMVert *BM_log_edge_split_do(BMLog *log, BMEdge *e, BMVert *v, BMEdge **newe, float t);

void BM_log_set_idmap(BMLog *log, struct BMIdMap *idmap);

int BM_log_entry_size(BMLogEntry *entry);

bool BM_log_has_vert(BMLog *log, BMVert *v);
bool BM_log_has_edge(BMLog *log, BMEdge *e);
bool BM_log_has_face(BMLog *log, BMFace *f);

bool BM_log_has_vert_post(BMLog *log, BMVert *v);
bool BM_log_has_edge_post(BMLog *log, BMEdge *e);
bool BM_log_has_face_post(BMLog *log, BMFace *f);

bool BM_log_has_vert_pre(BMLog *log, BMVert *v);
bool BM_log_has_edge_pre(BMLog *log, BMEdge *e);
bool BM_log_has_face_pre(BMLog *log, BMFace *f);

bool BM_log_validate(BMesh *inbm, BMLogEntry *entry, bool is_applied);
bool BM_log_validate_cur(BMLog *log);
#ifdef __cplusplus
}
#endif
