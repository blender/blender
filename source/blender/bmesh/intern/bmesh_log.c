
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

#include "BLI_array.h"
#include "BLI_compiler_attrs.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"

#include "BLI_strict_flags.h"
#include "bmesh.h"
#include "bmesh_log.h"
#include "bmesh_private.h"
#include "range_tree.h"

#define CUSTOMDATA

//#define DEBUG_LOG_REFCOUNTNG
//#define PRINT_LOG_REF_COUNTING

#ifdef DEBUG_LOG_REFCOUNTNG
static struct {
  char tag[4192];
} namestack[256] = {0};
int namestack_i = 1;

void _namestack_push(const char *name)
{
  namestack_i++;

  strcpy(namestack[namestack_i].tag, namestack[namestack_i - 1].tag);
  strcat(namestack[namestack_i].tag, ".");
  strcat(namestack[namestack_i].tag, name);
}

#  define namestack_push() _namestack_push(__func__)

void namestack_pop()
{
  namestack_i--;
}

#  define namestack_head_name namestack[namestack_i].tag
#else
#  define namestack_push()
#  define namestack_pop()
#  define namestack_head_name ""
#endif
//#define DO_LOG_PRINT

#ifdef DO_LOG_PRINT
static int msg_idgen = 1;
static char msg_buffer[256] = {0};

#  define SET_MSG(le) memcpy(le->msg, msg_buffer, sizeof(le->msg))
#  define GET_MSG(le) le->msg
#  define LOGPRINT(...) \
    printf("%s: ", __func__); \
    printf(__VA_ARGS__)
struct Mesh;
#else
#  define GET_MSG(le) le ? "" : " "
#  define SET_MSG(le)
#  define LOGPRINT(...)
#endif

#include <stdarg.h>

void bm_log_message(const char *fmt, ...)
{
  char msg[64];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

#ifdef DO_LOG_PRINT
  BLI_snprintf(msg_buffer, 64, "%d %s", msg_idgen, msg);
  msg_idgen++;

  printf("%s\n", msg);
#endif
}

typedef enum { LOG_ENTRY_PARTIAL, LOG_ENTRY_FULL_MESH, LOG_ENTRY_MESH_IDS } BMLogEntryType;

typedef struct BMLogIdMap {
  int elemmask;
  int elemtots[15];
  int *maps[15];
} BMLogIdMap;

struct BMLogEntry {
  struct BMLogEntry *next, *prev;

  /* The following GHashes map from an element ID to one of the log
   * types above */

  /* Elements that were in the previous entry, but have been
   * deleted */
  GHash *deleted_verts;
  GHash *deleted_edges;
  GHash *deleted_edges_post;  // used for split edges
  GHash *deleted_faces;

  /* Elements that were not in the previous entry, but are in the
   * result of this entry */
  GHash *added_verts;
  GHash *added_edges;
  GHash *added_faces;

  /* Vertices whose coordinates, mask value, or hflag have changed */
  GHash *modified_verts;
  GHash *modified_edges;
  GHash *modified_faces;

  BLI_mempool *pool_verts;
  BLI_mempool *pool_edges;
  BLI_mempool *pool_faces;
  MemArena *arena;

  /* This is only needed for dropping BMLogEntries while still in
   * dynamic-topology mode, as that should release vert/face IDs
   * back to the BMLog but no BMLog pointer is available at that
   * time.
   *
   * This field is not guaranteed to be valid, any use of it should
   * check for NULL. */
  BMLog *log;

  CustomData vdata, edata, ldata, pdata;
  struct BMLogEntry *combined_prev, *combined_next;

  BMLogEntryType type;

  struct Mesh
      *full_copy_mesh;  // avoid excessive memory use by saving a Mesh instead of copying the bmesh
  BMLogIdMap idmap;
};

struct BMLog {
  // BMLogEntry *frozen_full_mesh;

  int refcount;

  /* Mapping from unique IDs to vertices and faces
   *
   * Each vertex and face in the log gets a unique uinteger
   * assigned. That ID is taken from the set managed by the
   * unused_ids range tree.
   *
   * The ID is needed because element pointers will change as they
   * are created and deleted.
   */

  ThreadRWMutex lock;

  BMesh *bm;

  /* All BMLogEntrys, ordered from earliest to most recent */
  ListBase entries;

  /* The current log entry from entries list
   *
   * If null, then the original mesh from before any of the log
   * entries is current (i.e. there is nothing left to undo.)
   *
   * If equal to the last entry in the entries list, then all log
   * entries have been applied (i.e. there is nothing left to redo.)
   */
  BMLogEntry *current_entry;

  bool has_edges;
  int cd_dyn_vert;
  bool dead;
};

static void _bm_log_addref(BMLog *log, const char *func)
{
  log->refcount++;

#ifdef PRINT_LOG_REF_COUNTING
  printf("%d %s: bm_log_addref: %p\n", log->refcount, namestack_head_name, log);
  fflush(stdout);
#endif
}

static void _bm_log_decref(BMLog *log, const char *func)
{
  log->refcount--;
#ifdef PRINT_LOG_REF_COUNTING
  printf("%d %s: bm_log_decref: %p\n", log->refcount, namestack_head_name, log);
  fflush(stdout);
#endif
}

#define bm_log_addref(log) _bm_log_addref(log, __func__)
#define bm_log_decref(log) _bm_log_decref(log, __func__)

typedef struct BMLogVert {
#ifdef DO_LOG_PRINT
  char msg[64];
#endif

  float co[3];
  float no[3];
  char hflag;
  void *customdata;
} BMLogVert;

typedef struct BMLogEdge {
#ifdef DO_LOG_PRINT
  char msg[64];
#endif

  uint v1, v2;
  char hflag;
  void *customdata;
  uint id;
} BMLogEdge;

#define MAX_FACE_RESERVED 8

typedef struct {
#ifdef DO_LOG_PRINT
  char msg[64];
#endif

  uint *v_ids;
  uint *l_ids;
  void **customdata;

  float no[3];
  void *customdata_f;
  char hflag;

  size_t len;

  void *customdata_res[MAX_FACE_RESERVED];
  uint v_ids_res[MAX_FACE_RESERVED];
  uint l_ids_res[MAX_FACE_RESERVED];
} BMLogFace;

/************************* Get/set element IDs ************************/

/* bypass actual hashing, the keys don't overlap */
#define logkey_hash BLI_ghashutil_inthash_p_simple
#define logkey_cmp BLI_ghashutil_intcmp

static void log_idmap_load(BMesh *bm, BMLog *log, BMLogEntry *entry);
static void log_idmap_swap(BMesh *bm, BMLog *log, BMLogEntry *entry);
static void log_idmap_free(BMLogEntry *entry);

static void full_copy_swap(BMesh *bm, BMLog *log, BMLogEntry *entry);
static void full_copy_load(BMesh *bm, BMLog *log, BMLogEntry *entry);

BMLogEntry *bm_log_entry_add_ex(
    BMesh *bm, BMLog *log, bool combine_with_last, BMLogEntryType type, BMLogEntry *last_entry);
static bool bm_log_entry_free(BMLogEntry *entry);
static bool bm_log_free_direct(BMLog *log, bool safe_mode);

static void *log_ghash_lookup(BMLog *log, GHash *gh, const void *key)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_READ);
  void *ret = BLI_ghash_lookup(gh, key);
  BLI_rw_mutex_unlock(&log->lock);

  return ret;
}

// this is not 100% threadsafe
static void **log_ghash_lookup_p(BMLog *log, GHash *gh, const void *key)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_READ);
  void **ret = BLI_ghash_lookup_p(gh, key);
  BLI_rw_mutex_unlock(&log->lock);

  return ret;
}

static void log_ghash_insert(BMLog *log, GHash *gh, void *key, void *val)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_WRITE);
  BLI_ghash_insert(gh, key, val);
  BLI_rw_mutex_unlock(&log->lock);
}

static bool log_ghash_remove(
    BMLog *log, GHash *gh, const void *key, GHashKeyFreeFP keyfree, GHashValFreeFP valfree)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_WRITE);
  bool ret = BLI_ghash_remove(gh, key, keyfree, valfree);
  BLI_rw_mutex_unlock(&log->lock);

  return ret;
}

static bool log_ghash_reinsert(
    BMLog *log, GHash *gh, void *key, void *val, GHashKeyFreeFP keyfree, GHashValFreeFP valfree)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_WRITE);
  bool ret = BLI_ghash_reinsert(gh, key, val, keyfree, valfree);
  BLI_rw_mutex_unlock(&log->lock);

  return ret;
}

static void bm_log_copy_id(CustomData *cdata, BMElem *elem, void *data)
{
  int cd_id = cdata->typemap[CD_MESH_ID];

  if (cd_id >= 0) {
    cd_id = cdata->layers[cd_id].offset;

    int id = BM_ELEM_CD_GET_INT(elem, cd_id);

    BMElem elem2;
    elem2.head.data = data;

    BM_ELEM_CD_SET_INT(&elem2, cd_id, id);
  }
}

static bool log_ghash_haskey(BMLog *log, GHash *gh, const void *key)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_READ);
  bool ret = BLI_ghash_haskey(gh, key);
  BLI_rw_mutex_unlock(&log->lock);

  return ret;
}

static bool log_ghash_ensure_p(BMLog *log, GHash *gh, void *key, void ***val)
{
  BLI_rw_mutex_lock(&log->lock, THREAD_LOCK_WRITE);
  bool ret = BLI_ghash_ensure_p(gh, key, val);
  BLI_rw_mutex_unlock(&log->lock);

  return ret;
}

/* Get the vertex's unique ID from the log */
static uint bm_log_vert_id_get(BMLog *log, BMVert *v)
{
  return (uint)BM_ELEM_GET_ID(log->bm, v);
}

/* Get a vertex from its unique ID */
static BMVert *bm_log_vert_from_id(BMLog *log, uint id)
{
  return (BMVert *)BM_ELEM_FROM_ID(log->bm, id);
}

BMVert *BM_log_id_vert_get(BMLog *log, uint id)
{
  return bm_log_vert_from_id(log, id);
}

/* Get the vertex's unique ID from the log */
static uint bm_log_edge_id_get(BMLog *log, BMEdge *e)
{
  return (uint)BM_ELEM_GET_ID(log->bm, e);
}

/* Get a vertex from its unique ID */
static BMEdge *bm_log_edge_from_id(BMLog *log, uint id)
{
  return (BMEdge *)BM_ELEM_FROM_ID(log->bm, id);
}

BMEdge *BM_log_id_edge_get(BMLog *log, uint id)
{
  return bm_log_edge_from_id(log, id);
}

