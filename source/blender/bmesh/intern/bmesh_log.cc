/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * The BMLog is an interface for storing undo/redo steps as a BMesh is
 * modified. It only stores changes to the BMesh, not full copies.
 *
 * Currently it supports the following types of changes:
 *
 * - Adding and removing vertices
 * - Adding and removing faces
 * - Moving vertices
 * - Setting vertex paint-mask values
 * - Setting vertex hflags
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_pool.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.hh"

#include "bmesh.hh"
#include "bmesh_log.hh"

#include "range_tree.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

struct BMLogFace;
struct BMLogVert;

struct BMLogEntry {
  BMLogEntry *next, *prev;

  /* The following members map from an element ID to one of the log types above. */

  /** Elements that were in the previous entry, but have been deleted. */
  blender::Map<uint, BMLogVert *, 0> deleted_verts;
  blender::Map<uint, BMLogFace *, 0> deleted_faces;

  /** Elements that were not in the previous entry, but are in the result of this entry. */
  blender::Map<uint, BMLogVert *, 0> added_verts;
  blender::Map<uint, BMLogFace *, 0> added_faces;

  /** Vertices whose coordinates, mask value, or hflag have changed. */
  blender::Map<uint, BMLogVert *, 0> modified_verts;
  blender::Map<uint, BMLogFace *, 0> modified_faces;

  blender::Pool<BMLogVert> vert_pool;
  blender::Pool<BMLogFace> face_pool;

  blender::Vector<BMLogVert *, 0> allocated_verts;
  blender::Vector<BMLogFace *, 0> allocated_faces;

  /**
   * This is only needed for dropping BMLogEntries while still in
   * dynamic-topology mode, as that should release vert/face IDs
   * back to the BMLog but no BMLog pointer is available at that time.
   *
   * This field is not guaranteed to be valid, any use of it should
   * check for nullptr.
   */
  BMLog *log;
};

struct BMLog {
  /** Tree of free IDs */
  RangeTreeUInt *unused_ids;

  /**
   * Mapping from unique IDs to vertices and faces
   *
   * Each vertex and face in the log gets a unique `uint`
   * assigned. That ID is taken from the set managed by the
   * unused_ids range tree.
   *
   * The ID is needed because element pointers will change as they
   * are created and deleted.
   */
  blender::Map<uint, BMElem *, 0> id_to_elem;
  blender::Map<BMElem *, uint, 0> elem_to_id;

  /** All #BMLogEntrys, ordered from earliest to most recent. */
  ListBase entries;

  /**
   * The current log entry from entries list
   *
   * If null, then the original mesh from before any of the log
   * entries is current (i.e. there is nothing left to undo.)
   *
   * If equal to the last entry in the entries list, then all log
   * entries have been applied (i.e. there is nothing left to redo.)
   */
  BMLogEntry *current_entry;
};

struct BMLogVert {
  blender::float3 position;
  blender::float3 normal;
  char hflag;
  float mask;
};

struct BMLogFace {
  std::array<uint, 3> v_ids;
  char hflag;
};

/************************* Get/set element IDs ************************/

/* Get the vertex's unique ID from the log */
static uint bm_log_vert_id_get(BMLog *log, BMVert *v)
{
  return log->elem_to_id.lookup(reinterpret_cast<BMElem *>(v));
}

/* Set the vertex's unique ID in the log */
static void bm_log_vert_id_set(BMLog *log, BMVert *v, const uint id)
{
  log->id_to_elem.add_overwrite(id, reinterpret_cast<BMElem *>(v));
  log->elem_to_id.add_overwrite(reinterpret_cast<BMElem *>(v), id);
}

/* Get a vertex from its unique ID */
static BMVert *bm_log_vert_from_id(BMLog *log, const uint id)
{
  return reinterpret_cast<BMVert *>(log->id_to_elem.lookup(id));
}

