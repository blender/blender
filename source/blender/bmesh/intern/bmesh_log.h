/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate, initialize, and assign a new BMLog.
 */
BMLog *BM_log_create(BMesh *bm);

/**
 * Allocate and initialize a new #BMLog using existing #BMLogEntries
 *
 * The 'entry' should be the last entry in the #BMLog. Its `prev` pointer
 * will be followed back to find the first entry.
 *
 * The unused IDs field of the log will be initialized by taking all
 * keys from all GHashes in the log entry.
 */
BMLog *BM_log_from_existing_entries_create(BMesh *bm, BMLogEntry *entry);

/**
 * Free all the data in a BMLog including the log itself.
 */
void BM_log_free(BMLog *log);

/**
 * Get the number of log entries.
 */
int BM_log_length(const BMLog *log);

/** Apply a consistent ordering to BMesh vertices and faces. */
void BM_log_mesh_elems_reorder(BMesh *bm, BMLog *log);

/**
 * Start a new log entry and update the log entry list.
 *
 * If the log entry list is empty, or if the current log entry is the
 * last entry, the new entry is simply appended to the end.
 *
 * Otherwise, the new entry is added after the current entry and all
 * following entries are deleted.
 *
 * In either case, the new entry is set as the current log entry.
 */
BMLogEntry *BM_log_entry_add(BMLog *log);

/** Mark all used ids as unused for this node */
void BM_log_cleanup_entry(BMLogEntry *entry);

/**
 * Remove an entry from the log.
 *
 * Uses entry->log as the log. If the log is NULL, the entry will be
 * free'd but not removed from any list, nor shall its IDs be released.
 *
 * This operation is only valid on the first and last entries in the
 * log. Deleting from the middle will assert.
 */
void BM_log_entry_drop(BMLogEntry *entry);

/**
 * Undo one #BMLogEntry.
 *
 * Has no effect if there's nothing left to undo.
 */
void BM_log_undo(BMesh *bm, BMLog *log);

/**
 * Redo one #BMLogEntry.
 *
 * Has no effect if there's nothing left to redo.
 */
void BM_log_redo(BMesh *bm, BMLog *log);

/**
 * Log a vertex before it is modified.
 *
 * Before modifying vertex coordinates, masks, or hflags, call this
 * function to log its current values. This is better than logging
 * after the coordinates have been modified, because only those
 * vertices that are modified need to have their original values
 * stored.
 *
 * Handles two separate cases:
 *
 * If the vertex was added in the current log entry, update the
 * vertex in the map of added vertices.
 *
 * If the vertex already existed prior to the current log entry, a
 * separate key/value map of modified vertices is used (using the
 * vertex's ID as the key). The values stored in that case are
 * the vertex's original state so that an undo can restore the
 * previous state.
 *
 * On undo, the current vertex state will be swapped with the stored
 * state so that a subsequent redo operation will restore the newer
 * vertex state.
 */
void BM_log_vert_before_modified(BMLog *log, struct BMVert *v, int cd_vert_mask_offset);

/**
 * Log a new vertex as added to the #BMesh.
 *
 * The new vertex gets a unique ID assigned. It is then added to a map
 * of added vertices, with the key being its ID and the value
 * containing everything needed to reconstruct that vertex.
 */
void BM_log_vert_added(BMLog *log, struct BMVert *v, int cd_vert_mask_offset);

/**
 * Log a face before it is modified.
 *
 * This is intended to handle only header flags and we always
 * assume face has been added before.
 */
void BM_log_face_modified(BMLog *log, struct BMFace *f);

/**
 * Log a new face as added to the #BMesh.
 *
 * The new face gets a unique ID assigned. It is then added to a map
 * of added faces, with the key being its ID and the value containing
 * everything needed to reconstruct that face.
 */
void BM_log_face_added(BMLog *log, struct BMFace *f);

/**
 * Log a vertex as removed from the #BMesh.
 *
 * A couple things can happen here:
 *
 * If the vertex was added as part of the current log entry, then it's
 * deleted and forgotten about entirely. Its unique ID is returned to
 * the unused pool.
 *
 * If the vertex was already part of the #BMesh before the current log
 * entry, it is added to a map of deleted vertices, with the key being
 * its ID and the value containing everything needed to reconstruct
 * that vertex.
 *
 * If there's a move record for the vertex, that's used as the
 * vertices original location, then the move record is deleted.
 */
void BM_log_vert_removed(BMLog *log, struct BMVert *v, int cd_vert_mask_offset);

/**
 * Log a face as removed from the #BMesh.
 *
 * A couple things can happen here:
 *
 * If the face was added as part of the current log entry, then it's
 * deleted and forgotten about entirely. Its unique ID is returned to
 * the unused pool.
 *
 * If the face was already part of the #BMesh before the current log
 * entry, it is added to a map of deleted faces, with the key being
 * its ID and the value containing everything needed to reconstruct
 * that face.
 */
void BM_log_face_removed(BMLog *log, struct BMFace *f);

/**
 * Log all vertices/faces in the #BMesh as added.
 */
void BM_log_all_added(BMesh *bm, BMLog *log);

/** Log all vertices/faces in the #BMesh as removed. */
void BM_log_before_all_removed(BMesh *bm, BMLog *log);

/**
 * Get the logged coordinates of a vertex.
 *
 * Does not modify the log or the vertex.
 */
const float *BM_log_original_vert_co(BMLog *log, BMVert *v);

/**
 * Get the logged normal of a vertex
 *
 * Does not modify the log or the vertex.
 */
const float *BM_log_original_vert_no(BMLog *log, BMVert *v);

/**
 * Get the logged mask of a vertex
 *
 * Does not modify the log or the vertex.
 */
float BM_log_original_mask(BMLog *log, BMVert *v);

/** Get the logged data of a vertex (avoid multiple lookups). */
void BM_log_original_vert_data(BMLog *log, BMVert *v, const float **r_co, const float **r_no);

/** For internal use only (unit testing). */
BMLogEntry *BM_log_current_entry(BMLog *log);
/** For internal use only (unit testing) */
struct RangeTreeUInt *BM_log_unused_ids(BMLog *log);

void BM_log_print_entry(BMesh *bm, BMLogEntry *entry);

#ifdef __cplusplus
}
#endif
