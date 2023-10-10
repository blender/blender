/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sys_types.h"

/** \file
 * \ingroup bmesh
 *
 * `BMLog` is the undo system used by DynTopo. Changes
 *  to a `BMesh` are stored incrementally.
 *
 *  The following operations are supported for logging:
 * - Adding and removing vertices
 * - Adding and removing edges
 * - Adding and removing faces
 * - Attribute changes.
 * - Element header flags.
 *
 * Internal details:
 *
 * Each sculpt undo step owns a pointer to a `BMLogEntry`.
 * Every `BMLogEntry` in turn has a list of log sets.
 *
 * A log set is a subclass of `BMLogSetBase` and can be
 * either a delta set (`BMLogSetDiff`) or a full mesh
 * (BMLogSetFull).
 *
 * Particuarly complex mesh operations can sometimes benefit from
 * having a clean `BMLogSetDiff` set, this helps avoid corrupting
 * element IDs. This can be done with `BM_log_entry_add_delta_set`.
 *
 * To log a complete mesh, use `BM_log_full_mesh`.
 */

struct BMesh;
struct BMEdge;
struct BMElem;
struct BMFace;
struct BMIdMap;
struct BMLog;
struct BMLogEntry;
struct BMVert;
struct CustomData;

struct BMLogCallbacks {
  void (*on_vert_add)(BMVert *v, void *userdata);
  void (*on_vert_kill)(BMVert *v, void *userdata);
  void (*on_vert_change)(BMVert *v, void *userdata, void *old_customdata);

  void (*on_edge_add)(BMEdge *e, void *userdata);
  void (*on_edge_kill)(BMEdge *e, void *userdata);
  void (*on_edge_change)(BMEdge *e, void *userdata, void *old_customdata);

  void (*on_face_add)(BMFace *f, void *userdata);
  void (*on_face_kill)(BMFace *f, void *userdata);
  void (*on_face_change)(BMFace *f, void *userdata, void *old_customdata, char old_hflag);

  void (*on_full_mesh_load)(void *userdata);
  void (*on_mesh_customdata_change)(CustomData *domain, char htype, void *userdata);
  void *userdata;
};

/* Allocate and initialize a new BMLog */
BMLog *BM_log_create(BMesh *bm, BMIdMap *idmap);

/* Allocate and initialize a new BMLog using existing BMLogEntries
 */
BMLog *BM_log_from_existing_entries_create(BMesh *bm, BMIdMap *idmap, BMLogEntry *entry);

/* Does not free the log's entries, just the BMLog itself. */
bool BM_log_free(BMLog *log);

/* Start a new log entry and update the log entry list */
BMLogEntry *BM_log_entry_add(BMesh *bm, BMLog *log);

/*
 * Add a new delta set to the current log entry.  If no entry
 * exists one will be created. Returns current log entry.
 */
BMLogEntry *BM_log_entry_add_delta_set(BMesh *bm, BMLog *log);

/* Check if customdata layout has changed. If it has a new
 * subentry will be pushed so any further logging will have
 * the correct customdata.
 */
BMLogEntry *BM_log_entry_check_customdata(BMesh *bm, BMLog *log);

/* Undo one BMLogEntry. */
void BM_log_undo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks);
/* Skip one BMLogEntry. */
void BM_log_undo_skip(BMesh *bm, BMLog *log);

/* Redo one BMLogEntry */
void BM_log_redo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks);
/* Skip one BMLogEntry. */
void BM_log_redo_skip(BMesh *bm, BMLog *log);

/* Removes and deallocates a log entry. */
bool BM_log_entry_drop(BMLogEntry *entry);
void BM_log_set_idmap(BMLog *log, BMIdMap *idmap);

/* Log a vertex if it hasn't been logged in this undo step yet. */
void BM_log_vert_if_modified(BMesh *bm, BMLog *log, BMVert *v);
void BM_log_vert_modified(BMesh *bm, BMLog *log, BMVert *v);

/* Log a new vertex as added to the BMesh
 *
 * The new vertex gets a unique ID assigned. It is then added to a map
 * of added vertices, with the key being its ID and the value
 * containing everything needed to reconstruct that vertex.
 */
void BM_log_vert_added(BMesh *bm, BMLog *log, BMVert *v);
/* Log a vertex as removed from the BMesh */
void BM_log_vert_removed(BMesh *bm, BMLog *log, BMVert *v);

/* Log a new edge as added to the BMesh */
void BM_log_edge_added(BMesh *bm, BMLog *log, BMEdge *e);
/* Log an edge's flags and customdata. */
void BM_log_edge_modified(BMesh *bm, BMLog *log, BMEdge *e);
/* Log an edge as removed from the BMesh */
void BM_log_edge_removed(BMesh *bm, BMLog *log, BMEdge *e);

/* Log a face's flags and customdata. */
void BM_log_face_modified(BMesh *bm, BMLog *log, BMFace *f);
/* Log a face's flags and customdata if it doesn't exist in the log already. */
void BM_log_face_if_modified(BMesh *bm, BMLog *log, BMFace *f);
/* Log a new face as added to the BMesh. */
void BM_log_face_added(BMesh *bm, BMLog *log, BMFace *f);
/* Log a face as removed from the BMesh */
void BM_log_face_removed(BMesh *bm, BMLog *log, BMFace *f);
/* Logs a face as removed without checking if it's already been logged.*/
void BM_log_face_removed_no_check(BMesh *bm, BMLog *log, BMFace *f);

/* Log the complete mesh, will be stored as
 * a Mesh copy.
 */
void BM_log_full_mesh(BMesh *bm, BMLog *log);

/* Called from sculpt undo code. */
void BM_log_print_entry(BMLog *log, BMLogEntry *entry);
int BM_log_entry_size(BMLogEntry *entry);