/* Get the face's unique ID from the log */
static uint bm_log_face_id_get(BMLog *log, BMFace *f)
{
  return log->elem_to_id.lookup(reinterpret_cast<BMElem *>(f));
}

/* Set the face's unique ID in the log */
static void bm_log_face_id_set(BMLog *log, BMFace *f, const uint id)
{
  log->id_to_elem.add_overwrite(id, reinterpret_cast<BMElem *>(f));
  log->elem_to_id.add_overwrite(reinterpret_cast<BMElem *>(f), id);
}

/* Get a face from its unique ID */
static BMFace *bm_log_face_from_id(BMLog *log, const uint id)
{
  return reinterpret_cast<BMFace *>(log->id_to_elem.lookup(id));
}

/************************ BMLogVert / BMLogFace ***********************/

/* Get a vertex's paint-mask value
 *
 * Returns zero if no paint-mask layer is present */
static float vert_mask_get(BMVert *v, const int cd_vert_mask_offset)
{
  if (cd_vert_mask_offset != -1) {
    return BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
  }
  return 0.0f;
}

/* Set a vertex's paint-mask value
 *
 * Has no effect is no paint-mask layer is present */
static void vert_mask_set(BMVert *v, const float new_mask, const int cd_vert_mask_offset)
{
  if (cd_vert_mask_offset != -1) {
    BM_ELEM_CD_SET_FLOAT(v, cd_vert_mask_offset, new_mask);
  }
}

/* Update a BMLogVert with data from a BMVert */
static void bm_log_vert_bmvert_copy(BMLogVert *lv, BMVert *v, const int cd_vert_mask_offset)
{
  copy_v3_v3(lv->position, v->co);
  copy_v3_v3(lv->normal, v->no);
  lv->mask = vert_mask_get(v, cd_vert_mask_offset);
  lv->hflag = v->head.hflag;
}

/* Allocate and initialize a BMLogVert */
static BMLogVert *bm_log_vert_alloc(BMLog *log, BMVert *v, const int cd_vert_mask_offset)
{
  BMLogEntry *entry = log->current_entry;
  BMLogVert *lv = &entry->vert_pool.construct();
  entry->allocated_verts.append(lv);

  bm_log_vert_bmvert_copy(lv, v, cd_vert_mask_offset);

  return lv;
}

/* Allocate and initialize a BMLogFace */
static BMLogFace *bm_log_face_alloc(BMLog *log, BMFace *f)
{
  BMLogEntry *entry = log->current_entry;
  BMLogFace *lf = &entry->face_pool.construct();
  entry->allocated_faces.append(lf);
  BMVert *v[3];

  BLI_assert(f->len == 3);

  // BM_iter_as_array(nullptr, BM_VERTS_OF_FACE, f, (void **)v, 3);
  BM_face_as_array_vert_tri(f, v);

  lf->v_ids[0] = bm_log_vert_id_get(log, v[0]);
  lf->v_ids[1] = bm_log_vert_id_get(log, v[1]);
  lf->v_ids[2] = bm_log_vert_id_get(log, v[2]);

  lf->hflag = f->head.hflag;
  return lf;
}

/************************ Helpers for undo/redo ***********************/

static void bm_log_verts_unmake(BMesh *bm,
                                BMLog *log,
                                const blender::Map<uint, BMLogVert *, 0> &verts)
{
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  for (const auto item : verts.items()) {
    BMVert *v = bm_log_vert_from_id(log, item.key);

    /* Ensure the log has the final values of the vertex before
     * deleting it */
    bm_log_vert_bmvert_copy(item.value, v, cd_vert_mask_offset);

    BM_vert_kill(bm, v);
  }
}