/* Get the face's unique ID from the log */
static uint bm_log_face_id_get(BMLog *log, BMFace *f)
{
  return (uint)BM_ELEM_GET_ID(log->bm, f);
}

uint BM_log_vert_id_get(BMLog *log, BMVert *v)
{
  return bm_log_vert_id_get(log, v);
}

uint BM_log_face_id_get(BMLog *log, BMFace *f)
{
  return bm_log_face_id_get(log, f);
}

/* Get a face from its unique ID */
static BMFace *bm_log_face_from_id(BMLog *log, uint id)
{
  return (BMFace *)BM_ELEM_FROM_ID(log->bm, id);
}

BMFace *BM_log_id_face_get(BMLog *log, uint id)
{
  return bm_log_face_from_id(log, id);
}

/************************ BMLogVert / BMLogFace ***********************/

static void bm_log_vert_customdata(
    BMesh *bm, BMLog *log, BMLogEntry *entry, BMVert *v, BMLogVert *lv)
{
#ifdef CUSTOMDATA
  // if (!lv) {
  //  return;
  //}

  if (lv->customdata) {
    BLI_mempool_free(entry->vdata.pool, lv->customdata);
    lv->customdata = NULL;
  }

  CustomData_bmesh_copy_data(&bm->vdata, &entry->vdata, v->head.data, &lv->customdata);

  // forcibly copy id
  // bm_log_copy_id(&bm->vdata, (BMElem *)v, lv->customdata);

#endif
}

static void bm_log_edge_customdata(
    BMesh *bm, BMLog *log, BMLogEntry *entry, BMEdge *e, BMLogEdge *le)
{
  if (le->customdata) {
    BLI_mempool_free(entry->edata.pool, le->customdata);
    le->customdata = NULL;
  }

  CustomData_bmesh_copy_data(&bm->edata, &entry->edata, e->head.data, &le->customdata);
}

static void bm_log_face_customdata(BMesh *bm, BMLog *log, BMFace *f, BMLogFace *lf)
{
  BMLogEntry *entry = log->current_entry;

  if (!entry || !lf) {
    printf("bmlog error\n");
    return;
  }

  if (lf->customdata_f) {
    BLI_mempool_free(entry->pdata.pool, lf->customdata_f);
    lf->customdata_f = NULL;
  }

  CustomData_bmesh_copy_data(&bm->pdata, &entry->pdata, f->head.data, &lf->customdata_f);

  // forcibly copy id
  // bm_log_copy_id(&bm->pdata, (BMElem *)f, lf->customdata_f);

  BMLoop *l = f->l_first;
  int i = 0;
  do {
    if (lf->customdata[i]) {
      BLI_mempool_free(entry->ldata.pool, lf->customdata[i]);
      lf->customdata[i] = NULL;
    }

    CustomData_bmesh_copy_data(&bm->ldata, &entry->ldata, l->head.data, &lf->customdata[i]);
  } while ((i++, l = l->next) != f->l_first);
}

/* Update a BMLogVert with data from a BMVert */
static void bm_log_vert_bmvert_copy(BMLog *log,
                                    BMLogEntry *entry,
                                    BMLogVert *lv,
                                    BMVert *v,
                                    const int cd_vert_mask_offset,
                                    bool copy_customdata)
{
  copy_v3_v3(lv->co, v->co);
  copy_v3_v3(lv->no, v->no);

  lv->hflag = v->head.hflag;

  if (copy_customdata) {
    bm_log_vert_customdata(log->bm, log, entry, v, lv);
  }
}

/* Allocate and initialize a BMLogVert */
static BMLogVert *bm_log_vert_alloc(BMLog *log,
                                    BMVert *v,
                                    const int cd_vert_mask_offset,
                                    bool log_customdata)
{
  BMLogEntry *entry = log->current_entry;
  BMLogVert *lv = BLI_mempool_alloc(entry->pool_verts);
  lv->customdata = NULL;

  bm_log_vert_bmvert_copy(log, entry, lv, v, -1, log_customdata);

  return lv;
}

static void bm_log_edge_bmedge_copy(
    BMLog *log, BMLogEntry *entry, BMLogEdge *le, BMEdge *e, bool copy_customdata)
{
  if (e->head.htype != BM_EDGE) {
    printf("%s: e is not an edge; htype: %d\n", __func__, (int)e->head.htype);
  }

  le->v1 = (uint)BM_ELEM_GET_ID(log->bm, e->v1);
  le->v2 = (uint)BM_ELEM_GET_ID(log->bm, e->v2);

  le->id = (uint)BM_ELEM_GET_ID(log->bm, e);
  le->hflag = e->head.hflag;

  if (copy_customdata) {
    bm_log_edge_customdata(log->bm, log, entry, e, le);
  }
}

/* Allocate and initialize a BMLogVert */
static BMLogEdge *bm_log_edge_alloc(BMLog *log, BMEdge *e, bool log_customdata)
{
  BMLogEntry *entry = log->current_entry;
  BMLogEdge *le = BLI_mempool_alloc(entry->pool_edges);
  le->customdata = NULL;

#ifdef DO_LOG_PRINT
  le->msg[0] = 0;
#endif

  bm_log_edge_bmedge_copy(log, entry, le, e, log_customdata);

  return le;
}

/* Allocate and initialize a BMLogFace */
static BMLogFace *bm_log_face_alloc(BMLog *log, BMFace *f)
{
  BMLogEntry *entry = log->current_entry;
  BMLogFace *lf = BLI_mempool_alloc(entry->pool_faces);

  lf->len = (size_t)f->len;

  bool have_loop_ids = (log->bm->idmap.flag & BM_LOOP);

  if (f->len > MAX_FACE_RESERVED) {
    lf->v_ids = (uint *)BLI_memarena_alloc(entry->arena, sizeof(*lf->v_ids) * lf->len);
    lf->l_ids = (uint *)BLI_memarena_alloc(entry->arena, sizeof(*lf->l_ids) * lf->len);
    lf->customdata = (void **)BLI_memarena_alloc(entry->arena, sizeof(void *) * lf->len);
  }
  else {
    lf->v_ids = lf->v_ids_res;
    lf->l_ids = lf->l_ids_res;
    lf->customdata = lf->customdata_res;
  }

  lf->customdata_f = NULL;

  copy_v3_v3(lf->no, f->no);

  int i = 0;
  BMLoop *l = f->l_first;
  do {
    if (have_loop_ids) {
      lf->l_ids[i] = (uint)BM_ELEM_GET_ID(log->bm, l);
    }
    else {
      lf->l_ids[i] = (uint)-1;
    }

    lf->v_ids[i] = bm_log_vert_id_get(log, l->v);

    lf->customdata[i] = NULL;
  } while ((i++, l = l->next) != f->l_first);

  lf->hflag = f->head.hflag;
  return lf;
}

/************************ Helpers for undo/redo ***********************/

// exec vert kill callbacks before killing faces
static void bm_log_verts_unmake_pre(
    BMesh *bm, BMLog *log, GHash *verts, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, verts) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMVert *v = bm_log_vert_from_id(log, id);

    if (!v) {
      printf("bm_log error; vertex id: %p\n", key);
      continue;
    }

    if (v->head.htype != BM_VERT) {
      printf("bm_log error; vertex id: %p, type was: %d\n", key, v->head.htype);
      continue;
    }

    /* Ensure the log has the final values of the vertex before
     * deleting it */
    bm_log_vert_bmvert_copy(log, entry, lv, v, -1, true);

    if (callbacks) {
      callbacks->on_vert_kill(v, callbacks->userdata);
    }
  }
}

// exec vert kill callbacks before killing faces
static void bm_log_edges_unmake_pre(
    BMesh *bm, BMLog *log, GHash *edges, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, edges) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMEdge *e = bm_log_edge_from_id(log, id);

    if (!e) {
      printf("%s: missing edge; id: %d [%s]\n", __func__, id, GET_MSG(le));
      continue;
    }

    if (e->head.htype != BM_EDGE) {
      printf("%s: not an edge; edge id: %d, type was: %d [%s]\n",
             __func__,
             id,
             e->head.htype,
             GET_MSG(le));
      continue;
    }

    /* Ensure the log has the final values of the vertex before
     * deleting it */
    bm_log_edge_bmedge_copy(log, entry, le, e, true);

    if (callbacks) {
      callbacks->on_edge_kill(e, callbacks->userdata);
    }
  }
}

static void bm_log_edges_unmake(
    BMesh *bm, BMLog *log, GHash *edges, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, edges) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    BMEdge *e = bm_log_edge_from_id(log, id);

    if (!e) {
      printf("%s: missing edge; edge id: %d [%s]\n", __func__, id, GET_MSG(le));
      continue;
    }

    if (e->head.htype != BM_EDGE) {
      printf("%s: not an edge; edge id: %d, type: %d [%s]\n",
             __func__,
             id,
             e->head.htype,
             GET_MSG(le));
      continue;
    }

    BM_edge_kill(bm, e);
  }
}

static void bm_log_verts_unmake(
    BMesh *bm, BMLog *log, GHash *verts, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, verts) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMVert *v = bm_log_vert_from_id(log, id);

    if (!v) {
      printf("bmlog error.  vertex id: %p\n", key);
      continue;
    }

    BM_vert_kill(bm, v);
  }
}

static void bm_log_faces_unmake(
    BMesh *bm, BMLog *log, GHash *faces, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  BMEdge **e_tri = NULL;
  BLI_array_staticdeclare(e_tri, 32);

  GHASH_ITER (gh_iter, faces) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMFace *f = bm_log_face_from_id(log, id);

    if (!f) {
      printf("bmlog error in %s: missing face %d\n", __func__, id);
      continue;
    }

    if (f->head.htype != BM_FACE) {
      printf("bmlog error in %s: f was not a face, type was: %d\n", __func__, f->head.htype);
      continue;
    }

    BLI_array_clear(e_tri);

    BMLoop *l;
    int i;

    // ensure we have final customdata for face in log

    l = f->l_first;
    i = 0;
    do {
      if (lf->customdata[i]) {
        CustomData_bmesh_copy_data(&bm->ldata, &entry->ldata, l->head.data, &lf->customdata[i]);
      }

      BLI_array_append(e_tri, l->e);
    } while ((i++, l = l->next) != f->l_first);

    if (lf->customdata_f) {
      CustomData_bmesh_copy_data(&bm->pdata, &entry->pdata, f->head.data, &lf->customdata_f);

      // forcibly copy id
      // bm_log_copy_id(&bm->pdata, (BMElem *)f, lf->customdata_f);
    }

    if (callbacks) {
      callbacks->on_face_kill(f, callbacks->userdata);
    }

    BM_face_kill(bm, f);

#if 0
    /* Remove any unused edges */
    for (i = 0; i < (int)lf->len; i++) {
      if (BM_edge_is_wire(e_tri[i])) {
        BM_edge_kill(bm, e_tri[i]);
      }
    }
#endif
  }

  BLI_array_free(e_tri);
}