static void bm_log_faces_unmake(BMesh *bm,
                                BMLog *log,
                                const blender::Map<uint, BMLogFace *, 0> &faces)
{
  for (const uint id : faces.keys()) {
    BMFace *f = bm_log_face_from_id(log, id);
    std::array<BMEdge *, 3> e_tri;

    BMLoop *l_iter = BM_FACE_FIRST_LOOP(f);
    for (uint i = 0; i < e_tri.size(); i++, l_iter = l_iter->next) {
      e_tri[i] = l_iter->e;
    }

    /* Remove any unused edges */
    BM_face_kill(bm, f);
    for (uint i = 0; i < e_tri.size(); i++) {
      if (BM_edge_is_wire(e_tri[i])) {
        BM_edge_kill(bm, e_tri[i]);
      }
    }
  }
}

static void bm_log_verts_restore(BMesh *bm,
                                 BMLog *log,
                                 const blender::Map<uint, BMLogVert *, 0> &verts)
{
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  for (const auto item : verts.items()) {
    BMLogVert *lv = item.value;
    BMVert *v = BM_vert_create(bm, lv->position, nullptr, BM_CREATE_NOP);
    vert_mask_set(v, lv->mask, cd_vert_mask_offset);
    v->head.hflag = lv->hflag;
    copy_v3_v3(v->no, lv->normal);
    bm_log_vert_id_set(log, v, item.key);
  }
}

static void bm_log_faces_restore(BMesh *bm,
                                 BMLog *log,
                                 const blender::Map<uint, BMLogFace *, 0> &faces)
{
  const int cd_face_sets = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  for (const auto item : faces.items()) {
    BMLogFace *lf = item.value;
    BMVert *v[3] = {
        bm_log_vert_from_id(log, lf->v_ids[0]),
        bm_log_vert_from_id(log, lf->v_ids[1]),
        bm_log_vert_from_id(log, lf->v_ids[2]),
    };

    BMFace *f = BM_face_create_verts(bm, v, 3, nullptr, BM_CREATE_NOP, true);
    f->head.hflag = lf->hflag;
    bm_log_face_id_set(log, f, item.key);

    /* Ensure face sets have valid values.  Fixes #80174. */
    if (cd_face_sets != -1) {
      BM_ELEM_CD_SET_INT(f, cd_face_sets, 1);
    }
  }
}

static void bm_log_vert_values_swap(BMesh *bm,
                                    BMLog *log,
                                    const blender::Map<uint, BMLogVert *, 0> &verts)
{
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  for (const auto item : verts.items()) {
    BMLogVert *lv = item.value;
    BMVert *v = bm_log_vert_from_id(log, item.key);

    swap_v3_v3(v->co, lv->position);
    swap_v3_v3(v->no, lv->normal);
    std::swap(v->head.hflag, lv->hflag);
    float mask = lv->mask;
    lv->mask = vert_mask_get(v, cd_vert_mask_offset);
    vert_mask_set(v, mask, cd_vert_mask_offset);
  }
}

static void bm_log_face_values_swap(BMLog *log, const blender::Map<uint, BMLogFace *, 0> &faces)
{

  for (const auto item : faces.items()) {
    BMLogFace *lf = item.value;
    BMFace *f = bm_log_face_from_id(log, item.key);

    std::swap(f->head.hflag, lf->hflag);
  }
}

/**********************************************************************/

/* Assign unique IDs to all vertices and faces already in the BMesh */
static void bm_log_assign_ids(BMesh *bm, BMLog *log)
{
  BMIter iter;

  BMVert *v;
  /* Generate vertex IDs */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    uint id = range_tree_uint_take_any(log->unused_ids);
    bm_log_vert_id_set(log, v, id);
  }

  BMFace *f;
  /* Generate face IDs */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    uint id = range_tree_uint_take_any(log->unused_ids);
    bm_log_face_id_set(log, f, id);
  }
}

/* Allocate an empty log entry */
static BMLogEntry *bm_log_entry_create()
{
  BMLogEntry *entry = MEM_new<BMLogEntry>(__func__);

  return entry;
}

/* Free the data in a log entry
 *
 * NOTE: does not free the log entry itself. */