static void bm_log_verts_restore(
    BMesh *bm, BMLog *log, GHash *verts, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, verts) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);

    BMVert *v = BM_vert_create(bm, lv->co, NULL, BM_CREATE_SKIP_ID);

    v->head.hflag = lv->hflag;
    copy_v3_v3(v->no, lv->no);

#ifdef CUSTOMDATA
    if (lv->customdata) {
      CustomData_bmesh_copy_data(&entry->vdata, &bm->vdata, lv->customdata, &v->head.data);
    }
#endif

    bm_assign_id(bm, (BMElem *)v, POINTER_AS_UINT(key), false);

    if (callbacks) {
      callbacks->on_vert_add(v, callbacks->userdata);
    }
  }
}

static void bm_log_edges_restore(
    BMesh *bm, BMLog *log, GHash *edges, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, edges) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);

    if (id != le->id) {
      printf("%s: id differs from stored id in BMLogEdge!\n", __func__);
    }

    BMVert *v1 = bm_log_vert_from_id(log, le->v1);
    BMVert *v2 = bm_log_vert_from_id(log, le->v2);

    if (!v1 || !v2) {
      printf("%s: missing edge verts: %p %p\n", __func__, v1, v2);
      continue;
    }

    if (v1->head.htype != BM_VERT || v2->head.htype != BM_VERT) {
      printf("%s: edge verts were not verts: %d %d\n", __func__, v1->head.htype, v2->head.htype);
      continue;
    }

    BMEdge *e = BM_edge_exists(v1, v2);
    if (e) {
      printf("%s: edge already %d existed\n", __func__, (int)id);
      bm_free_id(bm, (BMElem *)e);
    }
    else {
      e = BM_edge_create(bm, v1, v2, NULL, BM_CREATE_SKIP_ID);
    }

    e->head.hflag = le->hflag;

#ifdef CUSTOMDATA
    if (le->customdata) {
      CustomData_bmesh_copy_data(&entry->edata, &bm->edata, le->customdata, &e->head.data);
    }
#endif

    bm_assign_id(bm, (BMElem *)e, POINTER_AS_UINT(key), false);

    if ((uint)BM_ELEM_GET_ID(bm, e) != id) {
      printf("%s: error assigning id\n", __func__);
    }

    if (callbacks) {
      callbacks->on_edge_add(e, callbacks->userdata);
    }
  }
}

static void bm_log_faces_restore(
    BMesh *bm, BMLog *log, GHash *faces, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  BMVert **vs_tmp = NULL;
  BLI_array_staticdeclare(vs_tmp, 32);

  bool have_loop_ids = (log->bm->idmap.flag & BM_LOOP);

  GHASH_ITER (gh_iter, faces) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);

    BLI_array_clear(vs_tmp);
    bool bad = false;

    for (int i = 0; i < (int)lf->len; i++) {
      BMVert *v = bm_log_vert_from_id(log, lf->v_ids[i]);

      if (!v) {
        BMIter iter;
        BMVert *v2;
        const int cd_id = bm->idmap.cd_id_off[BM_VERT];

        bad = true;

        BM_ITER_MESH (v2, &iter, bm, BM_VERTS_OF_MESH) {
          int id = BM_ELEM_CD_GET_INT(v2, cd_id);

          if (lf->v_ids[i] == (uint)id) {
            printf("found vertex %d\n", id);
            bad = false;
            v = v2;
            break;
          }
        }

        if (bad) {
          printf("Undo error! %p\n", v);
          break;
        }
      }

      if (bad) {
        continue;
      }

      if (v->head.htype != BM_VERT) {
        printf("vert %d in face %d was not a vertex\n", (int)lf->v_ids[i], POINTER_AS_INT(key));
        continue;
      }
      BLI_array_append(vs_tmp, v);
    }

    if ((int)BLI_array_len(vs_tmp) < 2) {
      printf("severely malformed face %d in %s\n", POINTER_AS_INT(key), __func__);
      continue;
    }

#if 0
    for (size_t j = 0; j < lf->len; j++) {
      BMVert *v1 = bm_log_vert_from_id(log, lf->v_ids[j]);
      BMVert *v2 = bm_log_vert_from_id(log, lf->v_ids[(j + 1) % lf->len]);

      if (!v1 || !v2) {
        continue;
      }

      if (!BM_edge_exists(v1, v2)) {
        int id = POINTER_AS_INT(key);
        printf("%s: missing edge, face %d had to create it\n", __func__, (int)id);
      }
    }
#endif

    BMFace *f = BM_face_create_verts(
        bm, vs_tmp, (int)BLI_array_len(vs_tmp), NULL, BM_CREATE_SKIP_ID, true);
    f->head.hflag = lf->hflag;

    copy_v3_v3(f->no, lf->no);

    if (lf->customdata_f) {
      CustomData_bmesh_copy_data(&entry->pdata, &bm->pdata, lf->customdata_f, &f->head.data);
    }

    bm_assign_id(bm, (BMElem *)f, POINTER_AS_UINT(key), false);

    BMLoop *l = f->l_first;
    int j = 0;

    do {
      if (have_loop_ids) {
        bm_assign_id(bm, (BMElem *)l, lf->l_ids[j], false);
      }

      if (lf->customdata[j]) {
        CustomData_bmesh_copy_data(&entry->ldata, &bm->ldata, lf->customdata[j], &l->head.data);
      }
    } while ((j++, l = l->next) != f->l_first);

    if (callbacks) {
      callbacks->on_face_add(f, callbacks->userdata);
    }
  }

  BLI_array_free(vs_tmp);
}

static void bm_log_vert_values_swap(
    BMesh *bm, BMLog *log, GHash *verts, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  void *scratch = bm->vdata.pool ? BLI_mempool_alloc(bm->vdata.pool) : NULL;

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, verts) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMVert *v = bm_log_vert_from_id(log, id);

    if (!v) {
      printf("missing vert in bmlog! %d", id);
      continue;
    }

    if (v->head.htype != BM_VERT) {
      printf("not a vertex: %d\n", v->head.htype);
      continue;
    }

    swap_v3_v3(v->co, lv->co);
    swap_v3_v3(v->no, lv->no);

    SWAP(char, v->head.hflag, lv->hflag);

    void *old_cdata = NULL;

    if (lv->customdata) {
      if (v->head.data) {
        old_cdata = scratch;
        memcpy(old_cdata, v->head.data, (size_t)bm->vdata.totsize);
      }

      CustomData_bmesh_swap_data(&entry->vdata, &bm->vdata, lv->customdata, &v->head.data);
    }

    if (callbacks) {
      callbacks->on_vert_change(v, callbacks->userdata, old_cdata);
    }
  }

  if (scratch) {
    BLI_mempool_free(bm->vdata.pool, scratch);
  }
}

static void bm_log_edge_values_swap(
    BMesh *bm, BMLog *log, GHash *edges, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  void *scratch = bm->edata.pool ? BLI_mempool_alloc(bm->edata.pool) : NULL;

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, edges) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMEdge *e = bm_log_edge_from_id(log, id);

    SWAP(char, e->head.hflag, le->hflag);

    void *old_cdata = NULL;

    if (le->customdata) {
      if (e->head.data) {
        old_cdata = scratch;
        memcpy(old_cdata, e->head.data, (size_t)bm->edata.totsize);
      }

      CustomData_bmesh_swap_data(&entry->edata, &bm->edata, le->customdata, &e->head.data);
    }

    if (callbacks) {
      callbacks->on_edge_change(e, callbacks->userdata, old_cdata);
    }
  }

  if (scratch) {
    BLI_mempool_free(bm->edata.pool, scratch);
  }
}

static void bm_log_face_values_swap(BMLog *log,
                                    GHash *faces,
                                    BMLogEntry *entry,
                                    BMLogCallbacks *callbacks)
{
  void *scratch = log->bm->pdata.pool ? BLI_mempool_alloc(log->bm->pdata.pool) : NULL;

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, faces) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMFace *f = bm_log_face_from_id(log, id);

    swap_v3_v3(f->no, lf->no);
    SWAP(char, f->head.hflag, lf->hflag);

    void *old_cdata = NULL;

    if (f->head.data) {
      old_cdata = scratch;
      memcpy(old_cdata, f->head.data, (size_t)log->bm->pdata.totsize);
    }

    if (lf->customdata_f) {
      CustomData_bmesh_swap_data(&entry->pdata, &log->bm->pdata, lf->customdata_f, &f->head.data);
    }

    int i = 0;
    BMLoop *l = f->l_first;

    do {
      if (lf->customdata[i]) {
        CustomData_bmesh_swap_data(
            &entry->ldata, &log->bm->ldata, lf->customdata[i], &l->head.data);
      }
    } while ((i++, l = l->next) != f->l_first);

    if (callbacks) {
      callbacks->on_face_change(f, callbacks->userdata, old_cdata);
    }
  }

  if (scratch) {
    BLI_mempool_free(log->bm->pdata.pool, scratch);
  }
}

/**********************************************************************/

static void bm_log_full_mesh_intern(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  CustomData_MeshMasks cd_mask_extra = {CD_MASK_DYNTOPO_VERT, 0, 0, 0, 0};

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  entry->full_copy_mesh = BKE_mesh_from_bmesh_nomain(
      bm,
      (&(struct BMeshToMeshParams){.update_shapekey_indices = false,
                                   .calc_object_remap = false,
                                   .cd_mask_extra = cd_mask_extra,
                                   .copy_temp_cdlayers = true,
                                   .ignore_mesh_id_layers = false}),
      NULL);
}

/* Allocate an empty log entry */
static BMLogEntry *bm_log_entry_create(BMLogEntryType type)
{
  BMLogEntry *entry = MEM_callocN(sizeof(BMLogEntry), __func__);

  entry->type = type;

  if (type == LOG_ENTRY_PARTIAL) {
    entry->deleted_verts = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->deleted_edges = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->deleted_edges_post = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->deleted_faces = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);

    entry->added_verts = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->added_edges = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->added_faces = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);

    entry->modified_verts = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->modified_edges = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->modified_faces = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);

    entry->pool_verts = BLI_mempool_create(sizeof(BMLogVert), 0, 64, BLI_MEMPOOL_NOP);
    entry->pool_edges = BLI_mempool_create(sizeof(BMLogEdge), 0, 64, BLI_MEMPOOL_NOP);
    entry->pool_faces = BLI_mempool_create(sizeof(BMLogFace), 0, 64, BLI_MEMPOOL_NOP);

    entry->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "bmlog arena");
  }

  return entry;
}