static void bm_log_entry_free(BMLogEntry *entry)
{
  BLI_assert(entry->vert_pool.size() == entry->allocated_verts.size());
  BLI_assert(entry->face_pool.size() == entry->allocated_faces.size());

  for (BMLogVert *log_vert : entry->allocated_verts) {
    entry->vert_pool.destruct(*log_vert);
  }

  for (BMLogFace *log_face : entry->allocated_faces) {
    entry->face_pool.destruct(*log_face);
  }

  BLI_assert(entry->vert_pool.is_empty());
  BLI_assert(entry->face_pool.is_empty());
}

/***************************** Public API *****************************/

BMLog *BM_log_create(BMesh *bm)
{
  BMLog *log = MEM_new<BMLog>(__func__);
  const uint reserve_num = uint(bm->totvert + bm->totface);

  log->unused_ids = range_tree_uint_alloc(0, uint(-1));
  log->id_to_elem.reserve(reserve_num);
  log->elem_to_id.reserve(reserve_num);

  /* Assign IDs to all existing vertices and faces */
  bm_log_assign_ids(bm, log);

  return log;
}

void BM_log_cleanup_entry(BMLogEntry *entry)
{
  BMLog *log = entry->log;

  if (log) {
    /* Take all used IDs */
    for (const uint id : entry->deleted_verts.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->deleted_faces.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->added_verts.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->added_faces.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->modified_verts.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->modified_faces.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }

    /* delete entries to avoid releasing ids in node cleanup */
    entry->deleted_verts.clear();
    entry->deleted_faces.clear();
    entry->added_verts.clear();
    entry->added_faces.clear();
    entry->modified_verts.clear();

    /* Is this last one needed? */
    entry->modified_faces.clear();
  }
}