/* Free the data in a log entry
 *
 * NOTE: does not free the log entry itself. */
static void bm_log_entry_free_direct(BMLogEntry *entry)
{
  switch (entry->type) {
    case LOG_ENTRY_MESH_IDS:
      log_idmap_free(entry);
      break;
    case LOG_ENTRY_FULL_MESH:

      BKE_mesh_free_data_for_undo(entry->full_copy_mesh);
      break;
    case LOG_ENTRY_PARTIAL:
      BLI_ghash_free(entry->deleted_verts, NULL, NULL);
      BLI_ghash_free(entry->deleted_edges, NULL, NULL);
      BLI_ghash_free(entry->deleted_edges_post, NULL, NULL);
      BLI_ghash_free(entry->deleted_faces, NULL, NULL);

      BLI_ghash_free(entry->added_verts, NULL, NULL);
      BLI_ghash_free(entry->added_edges, NULL, NULL);
      BLI_ghash_free(entry->added_faces, NULL, NULL);

      BLI_ghash_free(entry->modified_verts, NULL, NULL);
      BLI_ghash_free(entry->modified_edges, NULL, NULL);
      BLI_ghash_free(entry->modified_faces, NULL, NULL);

      BLI_mempool_destroy(entry->pool_verts);
      BLI_mempool_destroy(entry->pool_edges);
      BLI_mempool_destroy(entry->pool_faces);
      BLI_memarena_free(entry->arena);

      /* check for the weird case that a user has dynamic
         topology on with multires data */

      if (CustomData_has_layer(&entry->ldata, CD_MDISPS)) {
        int cd_mdisps = CustomData_get_offset(&entry->ldata, CD_MDISPS);

        /* iterate over cdata blocks directly */
        BLI_mempool_iter iter;
        BLI_mempool_iternew(entry->ldata.pool, &iter);
        void *block = BLI_mempool_iterstep(&iter);

        for (; block; block = BLI_mempool_iterstep(&iter)) {
          BMElem elem;
          elem.head.data = block;

          MDisps *mdisp = BM_ELEM_CD_GET_VOID_P(&elem, cd_mdisps);
          if (mdisp->disps) {
            MEM_freeN(mdisp->disps);
          }
        }
      }

      if (entry->vdata.pool) {
        BLI_mempool_destroy(entry->vdata.pool);
      }
      if (entry->edata.pool) {
        BLI_mempool_destroy(entry->edata.pool);
      }
      if (entry->ldata.pool) {
        BLI_mempool_destroy(entry->ldata.pool);
      }
      if (entry->pdata.pool) {
        BLI_mempool_destroy(entry->pdata.pool);
      }

      CustomData_free(&entry->vdata, 0);
      CustomData_free(&entry->edata, 0);
      CustomData_free(&entry->ldata, 0);
      CustomData_free(&entry->pdata, 0);
      break;
  }
}

/* Free the data in a log entry
 * and handles bmlog ref counting
 * NOTE: does not free the log entry itself. */
static bool bm_log_entry_free(BMLogEntry *entry)
{
  BMLog *log = entry->log;
  bool kill_log = false;

  if (log) {
    namestack_push();
    bm_log_decref(log);
    namestack_pop();

    if (log->refcount < 0) {
      fprintf(stderr, "BMLog refcount error\n");
      log->refcount = 0;
    }

    kill_log = !log->refcount;
  }

  bm_log_entry_free_direct(entry);

  if (kill_log) {
#ifdef PRINT_LOG_REF_COUNTING
    printf("killing log! %p\n", log);
#endif

    bm_log_free_direct(log, true);
  }

  return kill_log;
}

static int uint_compare(const void *a_v, const void *b_v)
{
  const uint *a = a_v;
  const uint *b = b_v;
  return (*a) < (*b);
}

/* Remap IDs to contiguous indices
 *
 * E.g. if the vertex IDs are (4, 1, 10, 3), the mapping will be:
 *    4 -> 2
 *    1 -> 0
 *   10 -> 3
 *    3 -> 1
 */
static GHash *bm_log_compress_ids_to_indices(uint *ids, uint totid)
{
  GHash *map = BLI_ghash_int_new_ex(__func__, totid);
  uint i;

  qsort(ids, totid, sizeof(*ids), uint_compare);

  for (i = 0; i < totid; i++) {
    void *key = POINTER_FROM_UINT(ids[i]);
    void *val = POINTER_FROM_UINT(i);
    BLI_ghash_insert(map, key, val);
  }

  return map;
}

/***************************** Public API *****************************/

void BM_log_set_cd_offsets(BMLog *log, int cd_dyn_vert)
{
  log->cd_dyn_vert = cd_dyn_vert;
}

void BM_log_set_bm(BMesh *bm, BMLog *log)
{
  log->bm = bm;
}

/* Allocate, initialize, and assign a new BMLog */
BMLog *BM_log_create(BMesh *bm, int cd_dyn_vert)
{
  BMLog *log = MEM_callocN(sizeof(*log), __func__);

  BLI_rw_mutex_init(&log->lock);

  BM_log_set_cd_offsets(log, cd_dyn_vert);

  return log;
}

BMLog *bm_log_from_existing_entries_create(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  log->current_entry = entry;

  /* Let BMLog manage the entry list again */
  log->entries.first = log->entries.last = entry;

  while (entry->prev) {
    entry = entry->prev;
    log->entries.first = entry;
  }

  entry = log->entries.last;
  while (entry->next) {
    entry = entry->next;
    log->entries.last = entry;
  }

  namestack_push();

  for (entry = log->entries.first; entry; entry = entry->next) {
    BMLogEntry *entry2 = entry;

    /* go to head of subgroup */
    while (entry2->combined_next) {
      entry2 = entry2->combined_next;
    }

    while (entry2) {
      entry2->log = log;
      entry2 = entry2->combined_prev;

      bm_log_addref(log);
    }
  }

  namestack_pop();

  return log;
}

/* Allocate and initialize a new BMLog using existing BMLogEntries
 *
 * The 'entry' should be the last entry in the BMLog. Its prev pointer
 * will be followed back to find the first entry.
 *
 * The unused IDs field of the log will be initialized by taking all
 * keys from all GHashes in the log entry.
 */
BMLog *BM_log_from_existing_entries_create(BMesh *bm, BMLogEntry *entry)
{
  BMLog *log = BM_log_create(bm, -1);

  bm_log_from_existing_entries_create(bm, log, entry);

  return log;
}

BMLog *BM_log_unfreeze(BMesh *bm, BMLogEntry *entry)
{
  if (!entry || !entry->log) {
    return NULL;
  }

#if 0
  BMLogEntry *frozen = entry->log->frozen_full_mesh;
  if (!frozen && entry->type == LOG_ENTRY_FULL_MESH) {
    frozen = entry;
  }

  if (!frozen || frozen->type != LOG_ENTRY_FULL_MESH) {
    return entry->log->bm == bm ? entry->log : NULL;
  }
#endif

  entry->log->bm = bm;

#if 0
  full_copy_load(bm, entry->log, frozen);

  if (entry->log->frozen_full_mesh) {
    entry->log->frozen_full_mesh->log = NULL;
    bm_log_entry_free(entry->log->frozen_full_mesh);
    entry->log->frozen_full_mesh = NULL;
  }
#endif
  return entry->log;
}

/* Free all the data in a BMLog including the log itself
 * safe_mode means log->refcount will be checked, and if nonzero log will not be freed
 */
static bool bm_log_free_direct(BMLog *log, bool safe_mode)
{
  BMLogEntry *entry;

  if (safe_mode && log->refcount) {
#if 0
    if (log->frozen_full_mesh) {
      log->frozen_full_mesh->log = NULL;
      bm_log_entry_free(log->frozen_full_mesh);
    }
#endif

    // log->frozen_full_mesh = bm_log_entry_create(LOG_ENTRY_FULL_MESH);
    // bm_log_full_mesh_intern(log->bm, log, log->frozen_full_mesh);

    return false;
  }

  log->dead = true;

  BLI_rw_mutex_end(&log->lock);

  /* Clear the BMLog references within each entry, but do not free
   * the entries themselves */
  for (entry = log->entries.first; entry; entry = entry->next) {
    entry->log = NULL;
  }

  return true;
}

// if true, make sure to call BM_log_free on the log
bool BM_log_is_dead(BMLog *log)
{
  return log->dead;
}

bool BM_log_free(BMLog *log, bool safe_mode)
{
  if (log->dead) {
    MEM_freeN(log);
    return true;
  }

  if (bm_log_free_direct(log, safe_mode)) {
    MEM_freeN(log);
    return true;
  }

  return false;
}

/* Get the number of log entries */
int BM_log_length(const BMLog *log)
{
  return BLI_listbase_count(&log->entries);
}

void BM_log_print_entry(BMLog *log, BMLogEntry *entry)
{
  BMLogEntry *first = entry;

  if (!log) {
    log = entry->log;
  }

  while (first->combined_prev) {
    first = first->combined_prev;
  }

  printf("==bmlog step==\n");

  while (first) {
    switch (first->type) {
      case LOG_ENTRY_FULL_MESH:
        printf(" ==full mesh copy==\n");
        break;
      case LOG_ENTRY_MESH_IDS:
        printf("==element IDs snapshot\n");
        break;
      case LOG_ENTRY_PARTIAL:
        printf("==modified: ");
        printf("v: %d ", BLI_ghash_len(first->modified_verts));
        printf("e: %d ", BLI_ghash_len(first->modified_edges));
        printf("f: %d ", BLI_ghash_len(first->modified_faces));
        printf(" new: ");
        printf("v: %d ", BLI_ghash_len(first->added_verts));
        printf("e: %d ", BLI_ghash_len(first->added_edges));
        printf("f: %d ", BLI_ghash_len(first->added_faces));
        printf(" deleted: ");
        printf("v: %d ", BLI_ghash_len(first->deleted_verts));
        printf("e: %d ", BLI_ghash_len(first->deleted_edges));
        printf("pe: %d ", BLI_ghash_len(first->deleted_edges_post));
        printf("f: %d ", BLI_ghash_len(first->deleted_faces));
        printf("\n");
        break;
    }

    first = first->combined_next;
  }
}

/* Apply a consistent ordering to BMesh vertices */
void BM_log_mesh_elems_reorder(BMesh *bm, BMLog *log)
{
#if 0  // TODO: make sure no edge cases relying on this function still exist
  uint *varr;
  uint *farr;

  GHash *id_to_idx;

  BMIter bm_iter;
  BMVert *v;
  BMFace *f;

  uint i;

  /* Put all vertex IDs into an array */
  varr = MEM_mallocN(sizeof(int) * (size_t)bm->totvert, __func__);
  BM_ITER_MESH_INDEX (v, &bm_iter, bm, BM_VERTS_OF_MESH, i) {
    varr[i] = bm_log_vert_id_get(log, v);
  }

  /* Put all face IDs into an array */
  farr = MEM_mallocN(sizeof(int) * (size_t)bm->totface, __func__);
  BM_ITER_MESH_INDEX (f, &bm_iter, bm, BM_FACES_OF_MESH, i) {
    farr[i] = bm_log_face_id_get(log, f);
  }

  /* Create BMVert index remap array */
  id_to_idx = bm_log_compress_ids_to_indices(varr, (uint)bm->totvert);
  BM_ITER_MESH_INDEX (v, &bm_iter, bm, BM_VERTS_OF_MESH, i) {
    const uint id = bm_log_vert_id_get(log, v);
    const void *key = POINTER_FROM_UINT(id);
    const void *val = log_ghash_lookup(log, id_to_idx, key);
    varr[i] = POINTER_AS_UINT(val);
  }
  BLI_ghash_free(id_to_idx, NULL, NULL);

  /* Create BMFace index remap array */
  id_to_idx = bm_log_compress_ids_to_indices(farr, (uint)bm->totface);
  BM_ITER_MESH_INDEX (f, &bm_iter, bm, BM_FACES_OF_MESH, i) {
    const uint id = bm_log_face_id_get(log, f);
    const void *key = POINTER_FROM_UINT(id);
    const void *val = log_ghash_lookup(log, id_to_idx, key);
    farr[i] = POINTER_AS_UINT(val);
  }
  BLI_ghash_free(id_to_idx, NULL, NULL);

  BM_mesh_remap(bm, varr, NULL, farr, NULL);

  MEM_freeN(varr);
  MEM_freeN(farr);
#endif
}

BMLogEntry *BM_log_entry_check_customdata(BMesh *bm, BMLog *log)
{
  BMLogEntry *entry = log->current_entry;

  if (!entry) {
    printf("no current entry; creating...\n");
    fflush(stdout);
    return BM_log_entry_add_ex(bm, log, false);
  }

  if (entry->type != LOG_ENTRY_PARTIAL) {
    return BM_log_entry_add_ex(bm, log, true);
  }

#ifndef CUSTOMDATA
  return entry;
#else

  CustomData *cd1[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};
  CustomData *cd2[4] = {&entry->vdata, &entry->edata, &entry->ldata, &entry->pdata};

  for (int i = 0; i < 4; i++) {
    if (!CustomData_layout_is_same(cd1[i], cd2[i])) {
      printf("Customdata changed for undo\n");
      fflush(stdout);
      return BM_log_entry_add_ex(bm, log, true);
    }
  }

  return entry;
#endif
}

/* Start a new log entry and update the log entry list
 *
 * If the log entry list is empty, or if the current log entry is the
 * last entry, the new entry is simply appended to the end.
 *
 * Otherwise, the new entry is added after the current entry and all
 * following entries are deleted.
 *
 * In either case, the new entry is set as the current log entry.
 */
BMLogEntry *BM_log_entry_add(BMesh *bm, BMLog *log)
{
  return BM_log_entry_add_ex(bm, log, false);
}

BMLogEntry *bm_log_entry_add_ex(
    BMesh *bm, BMLog *log, bool combine_with_last, BMLogEntryType type, BMLogEntry *last_entry)
{
  if (log->dead) {
    fprintf(stderr, "BMLog Error: log is dead\n");
    fflush(stderr);
    return NULL;
  }

  log->bm = bm;

  /* WARNING: this is now handled by the UndoSystem: BKE_UNDOSYS_TYPE_SCULPT
   * freeing here causes unnecessary complications. */
  BMLogEntry *entry;
#if 0
  /* Delete any entries after the current one */
  entry = log->current_entry;
  if (entry) {
    BMLogEntry *next;
    for (entry = entry->next; entry; entry = next) {
      next = entry->next;
      bm_log_entry_free(entry);
      BLI_freelinkN(&log->entries, entry);
    }
  }
#endif

  /* Create and append the new entry */
  entry = bm_log_entry_create(type);

  if (!last_entry || last_entry == log->current_entry) {
    BLI_addtail(&log->entries, entry);
  }

  entry->log = log;

  namestack_push();

  bm_log_addref(log);

  namestack_pop();

  if (combine_with_last) {
    if (!last_entry || last_entry == log->current_entry) {
      if (log->current_entry) {
        log->current_entry->combined_next = entry;
        BLI_remlink(&log->entries, log->current_entry);
      }

      entry->combined_prev = log->current_entry;
    }
    else {
      entry->combined_prev = last_entry;
      last_entry->combined_next = entry;
    }
  }

  if (type == LOG_ENTRY_PARTIAL) {
    CustomData_copy_all_layout(&bm->vdata, &entry->vdata);
    CustomData_copy_all_layout(&bm->edata, &entry->edata);
    CustomData_copy_all_layout(&bm->ldata, &entry->ldata);
    CustomData_copy_all_layout(&bm->pdata, &entry->pdata);

    CustomData_bmesh_init_pool_ex(&entry->vdata, 0, BM_VERT, __func__);
    CustomData_bmesh_init_pool_ex(&entry->edata, 0, BM_EDGE, __func__);
    CustomData_bmesh_init_pool_ex(&entry->ldata, 0, BM_LOOP, __func__);
    CustomData_bmesh_init_pool_ex(&entry->pdata, 0, BM_FACE, __func__);
  }

  log->current_entry = entry;

  return entry;
}

BMLogEntry *BM_log_entry_add_ex(BMesh *bm, BMLog *log, bool combine_with_last)
{
  return bm_log_entry_add_ex(bm, log, combine_with_last, LOG_ENTRY_PARTIAL, NULL);
}

/* Remove an entry from the log
 *
 * Uses entry->log as the log. If the log is NULL, the entry will be
 * free'd but not removed from any list, nor shall its IDs be
 * released.
 *
 * This operation is only valid on the first and last entries in the
 * log. Deleting from the middle will assert.
 */
bool BM_log_entry_drop(BMLogEntry *entry)
{
  BMLog *log = entry->log;

  namestack_push();

  // go to head of entry subgroup
  while (entry->combined_next) {
    entry = entry->combined_next;
  }

  if (!log) {
    /* Unlink */
    BLI_assert(!(entry->prev && entry->next));
    if (entry->prev) {
      entry->prev->next = NULL;
    }
    else if (entry->next) {
      entry->next->prev = NULL;
    }

    BMLogEntry *entry2 = entry->combined_prev;
    while (entry2) {
      BMLogEntry *prev = entry2->combined_prev;

      bm_log_entry_free(entry2);
      MEM_freeN(entry2);

      entry2 = prev;
    }

    namestack_pop();
    bm_log_entry_free(entry);
    MEM_freeN(entry);

    return false;
  }

  if (log && log->current_entry == entry) {
    log->current_entry = entry->prev;
  }

  if (log) {
    BLI_remlink(&log->entries, entry);
  }

  // free subentries first
  BMLogEntry *entry2 = entry->combined_prev;
  while (entry2) {
    BMLogEntry *prev = entry2->combined_prev;

    bm_log_entry_free(entry2);
    MEM_freeN(entry2);
    entry2 = prev;
  }

  bool ret = bm_log_entry_free(entry);

  MEM_freeN(entry);
  namestack_pop();

  return ret;
}

static void full_copy_load(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  CustomData_MeshMasks cd_mask_extra = {CD_MASK_DYNTOPO_VERT, 0, 0, 0, 0};

  BM_mesh_clear(bm);
  BM_mesh_bm_from_me(NULL,
                     bm,
                     entry->full_copy_mesh,
                     (&(struct BMeshFromMeshParams){.calc_face_normal = false,
                                                    .add_key_index = false,
                                                    .use_shapekey = false,
                                                    .active_shapekey = -1,

                                                    .cd_mask_extra = cd_mask_extra,
                                                    .copy_temp_cdlayers = true,
                                                    .ignore_id_layers = false}));

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);
}

static void log_idmap_free(BMLogEntry *entry)
{
  for (int i = 0; i < 4; i++) {
    int type = 1 << i;

    MEM_SAFE_FREE(entry->idmap.maps[type]);
    entry->idmap.maps[type] = NULL;
    entry->idmap.elemtots[type] = 0;
  }
}

static void log_idmap_save(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  log_idmap_free(entry);

  entry->type = LOG_ENTRY_MESH_IDS;
  memset((void *)&entry->idmap, 0, sizeof(entry->idmap));

  entry->idmap.elemmask = BM_VERT | BM_EDGE | BM_FACE;
  BMLogIdMap *idmap = &entry->idmap;

  BMIter iter;

  int cd_id_offs[4] = {CustomData_get_offset(&bm->vdata, CD_MESH_ID),
                       CustomData_get_offset(&bm->edata, CD_MESH_ID),
                       CustomData_get_offset(&bm->ldata, CD_MESH_ID),
                       CustomData_get_offset(&bm->pdata, CD_MESH_ID)};

  const char iters[] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, 0, BM_FACES_OF_MESH};
  int tots[] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};

  // enforce elemmask
  for (int i = 0; i < 4; i++) {
    int type = 1 << i;

    if (!(idmap->elemmask & type) || !tots[i]) {
      tots[i] = 0;
      cd_id_offs[i] = -1;
    }
  }

  // set up loop map which is handled specially
  if (cd_id_offs[2] >= 0 && tots[2] > 0) {
    idmap->maps[BM_LOOP] = MEM_malloc_arrayN((size_t)tots[2], sizeof(int), "idmap->maps[BM_LOOP]");
  }

  for (int i = 0; i < 4; i++) {
    if (i == 2) {  // loops are saved in face pass
      continue;
    }

    int type = 1 << i;
    const int cd_off = cd_id_offs[i];
    const int tot = tots[i];

    idmap->elemtots[type] = tot;

    if (cd_off < 0 || tot == 0) {
      continue;
    }

    int *map = idmap->maps[type] = MEM_malloc_arrayN(
        (size_t)tot, sizeof(int), "idmap->maps entry");

    BMElem *elem;
    int j = 0;
    int loopi = 0;
    int cd_loop_off = cd_id_offs[2];
    int *lmap = idmap->maps[2];

    bool reported = false;

    BM_ITER_MESH_INDEX (elem, &iter, bm, iters[i], j) {
      int id = BM_ELEM_CD_GET_INT(elem, cd_off);

      if (!reported && (BMElem *)BM_ELEM_FROM_ID(bm, id) != elem) {
        printf("IDMap error for elem type %d\n", elem->head.htype);
        printf("  further errors suppressed\n");
        reported = true;
      }

      map[j] = id;

      // deal with loops
      if (type == BM_FACE && cd_loop_off >= 0 && lmap) {
        BMFace *f = (BMFace *)elem;
        BMLoop *l = f->l_first;

        do {
          lmap[loopi++] = BM_ELEM_CD_GET_INT(l, cd_loop_off);
        } while ((l = l->next) != f->l_first);
      }
    }

    if (type == BM_FACE) {
      idmap->elemtots[BM_LOOP] = loopi;
    }
  }
}