BMLog *BM_log_from_existing_entries_create(BMesh *bm, BMLogEntry *entry)
{
  BMLog *log = BM_log_create(bm);

  if (entry->prev) {
    log->current_entry = entry;
  }
  else {
    log->current_entry = nullptr;
  }

  /* Let BMLog manage the entry list again */
  log->entries.first = log->entries.last = entry;

  {
    while (entry->prev) {
      entry = entry->prev;
      log->entries.first = entry;
    }
    entry = static_cast<BMLogEntry *>(log->entries.last);
    while (entry->next) {
      entry = entry->next;
      log->entries.last = entry;
    }
  }

  for (entry = static_cast<BMLogEntry *>(log->entries.first); entry; entry = entry->next) {
    entry->log = log;

    /* Take all used IDs */
    for (const uint id : entry->deleted_verts.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->deleted_faces.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->added_verts.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->added_faces.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->modified_verts.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
    for (const uint id : entry->modified_faces.keys()) {
      range_tree_uint_retake(log->unused_ids, id);
    }
  }

  return log;
}

void BM_log_free(BMLog *log)
{
  if (log->unused_ids) {
    range_tree_uint_free(log->unused_ids);
  }

  /* Clear the BMLog references within each entry, but do not free
   * the entries themselves */
  LISTBASE_FOREACH (BMLogEntry *, entry, &log->entries) {
    entry->log = nullptr;
  }

  MEM_delete(log);
}

BMLogEntry *BM_log_entry_add(BMLog *log)
{
  /* WARNING: Deleting any entries after the current one is now handled by the
   * UndoSystem: BKE_UNDOSYS_TYPE_SCULPT freeing here causes unnecessary complications. */

  /* Create and append the new entry */
  BMLogEntry *entry = bm_log_entry_create();
  BLI_addtail(&log->entries, entry);
  entry->log = log;
  log->current_entry = entry;

  return entry;
}

void BM_log_entry_drop(BMLogEntry *entry)
{
  BMLog *log = entry->log;

  if (!log) {
    /* Unlink */
    BLI_assert(!(entry->prev && entry->next));
    if (entry->prev) {
      entry->prev->next = nullptr;
    }
    else if (entry->next) {
      entry->next->prev = nullptr;
    }

    bm_log_entry_free(entry);
    MEM_delete(entry);
    return;
  }

  if (!entry->prev) {
    /* Release IDs of elements that are deleted by this
     * entry. Since the entry is at the beginning of the undo
     * stack, and it's being deleted, those elements can never be
     * restored. Their IDs can go back into the pool. */

    /* This would never happen usually since first entry of log is
     * usually dyntopo enable, which, when reverted will free the log
     * completely. However, it is possible have a stroke instead of
     * dyntopo enable as first entry if nodes have been cleaned up
     * after sculpting on a different object than A, B.
     *
     * The steps are:
     * A dyntopo enable - sculpt
     * B dyntopo enable - sculpt - undo (A objects operators get cleaned up)
     * A sculpt (now A's log has a sculpt operator as first entry)
     *
     * Causing a cleanup at this point will call the code below, however
     * this will invalidate the state of the log since the deleted vertices
     * have been reclaimed already on step 2 (see BM_log_cleanup_entry)
     *
     * Also, design wise, a first entry should not have any deleted vertices since it
     * should not have anything to delete them -from-
     */
    // bm_log_id_ghash_release(log, entry->deleted_faces);
    // bm_log_id_ghash_release(log, entry->deleted_verts);
  }
  else if (!entry->next) {
    /* Release IDs of elements that are added by this entry. Since
     * the entry is at the end of the undo stack, and it's being
     * deleted, those elements can never be restored. Their IDs
     * can go back into the pool. */
    for (const uint id : entry->added_faces.keys()) {
      range_tree_uint_release(log->unused_ids, id);
    }
    for (const uint id : entry->added_verts.keys()) {
      range_tree_uint_release(log->unused_ids, id);
    }
  }
  else {
    BLI_assert_msg(0, "Cannot drop BMLogEntry from middle");
  }

  if (log->current_entry == entry) {
    log->current_entry = entry->prev;
  }

  bm_log_entry_free(entry);
  BLI_remlink(&log->entries, entry);
  MEM_delete(entry);
}

void BM_log_undo(BMesh *bm, BMLog *log)
{
  BMLogEntry *entry = log->current_entry;

  if (entry) {
    log->current_entry = entry->prev;

    /* Delete added faces and verts */
    bm_log_faces_unmake(bm, log, entry->added_faces);
    bm_log_verts_unmake(bm, log, entry->added_verts);

    /* Restore deleted verts and faces */
    bm_log_verts_restore(bm, log, entry->deleted_verts);
    bm_log_faces_restore(bm, log, entry->deleted_faces);

    /* Restore vertex coordinates, mask, and hflag */
    bm_log_vert_values_swap(bm, log, entry->modified_verts);
    bm_log_face_values_swap(log, entry->modified_faces);
  }
}

void BM_log_redo(BMesh *bm, BMLog *log)
{
  BMLogEntry *entry = log->current_entry;

  if (!entry) {
    /* Currently at the beginning of the undo stack, move to first entry */
    entry = static_cast<BMLogEntry *>(log->entries.first);
  }
  else if (entry->next) {
    /* Move to next undo entry */
    entry = entry->next;
  }
  else {
    /* Currently at the end of the undo stack, nothing left to redo */
    return;
  }

  log->current_entry = entry;

  if (entry) {
    /* Re-delete previously deleted faces and verts */
    bm_log_faces_unmake(bm, log, entry->deleted_faces);
    bm_log_verts_unmake(bm, log, entry->deleted_verts);

    /* Restore previously added verts and faces */
    bm_log_verts_restore(bm, log, entry->added_verts);
    bm_log_faces_restore(bm, log, entry->added_faces);

    /* Restore vertex coordinates, mask, and hflag */
    bm_log_vert_values_swap(bm, log, entry->modified_verts);
    bm_log_face_values_swap(log, entry->modified_faces);
  }
}

void BM_log_vert_before_modified(BMLog *log, BMVert *v, const int cd_vert_mask_offset)
{
  BMLogEntry *entry = log->current_entry;
  const uint v_id = bm_log_vert_id_get(log, v);

  /* Find or create the BMLogVert entry */
  if (entry->added_verts.contains(v_id)) {
    bm_log_vert_bmvert_copy(entry->added_verts.lookup(v_id), v, cd_vert_mask_offset);
  }
  else {
    entry->modified_verts.lookup_or_add_cb(
        v_id, [&] { return bm_log_vert_alloc(log, v, cd_vert_mask_offset); });
  }
}

void BM_log_vert_added(BMLog *log, BMVert *v, const int cd_vert_mask_offset)
{
  const uint v_id = range_tree_uint_take_any(log->unused_ids);

  bm_log_vert_id_set(log, v, v_id);
  BMLogVert *lv = bm_log_vert_alloc(log, v, cd_vert_mask_offset);
  log->current_entry->added_verts.add(v_id, lv);
}

void BM_log_face_modified(BMLog *log, BMFace *f)
{
  const uint f_id = bm_log_face_id_get(log, f);

  BMLogFace *lf = bm_log_face_alloc(log, f);
  log->current_entry->modified_faces.add(f_id, lf);
}

void BM_log_face_added(BMLog *log, BMFace *f)
{
  const uint f_id = range_tree_uint_take_any(log->unused_ids);

  /* Only triangles are supported for now */
  BLI_assert(f->len == 3);

  bm_log_face_id_set(log, f, f_id);
  BMLogFace *lf = bm_log_face_alloc(log, f);
  log->current_entry->added_faces.add(f_id, lf);
}

void BM_log_vert_removed(BMLog *log, BMVert *v, const int cd_vert_mask_offset)
{
  BMLogEntry *entry = log->current_entry;
  const uint v_id = bm_log_vert_id_get(log, v);

  BLI_assert(!entry->added_verts.contains(v_id) ||
             (entry->added_verts.contains(v_id) && entry->added_verts.lookup(v_id) != nullptr));

  if (entry->added_verts.remove(v_id)) {
    range_tree_uint_release(log->unused_ids, v_id);
  }
  else {
    BMLogVert *lv = bm_log_vert_alloc(log, v, cd_vert_mask_offset);
    entry->deleted_verts.add(v_id, lv);

    /* If the vertex was modified before deletion, ensure that the
     * original vertex values are stored */
    if (std::optional<BMLogVert *> lv_mod = entry->modified_verts.lookup_try(v_id)) {
      *lv = *lv_mod.value();
      entry->modified_verts.remove(v_id);
    }
  }
}

void BM_log_face_removed(BMLog *log, BMFace *f)
{
  BMLogEntry *entry = log->current_entry;
  const uint f_id = bm_log_face_id_get(log, f);

  BLI_assert(!entry->added_faces.contains(f_id) ||
             (entry->added_faces.contains(f_id) && entry->added_faces.lookup(f_id) != nullptr));

  if (entry->added_faces.remove(f_id)) {
    range_tree_uint_release(log->unused_ids, f_id);
  }
  else {
    BMLogFace *lf = bm_log_face_alloc(log, f);
    entry->deleted_faces.add(f_id, lf);
  }
}

void BM_log_all_added(BMesh *bm, BMLog *log)
{
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  /* avoid unnecessary resizing on initialization */
  if (log->current_entry->added_verts.is_empty()) {
    log->current_entry->added_verts.reserve(bm->totvert);
  }

  if (log->current_entry->added_faces.is_empty()) {
    log->current_entry->added_faces.reserve(bm->totface);
  }

  BMIter bm_iter;
  BMVert *v;
  /* Log all vertices as newly created */
  BM_ITER_MESH (v, &bm_iter, bm, BM_VERTS_OF_MESH) {
    BM_log_vert_added(log, v, cd_vert_mask_offset);
  }

  BMFace *f;
  /* Log all faces as newly created */
  BM_ITER_MESH (f, &bm_iter, bm, BM_FACES_OF_MESH) {
    BM_log_face_added(log, f);
  }
}

void BM_log_before_all_removed(BMesh *bm, BMLog *log)
{
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  BMIter bm_iter;
  BMFace *f;
  /* Log deletion of all faces */
  BM_ITER_MESH (f, &bm_iter, bm, BM_FACES_OF_MESH) {
    BM_log_face_removed(log, f);
  }

  BMVert *v;
  /* Log deletion of all vertices */
  BM_ITER_MESH (v, &bm_iter, bm, BM_VERTS_OF_MESH) {
    BM_log_vert_removed(log, v, cd_vert_mask_offset);
  }
}

const float *BM_log_find_original_vert_co(BMLog *log, BMVert *v)
{
  BMLogEntry *entry = log->current_entry;
  const uint v_id = bm_log_vert_id_get(log, v);

  if (std::optional<BMLogVert *> log_vert = entry->modified_verts.lookup_try(v_id)) {
    return log_vert.value()->position;
  }
  return nullptr;
}

const float *BM_log_find_original_vert_mask(BMLog *log, BMVert *v)
{
  BMLogEntry *entry = log->current_entry;
  const uint v_id = bm_log_vert_id_get(log, v);

  if (std::optional<BMLogVert *> log_vert = entry->modified_verts.lookup_try(v_id)) {
    return &log_vert.value()->mask;
  }
  return nullptr;
}

void BM_log_original_vert_data(BMLog *log, BMVert *v, const float **r_co, const float **r_no)
{
  BMLogEntry *entry = log->current_entry;
  BLI_assert(entry);

  const uint v_id = bm_log_vert_id_get(log, v);

  BLI_assert(entry->modified_verts.contains(v_id));

  const BMLogVert *lv = entry->modified_verts.lookup(v_id);
  *r_co = lv->position;
  *r_no = lv->normal;
}

/************************ Debugging and Testing ***********************/

#ifndef NDEBUG
BMLogEntry *BM_log_current_entry(BMLog *log)
{
  return log->current_entry;
}

RangeTreeUInt *BM_log_unused_ids(BMLog *log)
{
  return log->unused_ids;
}

/* Print the list of entries, marking the current one
 *
 * Keep around for debugging */
void BM_log_print(const BMLog *log, const char *description)
{
  const BMLogEntry *entry;
  const char *current = " <-- current";
  int i;

  printf("%s:\n", description);
  printf("    % 2d: [ initial ]%s\n", 0, (!log->current_entry) ? current : "");
  for (entry = static_cast<const BMLogEntry *>(log->entries.first), i = 1; entry;
       entry = entry->next, i++)
  {
    printf("    % 2d: [%p]%s\n", i, entry, (entry == log->current_entry) ? current : "");
  }
}

void BM_log_print_entry(BMesh *bm, BMLogEntry *entry)
{
  if (bm) {
    printf("BM { totvert=%d totedge=%d totloop=%d faces_num=%d\n",
           bm->totvert,
           bm->totedge,
           bm->totloop,
           bm->totface);

    if (!bm->totvert) {
      printf("%s: Warning: empty bmesh\n", __func__);
    }
  }
  else {
    printf("BM { totvert=unknown totedge=unknown totloop=unknown faces_num=unknown\n");
  }

  printf("v | added: %d, removed: %d, modified: %d\n",
         int(entry->added_verts.size()),
         int(entry->deleted_verts.size()),
         int(entry->modified_verts.size()));
  printf("f | added: %d, removed: %d, modified: %d\n",
         int(entry->added_faces.size()),
         int(entry->deleted_faces.size()),
         int(entry->modified_faces.size()));
  printf("}\n");
}
#endif