static void log_idmap_load(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  const int cd_id_offs[4] = {CustomData_get_offset(&bm->vdata, CD_MESH_ID),
                             CustomData_get_offset(&bm->edata, CD_MESH_ID),
                             CustomData_get_offset(&bm->ldata, CD_MESH_ID),
                             CustomData_get_offset(&bm->pdata, CD_MESH_ID)};

  const char iters[] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, 0, BM_FACES_OF_MESH};
  const int tots[] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};
  BMLogIdMap *idmap = &entry->idmap;

  BM_clear_ids(bm);

  for (int i = 0; i < 4; i++) {
    int type = 1 << i;

    if (!(idmap->elemmask & type) || i == 2) {
      continue;
    }

    if (cd_id_offs[i] < 0) {
      printf("mesh doesn't have ids for elem type %d\n", type);
      continue;
    }

    if (idmap->elemtots[type] != tots[i]) {
      printf("idmap elem count mismatch error");
      continue;
    }

    if (!idmap->elemtots[type]) {
      continue;
    }

    const int cd_loop_id = (idmap->elemmask & type) ? cd_id_offs[2] : -1;

    int j = 0;
    BMElem *elem;
    BMIter iter;
    int *map = idmap->maps[type];
    int loopi = 0;
    int *lmap = idmap->maps[BM_LOOP];

    BM_ITER_MESH_INDEX (elem, &iter, bm, iters[i], j) {
      bm_assign_id(bm, elem, (uint)map[j], false);

      // deal with loops
      if (type == BM_FACE && cd_loop_id >= 0) {
        BMFace *f = (BMFace *)elem;
        BMLoop *l = f->l_first;

        do {
          bm_assign_id(bm, (BMElem *)l, (uint)lmap[loopi], false);

          loopi++;
        } while ((l = l->next) != f->l_first);
      }
    }
  }
}

static void log_idmap_swap(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  const int cd_id_offs[4] = {CustomData_get_offset(&bm->vdata, CD_MESH_ID),
                             CustomData_get_offset(&bm->edata, CD_MESH_ID),
                             CustomData_get_offset(&bm->ldata, CD_MESH_ID),
                             CustomData_get_offset(&bm->pdata, CD_MESH_ID)};

  const char iters[] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, 0, BM_FACES_OF_MESH};
  const int tots[] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};
  BMLogIdMap *idmap = &entry->idmap;

  BM_clear_ids(bm);

  for (int i = 0; i < 4; i++) {
    int type = 1 << i;

    if (!(idmap->elemmask & type) || i == 2) {
      continue;
    }

    if (cd_id_offs[i] < 0) {
      printf("mesh doesn't have ids for elem type %d\n", type);
      continue;
    }

    if (idmap->elemtots[type] != tots[i]) {
      printf("idmap elem count mismatch error");
      continue;
    }

    if (!idmap->elemtots[type]) {
      continue;
    }

    const int cd_loop_id = (idmap->elemmask & type) ? cd_id_offs[2] : -1;

    int cd_id = cd_id_offs[i];
    int j = 0;
    BMElem *elem;
    BMIter iter;
    int *map = idmap->maps[type];
    int loopi = 0;
    int *lmap = idmap->maps[BM_LOOP];

    BM_ITER_MESH_INDEX (elem, &iter, bm, iters[i], j) {
      int id = BM_ELEM_CD_GET_INT(elem, cd_id);

      bm_assign_id(bm, elem, (uint)map[j], false);
      map[j] = id;

      // deal with loops
      if (type == BM_FACE && cd_loop_id >= 0) {
        BMFace *f = (BMFace *)elem;
        BMLoop *l = f->l_first;

        do {
          int id2 = BM_ELEM_CD_GET_INT(l, cd_loop_id);

          bm_assign_id(bm, (BMElem *)l, (uint)lmap[loopi], false);
          lmap[loopi] = id2;

          loopi++;
        } while ((l = l->next) != f->l_first);
      }
    }
  }
}

void BM_log_set_current_entry(BMLog *log, BMLogEntry *entry)
{
  // you cannot set the current entry to a sub-entry, so this should never happen.
  while (entry && entry->combined_next) {
    entry = entry->combined_next;
  }

  log->current_entry = entry;
}

BMLogEntry *BM_log_all_ids(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  if (!entry) {
    entry = bm_log_entry_add_ex(bm, log, false, LOG_ENTRY_MESH_IDS, NULL);
  }
  else if (entry->type != LOG_ENTRY_MESH_IDS) {
    entry = bm_log_entry_add_ex(bm, log, true, LOG_ENTRY_MESH_IDS, entry);
  }

  if (!entry) {
    // log was dead
    return NULL;
  }

  log_idmap_save(bm, log, entry);
  return entry;
}

static void full_copy_swap(BMesh *bm, BMLog *log, BMLogEntry *entry)
{
  CustomData_MeshMasks cd_mask_extra = {CD_MASK_DYNTOPO_VERT, 0, 0, 0, 0};

  BMLogEntry tmp = {0};

  bm_log_full_mesh_intern(bm, log, &tmp);

  BM_mesh_clear(bm);
  BM_mesh_bm_from_me(NULL,
                     bm,
                     entry->full_copy_mesh,
                     (&(struct BMeshFromMeshParams){.calc_face_normal = false,
                                                    .add_key_index = false,
                                                    .use_shapekey = false,
                                                    .active_shapekey = -1,

                                                    .cd_mask_extra = cd_mask_extra,
                                                    .copy_temp_cdlayers = true,
                                                    .ignore_id_layers = false}));

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  BKE_mesh_free_data_for_undo(entry->full_copy_mesh);

  entry->full_copy_mesh = tmp.full_copy_mesh;
}

/* Undo one BMLogEntry
 *
 * Has no effect if there's nothing left to undo */
static void bm_log_undo_intern(
    BMesh *bm, BMLog *log, BMLogEntry *entry, BMLogCallbacks *callbacks, const char *node_layer_id)
{
  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  if (entry->type == LOG_ENTRY_FULL_MESH) {
    full_copy_swap(bm, log, entry);

    if (callbacks) {
      callbacks->on_full_mesh_load(callbacks->userdata);
    }
    return;
  }
  else if (entry->type == LOG_ENTRY_MESH_IDS) {
    log_idmap_load(bm, log, entry);

    if (callbacks && callbacks->on_mesh_id_restore) {
      callbacks->on_mesh_id_restore(callbacks->userdata);
    }
    return;
  }

  bm_log_edges_restore(bm, log, entry->deleted_edges_post, entry, callbacks);

  /* Delete added faces and verts */
  bm_log_edges_unmake_pre(bm, log, entry->added_edges, entry, callbacks);
  bm_log_verts_unmake_pre(bm, log, entry->added_verts, entry, callbacks);

  bm_log_faces_unmake(bm, log, entry->added_faces, entry, callbacks);
  bm_log_edges_unmake(bm, log, entry->added_edges, entry, callbacks);
  bm_log_verts_unmake(bm, log, entry->added_verts, entry, callbacks);

  /* Restore deleted verts and faces */
  bm_log_verts_restore(bm, log, entry->deleted_verts, entry, callbacks);
  bm_log_edges_restore(bm, log, entry->deleted_edges, entry, callbacks);
  bm_log_faces_restore(bm, log, entry->deleted_faces, entry, callbacks);

  /* Restore vertex coordinates, mask, and hflag */
  bm_log_vert_values_swap(bm, log, entry->modified_verts, entry, callbacks);
  bm_log_edge_values_swap(bm, log, entry->modified_edges, entry, callbacks);
  bm_log_face_values_swap(log, entry->modified_faces, entry, callbacks);
}

void BM_log_undo_skip(BMesh *bm, BMLog *log)
{
  if (log->current_entry) {
    log->current_entry = log->current_entry->prev;
  }
}

void BM_log_redo_skip(BMesh *bm, BMLog *log)
{
  if (log->current_entry) {
    log->current_entry = log->current_entry->next;
  }
  else {
    log->current_entry = log->entries.first;
  }
}

void BM_log_undo_single(BMesh *bm,
                        BMLog *log,
                        BMLogCallbacks *callbacks,
                        const char *node_layer_id)
{
  BMLogEntry *entry = log->current_entry;
  log->bm = bm;

  if (!entry) {
    return;
  }

  BMLogEntry *preventry = entry->prev;

  bm_log_undo_intern(bm, log, entry, callbacks, node_layer_id);
  entry = entry->combined_prev;

  log->current_entry = entry ? entry : preventry;
}

void BM_log_undo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks, const char *node_layer_id)
{
  BMLogEntry *entry = log->current_entry;
  log->bm = bm;

  if (!entry) {
    return;
  }

  BMLogEntry *preventry = entry->prev;

  while (entry) {
    bm_log_undo_intern(bm, log, entry, callbacks, node_layer_id);
    entry = entry->combined_prev;
  }

  log->current_entry = preventry;
}

/* Redo one BMLogEntry
 *
 * Has no effect if there's nothing left to redo */
static void bm_log_redo_intern(
    BMesh *bm, BMLog *log, BMLogEntry *entry, BMLogCallbacks *callbacks, const char *node_layer_id)
{
  if (entry->type == LOG_ENTRY_FULL_MESH) {
    // hrm, should we swap?
    full_copy_swap(bm, log, entry);

    if (callbacks) {
      callbacks->on_full_mesh_load(callbacks->userdata);
    }

    return;
  }
  else if (entry->type == LOG_ENTRY_MESH_IDS) {
    log_idmap_load(bm, log, entry);

    if (callbacks && callbacks->on_mesh_id_restore) {
      callbacks->on_mesh_id_restore(callbacks->userdata);
    }
    return;
  }

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  /* Re-delete previously deleted faces and verts */
  bm_log_edges_unmake_pre(bm, log, entry->deleted_edges, entry, callbacks);
  bm_log_verts_unmake_pre(bm, log, entry->deleted_verts, entry, callbacks);

  bm_log_faces_unmake(bm, log, entry->deleted_faces, entry, callbacks);
  bm_log_edges_unmake(bm, log, entry->deleted_edges, entry, callbacks);
  bm_log_verts_unmake(bm, log, entry->deleted_verts, entry, callbacks);

  /* Restore previously added verts and faces */
  bm_log_verts_restore(bm, log, entry->added_verts, entry, callbacks);
  bm_log_edges_restore(bm, log, entry->added_edges, entry, callbacks);
  bm_log_faces_restore(bm, log, entry->added_faces, entry, callbacks);

  bm_log_edges_unmake(bm, log, entry->deleted_edges_post, entry, callbacks);

  /* Restore vertex coordinates, mask, and hflag */
  bm_log_vert_values_swap(bm, log, entry->modified_verts, entry, callbacks);
  bm_log_edge_values_swap(bm, log, entry->modified_edges, entry, callbacks);
  bm_log_face_values_swap(log, entry->modified_faces, entry, callbacks);
}

BMLogEntry *BM_log_entry_prev(BMLogEntry *entry)
{
  return entry->prev;
}

BMLogEntry *BM_log_entry_next(BMLogEntry *entry)
{
  return entry->next;
}

void BM_log_redo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks, const char *node_layer_id)
{
  BMLogEntry *entry = log->current_entry;
  log->bm = bm;

  if (!entry) {
    /* Currently at the beginning of the undo stack, move to first entry */
    entry = log->entries.first;
  }
  else if (entry->next) {
    /* Move to next undo entry */
    entry = entry->next;
  }

  if (!entry) {
    /* Currently at the end of the undo stack, nothing left to redo */
    return;
  }

  BMLogEntry *nextentry = entry;

  while (entry->combined_prev) {
    entry = entry->combined_prev;
  }

  while (entry) {
    bm_log_redo_intern(bm, log, entry, callbacks, node_layer_id);
    entry = entry->combined_next;
  }

  log->current_entry = nextentry;
}

/* Log a vertex before it is modified
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
void BM_log_vert_before_modified(BMLog *log,
                                 BMVert *v,
                                 const int cd_vert_mask_offset,
                                 bool log_customdata)
{
  BMLogEntry *entry = log->current_entry;
  BMLogVert *lv;
  uint v_id = (uint)BM_ELEM_GET_ID(log->bm, v);
  void *key = POINTER_FROM_UINT(v_id);
  void **val_p;

  // LOGPRINT("key %d\n", (int)key);

  /* Find or create the BMLogVert entry */
  if ((lv = log_ghash_lookup(log, entry->added_verts, key))) {
    bm_log_vert_bmvert_copy(log, entry, lv, v, -1, log_customdata);
  }
  else if (!log_ghash_ensure_p(log, entry->modified_verts, key, &val_p)) {
    lv = bm_log_vert_alloc(log, v, -1, true);
    *val_p = lv;
  }
}

void BM_log_edge_before_modified(BMLog *log, BMEdge *e, bool log_customdata)
{
  BMLogEntry *entry = log->current_entry;
  BMLogEdge *le;
  uint e_id = (uint)BM_ELEM_GET_ID(log->bm, e);
  void *key = POINTER_FROM_UINT(e_id);
  void **val_p;

  /* Find or create the BMLogVert entry */
  if ((le = log_ghash_lookup(log, entry->added_edges, key))) {
    bm_log_edge_bmedge_copy(log, entry, le, e, log_customdata);
  }
  else if (!log_ghash_ensure_p(log, entry->modified_edges, key, &val_p)) {
    le = bm_log_edge_alloc(log, e, true);
    *val_p = le;
  }
}

/* Log a new edge as added to the BMesh
 */
void BM_log_edge_added(BMLog *log, BMEdge *e)
{
  // return;  // XXX
  BMLogEdge *le;
  uint e_id = (uint)BM_ELEM_GET_ID(log->bm, e);
  void *key = POINTER_FROM_UINT(e_id);
  void **val = NULL;

  LOGPRINT("key %d\n", (int)key);

  le = bm_log_edge_alloc(log, e, true);
  SET_MSG(le);

  if (BLI_ghash_ensure_p(log->current_entry->added_edges, key, &val)) {
    BLI_mempool_free(log->current_entry->pool_edges, *val);
  }

  *val = le;
}

/* Log a new vertex as added to the BMesh
 */
void BM_log_vert_added(BMLog *log, BMVert *v, const int cd_vert_mask_offset)
{
  BMLogVert *lv;
  uint v_id = (uint)BM_ELEM_GET_ID(log->bm, v);
  void *key = POINTER_FROM_UINT(v_id);

  LOGPRINT("key %d\n", (int)key);

  lv = bm_log_vert_alloc(log, v, -1, true);
  log_ghash_insert(log, log->current_entry->added_verts, key, lv);
}

/* Log a face before it is modified
 *
 * We always assume face has been added before
 */
void BM_log_face_modified(BMLog *log, BMFace *f)
{
  BMLogFace *lf;
  uint f_id = (uint)BM_ELEM_GET_ID(log->bm, f);
  void *key = POINTER_FROM_UINT(f_id);

  // LOGPRINT("key %d\n", (int)key);

  lf = bm_log_face_alloc(log, f);

  log_ghash_insert(log, log->current_entry->modified_faces, key, lf);
  bm_log_face_customdata(log->bm, log, f, lf);
}

/* Log a new face as added to the BMesh
 *
 * The new face gets a unique ID assigned. It is then added to a map
 * of added faces, with the key being its ID and the value containing
 * everything needed to reconstruct that face.
 */
void BM_log_face_added(BMLog *log, BMFace *f)
{
  BMLogFace *lf;
  uint f_id = (uint)BM_ELEM_GET_ID(log->bm, f);
  void *key = POINTER_FROM_UINT(f_id);

  LOGPRINT("key %d\n", (int)key);

  lf = bm_log_face_alloc(log, f);
  log_ghash_insert(log, log->current_entry->added_faces, key, lf);

  bm_log_face_customdata(log->bm, log, f, lf);
}

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
void BM_log_vert_removed(BMLog *log, BMVert *v, const int cd_vert_mask_offset)
{
  BMLogEntry *entry = log->current_entry;
  uint v_id = (uint)BM_ELEM_GET_ID(log->bm, v);
  void *key = POINTER_FROM_UINT(v_id);

  LOGPRINT("key %d\n", (int)key);

  if (!log_ghash_remove(log, entry->added_verts, key, NULL, NULL)) {
    BMLogVert *lv, *lv_mod;

    lv = bm_log_vert_alloc(log, v, -1, false);
    log_ghash_insert(log, entry->deleted_verts, key, lv);

    /* If the vertex was modified before deletion, ensure that the
     * original vertex values are stored */
    if ((lv_mod = log_ghash_lookup(log, entry->modified_verts, key))) {
      if (lv->customdata) {
        BLI_mempool_free(entry->vdata.pool, lv->customdata);
      }

      (*lv) = (*lv_mod);
      lv_mod->customdata = NULL;

      log_ghash_remove(log, entry->modified_verts, key, NULL, NULL);
      BLI_mempool_free(entry->pool_verts, lv_mod);
    }
    else {
      bm_log_vert_customdata(log->bm, log, entry, v, lv);
    }
  }
}

void BM_log_edge_removed_post(BMLog *log, BMEdge *e)
{
  BMLogEntry *entry = log->current_entry;
  uint e_id = (uint)BM_ELEM_GET_ID(log->bm, e);
  void *key = POINTER_FROM_UINT(e_id);

  LOGPRINT("key %d\n", (int)key);

  if (1) {  //! log_ghash_remove(log, entry->added_edges, key, NULL, NULL)) {
    BMLogEdge *le, *le_mod;
    void **val;

    le = bm_log_edge_alloc(log, e, false);
    SET_MSG(le);

    if (BLI_ghash_ensure_p(entry->deleted_edges_post, key, &val)) {
      BLI_mempool_free(entry->pool_edges, *val);
    }

    *val = (void *)le;

#if 1
    /* If the vertex was modified before deletion, ensure that the
     * original edge values are stored */
    if ((le_mod = log_ghash_lookup(log, entry->modified_edges, key))) {
      if (le->customdata) {
        BLI_mempool_free(entry->edata.pool, le->customdata);
      }

      (*le) = (*le_mod);
      le_mod->customdata = NULL;

      SET_MSG(le);

      log_ghash_remove(log, entry->modified_edges, key, NULL, NULL);
      BLI_mempool_free(entry->pool_edges, le_mod);
    }
    else {
      bm_log_edge_customdata(log->bm, log, entry, e, le);
    }
#else
    bm_log_edge_customdata(log->bm, log, entry, e, le);
#endif
  }
}

/**
Splits e and logs the new edge and vertex.
e is assigned a new ID.
*/
BMVert *BM_log_edge_split_do(BMLog *log, BMEdge *e, BMVert *v, BMEdge **newe, float t)
{
#if 0
  BMVert *newv = BM_edge_split(log->bm, e, v, newe, t);
  BM_log_vert_added(log, newv, -1);

  return newv;

#else
  BMEdge *tmp = NULL;
  if (!newe) {
    newe = &tmp;
  }

  BMesh *bm = log->bm;

  int eid0 = BM_ELEM_GET_ID(bm, e);

  bm_log_message("edge split");
  bm_log_message(" esplit: remove edge %d", eid0);
  BM_log_edge_removed(log, e);

  BMVert *v1 = e->v1, *v2 = e->v2;
  uint id1 = (uint)BM_ELEM_GET_ID(bm, v1);
  uint id2 = (uint)BM_ELEM_GET_ID(bm, v2);

  bm_log_message(" esplit: split edge %d (v1=%d v2=%d)", eid0, id1, id2);
  BMVert *newv = BM_edge_split(log->bm, e, v, newe, t);

  uint id3 = (uint)BM_ELEM_GET_ID(bm, newv);
  uint nid = (uint)BM_ELEM_GET_ID(bm, (*newe));

  // get a new id
#  ifndef WITH_BM_ID_FREELIST
  uint id = range_tree_uint_take_any(log->bm->idmap.idtree);
  bm_free_id(log->bm, (BMElem *)e);
  bm_assign_id(log->bm, (BMElem *)e, id, false);
#  else
  bm_free_id(log->bm, (BMElem *)e);
  bm_alloc_id(log->bm, (BMElem *)e);

  uint id = BM_ELEM_GET_ID(bm, e);
#  endif

  bm_log_message(" esplit: add new vert %d", id3);
  BM_log_vert_added(log, newv, -1);

  bm_log_message(" esplit: add old edge (with new id %d)", id);
  BM_log_edge_added(log, e);

  bm_log_message(" esplit: add new edge %d", nid);
  BM_log_edge_added(log, *newe);

  return newv;
#endif
}

void BM_log_edge_removed(BMLog *log, BMEdge *e)
{
  if (e->head.htype != BM_EDGE) {
    printf("%s: e is not an edge; htype: %d\n", __func__, (int)e->head.htype);
    return;
  }

  // return;  // XXX
  BMLogEntry *entry = log->current_entry;
  uint e_id = (uint)BM_ELEM_GET_ID(log->bm, e);
  void *key = POINTER_FROM_UINT(e_id);

  LOGPRINT("key %d\n", (int)key);

  if (!log_ghash_remove(log, entry->added_edges, key, NULL, NULL)) {
    BMLogEdge *le, *le_mod;
    void **val;

    le = bm_log_edge_alloc(log, e, false);
    SET_MSG(le);

    if (BLI_ghash_ensure_p(entry->deleted_edges, key, &val)) {
      BLI_mempool_free(entry->pool_edges, *val);
    }

    *val = (void *)le;

#if 1
    /* If the edge was modified before deletion, ensure that the
     * original edge values are stored */
    if ((le_mod = log_ghash_lookup(log, entry->modified_edges, key))) {
      if (le->customdata) {
        BLI_mempool_free(entry->edata.pool, le->customdata);
      }

      (*le) = (*le_mod);
      le_mod->customdata = NULL;
      SET_MSG(le);

      log_ghash_remove(log, entry->modified_edges, key, NULL, NULL);
      BLI_mempool_free(entry->pool_edges, le_mod);
    }
    else {
      bm_log_edge_customdata(log->bm, log, entry, e, le);
    }
#else
    bm_log_edge_customdata(log->bm, log, entry, e, le);
#endif
  }
}

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
void BM_log_face_removed(BMLog *log, BMFace *f)
{
  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)BM_ELEM_GET_ID(log->bm, f);
  void *key = POINTER_FROM_UINT(f_id);

  LOGPRINT("key %d\n", (int)key);

  /* if it has a key, it shouldn't be NULL */
  BLI_assert(!!log_ghash_lookup(log, entry->added_faces, key) ==
             !!log_ghash_haskey(log, entry->added_faces, key));

  if (!log_ghash_remove(log, entry->added_faces, key, NULL, NULL)) {
    BMLogFace *lf = bm_log_face_alloc(log, f);

    void **val;
    if (BLI_ghash_ensure_p(entry->deleted_faces, key, &val)) {
      BMLogFace *lf2 = (BMLogFace *)*val;

      if (lf2->customdata_f) {
        // BLI_mempool_free(entry->pdata.pool, lf2->customdata_f);
        CustomData_bmesh_free_block(&entry->pdata, &lf2->customdata_f);
      }

      for (uint i = 0; i < lf2->len; i++) {
        if (lf2->customdata[i]) {
          CustomData_bmesh_free_block(&entry->ldata, &lf2->customdata[i]);
        }
      }

      BLI_mempool_free(entry->pool_faces, (void *)lf2);
    }

    if (lf) {
      bm_log_face_customdata(log->bm, log, f, lf);
    }

    *val = lf;
  }
}

/* Log all vertices/faces in the BMesh as added */
void BM_log_all_added(BMesh *bm, BMLog *log)
{
  if (!log->current_entry) {
    BM_log_entry_add_ex(bm, log, false);
  }

  BMIter bm_iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  /* avoid unnecessary resizing on initialization */
  if (BLI_ghash_len(log->current_entry->added_verts) == 0) {
    BLI_ghash_reserve(log->current_entry->added_verts, (uint)bm->totvert);
  }

  if (BLI_ghash_len(log->current_entry->added_faces) == 0) {
    BLI_ghash_reserve(log->current_entry->added_faces, (uint)bm->totface);
  }

  /* Log all vertices as newly created */
  BM_ITER_MESH (v, &bm_iter, bm, BM_VERTS_OF_MESH) {
    BM_log_vert_added(log, v, -1);
  }

  /* Log all edges as newly created */
  BM_ITER_MESH (e, &bm_iter, bm, BM_EDGES_OF_MESH) {
    BM_log_edge_added(log, e);
  }

  /* Log all faces as newly created */
  BM_ITER_MESH (f, &bm_iter, bm, BM_FACES_OF_MESH) {
    BM_log_face_added(log, f);
  }
}

void BM_log_full_mesh(BMesh *bm, BMLog *log)
{
  BMLogEntry *entry = log->current_entry;

  if (!entry) {
    entry = bm_log_entry_add_ex(bm, log, false, LOG_ENTRY_FULL_MESH, NULL);
  }

  // add an entry if current entry isn't empty or isn't LOG_ENTRY_PARTIAL
  bool add = false;

  if (entry->type == LOG_ENTRY_PARTIAL) {
    add = BLI_ghash_len(entry->added_faces) > 0;
    add |= BLI_ghash_len(entry->modified_verts) > 0;
    add |= BLI_ghash_len(entry->modified_faces) > 0;
    add |= BLI_ghash_len(entry->deleted_verts) > 0;
    add |= BLI_ghash_len(entry->deleted_faces) > 0;
  }
  else {
    add = true;
  }

  if (add) {
    entry = bm_log_entry_add_ex(bm, log, true, LOG_ENTRY_FULL_MESH, NULL);
  }
  else {
    bm_log_entry_free_direct(entry);
    entry->type = LOG_ENTRY_FULL_MESH;
  }

  bm_log_full_mesh_intern(bm, log, entry);

  // push a fresh entry
  BM_log_entry_add_ex(bm, log, true);
}

/* Log all vertices/faces in the BMesh as removed */
void BM_log_before_all_removed(BMesh *bm, BMLog *log)
{
  if (!log->current_entry) {
    BM_log_entry_add_ex(bm, log, false);
  }

  BMIter bm_iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  /* Log deletion of all faces */
  BM_ITER_MESH (f, &bm_iter, bm, BM_FACES_OF_MESH) {
    BM_log_face_removed(log, f);
  }

  BM_ITER_MESH (e, &bm_iter, bm, BM_EDGES_OF_MESH) {
    BM_log_edge_removed(log, e);
  }

  /* Log deletion of all vertices */
  BM_ITER_MESH (v, &bm_iter, bm, BM_VERTS_OF_MESH) {
    BM_log_vert_removed(log, v, -1);
  }
}

/* Get the logged coordinates of a vertex
 *
 * Does not modify the log or the vertex */
const float *BM_log_original_vert_co(BMLog *log, BMVert *v)
{
  BMLogEntry *entry = log->current_entry;
  const BMLogVert *lv;
  uint v_id = (uint)BM_ELEM_GET_ID(log->bm, v);
  void *key = POINTER_FROM_UINT(v_id);

  BLI_assert(entry);

  BLI_assert(log_ghash_haskey(log, entry->modified_verts, key));

  lv = log_ghash_lookup(log, entry->modified_verts, key);
  return lv->co;
}

/* Get the logged normal of a vertex
 *
 * Does not modify the log or the vertex */
const float *BM_log_original_vert_no(BMLog *log, BMVert *v)
{
  BMLogEntry *entry = log->current_entry;
  const BMLogVert *lv;
  uint v_id = (uint)BM_ELEM_GET_ID(log->bm, v);
  void *key = POINTER_FROM_UINT(v_id);

  BLI_assert(entry);

  BLI_assert(log_ghash_haskey(log, entry->modified_verts, key));

  lv = log_ghash_lookup(log, entry->modified_verts, key);
  return lv->no;
}

/* DEPRECATED
 * Get the logged mask of a vertex
 *
 * Does not modify the log or the vertex */
float BM_log_original_mask(BMLog *log, BMVert *v)
{
  MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, log->cd_dyn_vert);
  return mv->origmask;
}

void BM_log_original_vert_data(BMLog *log, BMVert *v, const float **r_co, const float **r_no)
{
  BMLogEntry *entry = log->current_entry;
  const BMLogVert *lv;
  uint v_id = (uint)BM_ELEM_GET_ID(log->bm, v);
  void *key = POINTER_FROM_UINT(v_id);

  BLI_assert(entry);

  BLI_assert(log_ghash_haskey(log, entry->modified_verts, key));

  lv = log_ghash_lookup(log, entry->modified_verts, key);
  *r_co = lv->co;
  *r_no = lv->no;
}

/************************ Debugging and Testing ***********************/

/* For internal use only (unit testing) */
BMLogEntry *BM_log_current_entry(BMLog *log)
{
  return log->current_entry;
}

#if 0
/* Print the list of entries, marking the current one
 *
 * Keep around for debugging */
void bm_log_print(const BMLog *log, const char *description)
{
  const BMLogEntry *entry;
  const char *current = " <-- current";
  int i;

  printf("%s:\n", description);
  printf("    % 2d: [ initial ]%s\n", 0, (!log->current_entry) ? current : "");
  for (entry = log->entries.first, i = 1; entry; entry = entry->next, i++) {
    printf("    % 2d: [%p]%s\n", i, entry, (entry == log->current_entry) ? current : "");
  }
}
#endif

static int bmlog_entry_memsize(BMLogEntry *entry)
{
  int ret = 0;

  if (entry->type == LOG_ENTRY_PARTIAL) {
    ret += (int)BLI_mempool_get_size(entry->pool_verts);
    ret += (int)BLI_mempool_get_size(entry->pool_edges);
    ret += (int)BLI_mempool_get_size(entry->pool_faces);
    ret += entry->vdata.pool ? (int)BLI_mempool_get_size(entry->vdata.pool) : 0;
    ret += entry->edata.pool ? (int)BLI_mempool_get_size(entry->edata.pool) : 0;
    ret += entry->ldata.pool ? (int)BLI_mempool_get_size(entry->ldata.pool) : 0;
    ret += entry->pdata.pool ? (int)BLI_mempool_get_size(entry->pdata.pool) : 0;

    // estimate ghash memory usage
    ret += (int)BLI_ghash_len(entry->added_verts) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->added_edges) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->added_faces) * (int)sizeof(void *) * 4;

    ret += (int)BLI_ghash_len(entry->modified_verts) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->modified_edges) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->modified_faces) * (int)sizeof(void *) * 4;

    ret += (int)BLI_ghash_len(entry->deleted_verts) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->deleted_edges) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->deleted_faces) * (int)sizeof(void *) * 4;
  }
  else if (entry->type == LOG_ENTRY_FULL_MESH) {
    Mesh *me = entry->full_copy_mesh;

    ret += me->totvert * me->vdata.totsize;
    ret += me->totedge * me->edata.totsize;
    ret += me->totloop * me->ldata.totsize;
    ret += me->totpoly * me->pdata.totsize;
  }

  return ret;
}

int BM_log_entry_size(BMLogEntry *entry)
{
  while (entry->combined_prev) {
    entry = entry->combined_prev;
  }

  int ret = 0;

  while (entry) {
    ret += bmlog_entry_memsize(entry);

    entry = entry->combined_next;
  }

  return ret;
}
