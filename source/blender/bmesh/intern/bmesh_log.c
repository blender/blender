/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * The BMLog is an interface for storing undo/redo steps as a BMesh is
 * modified. It only stores changes to the BMesh, not full copies.
 *
 * Currently it supports the following types of changes:
 *
 * - Adding and removing vertices
 * - Adding and removing facels
 * - Moving vertices
 * - Setting vertex paint-mask values
 * - Setting vertex hflags
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_compiler_attrs.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_smallhash.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"

//#include "BLI_strict_flags.h"
#include "bmesh.h"
#include "bmesh_idmap.h"
#include "bmesh_log_intern.h"
#include "bmesh_private.h"

#include "range_tree.h"

//#define BM_VALIDATE_LOG

#ifdef BM_VALIDATE_LOG
#  define VALIDATE_LOG(bm, emtry) BM_log_validate(bm, entry, false)
#else
#  define VALIDATE_LOG(bm, entry)
#endif

#define CUSTOMDATA

void CustomData_bmesh_asan_unpoison(const CustomData *data, void *block);
void CustomData_bmesh_asan_poison(const CustomData *data, void *block);

//#define DO_LOG_PRINT

//#define DEBUG_LOG_TO_FILE

#define BM_LOG_USE_SMALLHASH

#ifdef BM_LOG_USE_SMALLHASH
static SmallHash *new_smallhash()
{
  SmallHash *sh = MEM_callocN(sizeof(*sh), "smallhash for dyntopo");

  BLI_smallhash_init(sh);

  return sh;
}

static void free_smallhash(SmallHash *sh)
{
  BLI_smallhash_release(sh);
  MEM_freeN(sh);
}

static void smallhash_reserve(SmallHash *sh, unsigned int n)
{
}

typedef struct myiter {
  SmallHashIter iter;
  uintptr_t key;
  void *data;
} myiter;

#  ifdef GHASH_ITER
#    undef GHASH_ITER
#  endif

#  define GHashIterator myiter

#  ifdef __GNUC__
/* I can't even *cast* signed ints in gcc's sign-conversion warning? gcc 10.3.0 -joeedh */
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#  endif

#  ifdef __GNUC__
/* I can't even *cast* signed ints in gcc's sign-conversion warning? gcc 10.3.0 -joeedh */
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#  endif

static void *my_popkey(SmallHash *sh, const void *key)
{
  void *val;

  if (BLI_smallhash_remove_p(sh, (uintptr_t)key, &val)) {
    return val;
  }
  else {
    return NULL;
  }
}

#  define BLI_ghash_popkey(sh, key, a) my_popkey(sh, key)
#  define BLI_ghash_free(sh, a, b) free_smallhash(sh)
#  define BLI_ghash_int_new_ex(a, b) new_smallhash()
#  define BLI_ghash_reserve(sh, n) smallhash_reserve(sh, (unsigned int)(n))
#  define BLI_ghash_new(a, b, c) new_smallhash()
#  define BLI_ghash_insert(sh, key, val) BLI_smallhash_insert((sh), (uintptr_t)(key), (val))
#  define BLI_ghash_remove(sh, key, a, b) BLI_smallhash_remove((sh), (uintptr_t)(key))
#  define BLI_ghash_lookup(sh, key) BLI_smallhash_lookup((sh), (uintptr_t)(key))
#  define BLI_ghash_lookup_p(sh, key) BLI_smallhash_lookup_p((sh), (uintptr_t)(key))
#  define BLI_ghash_haskey(sh, key) BLI_smallhash_haskey((sh), (uintptr_t)(key))
#  define GHASH_ITER(iter1, sh) \
    for (iter1.data = BLI_smallhash_iternew(sh, &iter1.iter, &iter1.key); iter1.data; \
         iter1.data = BLI_smallhash_iternext(&iter1.iter, &iter1.key))
#  define BLI_ghashIterator_getKey(iter) ((void *)(iter)->key)
#  define BLI_ghashIterator_getValue(iter) ((void *)(iter)->data)
#  define BLI_ghash_ensure_p(sh, key, val) BLI_smallhash_ensure_p(sh, (uintptr_t)(key), val)
#  define BLI_ghash_reinsert(sh, key, val, a, b) BLI_smallhash_reinsert(sh, (uintptr_t)(key), val)
#  define BLI_ghash_len(sh) (int)sh->nentries
#  define GHash SmallHash

#  ifdef BM_ELEM_FROM_ID
#    undef BM_ELEM_FROM_ID
#  endif

#  define BM_ELEM_FROM_ID(bm, id) \
    ((bm->idmap.flag & BM_NO_REUSE_IDS) ? \
         BLI_ghash_lookup(bm->idmap.ghash, POINTER_FROM_UINT(id)) : \
         bm->idmap.map[id])

#endif
//#define DEBUG_LOG_REFCOUNTNG
//#define PRINT_LOG_REF_COUNTING

#ifdef DEBUG_LOG_TO_FILE
FILE *DEBUG_FILE = NULL;
#else
#  define DEBUG_FILE stdout
#endif

typedef struct BMLogHead {
#ifdef BM_LOG_TRACE
  const char *func;
  int line;
#endif

#ifdef DO_LOG_PRINT
  char msg[64];
#endif

#ifdef DEBUG_LOG_CALL_STACKS
  const char *tag;
#endif

  uint id;
} BMLogHead;

typedef struct BMLogElem {
  BMLogHead head;
} BMLogElem;

#ifdef DEBUG_LOG_CALL_STACKS

static struct {
  char tag[1024];
} fine_namestack[256] = {0};
static int fine_namestack_i = 0;
static SmallHash *small_str_hash = NULL;

#  define bm_logstack_head _bm_logstack_head

#  define SET_TRACE(le) (le)->head.tag = bm_logstack_head()
#  define _GET_TRACE(le, entry) gettrace_format((le)->head.tag, entry)

const char *small_str_get(const char *str)
{
  void **val;
  uint hash = BLI_hash_string(str);

  if (!small_str_hash) {
    small_str_hash = MEM_callocN(sizeof(*small_str_hash), "small_str_hash");
    BLI_smallhash_init(small_str_hash);
  }

  if (!BLI_smallhash_ensure_p(small_str_hash, (uintptr_t)hash, &val)) {
    *val = strdup(str);
  }

  return (const char *)*val;
}

void _bm_logstack_push(const char *name)
{
  fine_namestack_i++;

  strcpy(fine_namestack[fine_namestack_i].tag, fine_namestack[fine_namestack_i - 1].tag);
  strcat(fine_namestack[fine_namestack_i].tag, ".");
  strcat(fine_namestack[fine_namestack_i].tag, name);
}

const char *_bm_logstack_head()
{
  return small_str_get(fine_namestack[fine_namestack_i].tag);
}

void _bm_logstack_pop()
{
  fine_namestack_i--;
}
#else
#  define SET_TRACE(le)
#  define _GET_TRACE(entry, le) (((void *)le) ? __func__ : __func__)
#endif

#ifdef BM_LOG_TRACE
static const char *_get_trace(struct BMLogEntry *entry, BMLogElem *le, const char *_trace)
{
  static char buf[512];

  sprintf(buf, "%s(%s:%d)", _trace, le->head.func, le->head.line);

  return buf;
}
#  define GET_TRACE(le, entry) _get_trace(entry, (BMLogElem *)le, _GET_TRACE(le, entry))
#else
#  define GET_TRACE(le, entry) _GET_TRACE(le, entry)
#endif

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

#ifdef DO_LOG_PRINT
static int msg_idgen = 1;
static char msg_buffer[256] = {0};

#  define SET_MSG(le) memcpy(le->msg, msg_buffer, sizeof(le->msg))
#  define GET_MSG(le) (le)->msg
#  ifdef DEBUG_LOG_CALL_STACKS
#    define LOGPRINT(entry, ...) \
      fprintf(DEBUG_FILE, "%d: %s: ", entry->id, bm_logstack_head()); \
      fprintf(DEBUG_FILE, __VA_ARGS__)
#  else
#    define LOGPRINT(entry, ...) \
      fprintf(DEBUG_FILE, "%s: ", __func__); \
      fprintf(DEBUG_FILE, __VA_ARGS__)
#  endif
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

  fprintf(DEBUG_FILE, "%s\n", msg);
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

  /* The following #GHash members map from an element ID to one of the log types above. */

  /* topology at beginning of step */
  GHash *topo_modified_verts_pre;
  GHash *topo_modified_edges_pre;
  GHash *topo_modified_faces_pre;

  /* topology at end of step */
  GHash *topo_modified_verts_post;
  GHash *topo_modified_edges_post;
  GHash *topo_modified_faces_post;

  /** Vertices whose coordinates, mask value, or hflag have changed. */
  GHash *modified_verts;
  GHash *modified_edges;
  GHash *modified_faces;

  BLI_mempool *pool_verts;
  BLI_mempool *pool_edges;
  BLI_mempool *pool_faces;
  MemArena *arena;

  /**
   * This is only needed for dropping BMLogEntries while still in
   * dynamic-topology mode, as that should release vert/face IDs
   * back to the BMLog but no BMLog pointer is available at that time.
   *
   * This field is not guaranteed to be valid, any use of it should
   * check for NULL.
   */
  BMLog *log;

  CustomData vdata, edata, ldata, pdata;
  struct BMLogEntry *combined_prev, *combined_next;

  BMLogEntryType type;

  struct Mesh
      *full_copy_mesh;  // avoid excessive memory use by saving a Mesh instead of copying the bmesh
  BMLogIdMap idmap;

  int id;
};

#ifdef DEBUG_LOG_CALL_STACKS
static char *gettrace_format(const char *a, BMLogEntry *entry)
{
  static char buf[1024];
  sprintf(buf, "%d: %s", entry->id, a);

  return buf;
}
#endif

struct BMLog {
  // BMLogEntry *frozen_full_mesh;

  int refcount;

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

  ThreadRWMutex lock;

  BMesh *bm;

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

  bool has_edges;
  int cd_sculpt_vert;
  bool dead;

  BMIdMap *idmap;
};

static void _bm_log_addref(BMLog *log, const char *func)
{
  log->refcount++;

#ifdef PRINT_LOG_REF_COUNTING
  fprintf(DEBUG_FILE, "%d %s: bm_log_addref: %p\n", log->refcount, namestack_head_name, log);
  fflush(stdout);
#endif
}

static void _bm_log_decref(BMLog *log, const char *func)
{
  log->refcount--;
#ifdef PRINT_LOG_REF_COUNTING
  fprintf(DEBUG_FILE, "%d %s: bm_log_decref: %p\n", log->refcount, namestack_head_name, log);
  fflush(stdout);
#endif
}

#define bm_log_addref(log) _bm_log_addref(log, __func__)
#define bm_log_decref(log) _bm_log_decref(log, __func__)

typedef struct BMLogVert {
  BMLogHead head;

  float co[3];
  float no[3];
  char hflag;
  void *customdata;
} BMLogVert;

typedef struct BMLogEdge {
  BMLogHead head;

  uint v1, v2;
  char hflag;
  void *customdata;
} BMLogEdge;

#define MAX_FACE_RESERVED 8

typedef struct BMLogFace {
  BMLogHead head;

  uint *v_ids;
  uint *l_ids;
  void **customdata;

  float no[3];
  void *customdata_f;
  char hflag;

  uint len;
  short mat_nr;

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

static unsigned char *get_elem_htype_str(int htype)
{
  switch (htype) {
    case BM_VERT:
      return "vertex";
    case BM_EDGE:
      return "edge";
    case BM_LOOP:
      return "loop";
    case BM_FACE:
      return "face";
    default:
      return "unknown type";
  }
}

#ifdef USE_NEW_IDMAP
void bm_log_free_id(BMLog *log, BMElem *elem, bool clear_id)
{
  BM_idmap_release(log->idmap, elem, clear_id);
}

void bm_log_alloc_id(BMLog *log, BMElem *elem)
{
  BM_idmap_check_assign(log->idmap, elem);
}

void bm_log_assign_id(BMLog *log, BMElem *elem, int id, bool check_unique)
{
  if (check_unique) {
    BMElem *old;

    if ((old = BM_idmap_lookup(log->idmap, id))) {
      printf("id conflict in bm_assign_id; elem %p (a %s) is being reassinged to id %d.\n",
             elem,
             get_elem_htype_str((int)elem->head.htype),
             (int)id);
      printf(
          "  elem %p (a %s) will get a new id\n", old, get_elem_htype_str((int)old->head.htype));

      BM_idmap_assign(log->idmap, elem, id);
      return;
    }
  }

  BM_idmap_assign(log->idmap, elem, id);
}
#else
void bm_log_free_id(BMLog *log, BMElem *elem, bool /* clear_id */)
{
  bm_free_id(log->bm, elem);
}

void bm_log_alloc_id(BMLog *log, BMElem *elem)
{
  bm_alloc_id(log->bm, elem);
}

void bm_log_assign_id(BMLog *log, BMElem *elem, int id, bool check_unique)
{
  bm_assign_id(log->bm, elem, id, check_unique);
}
#endif

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

static bool log_ghash_remove(GHash *gh, const void *key, BLI_mempool *pool)
{
  void *val = BLI_ghash_popkey(gh, key, keyfree);

  if (val && pool) {
    BLI_mempool_free(pool, val);
  }

  return val != NULL;
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

#ifdef USE_NEW_IDMAP
ATTR_NO_OPT static void bm_log_set_id_raw(BMLog *log, BMElem *elem, int id)
{
  BM_ELEM_CD_SET_INT(elem, log->idmap->cd_id_off[elem->head.htype], id);
}

/* Get the vertex's unique ID from the log */
ATTR_NO_OPT static uint bm_log_vert_id_get(BMLog *log, BMVert *v)
{
  return BM_idmap_get_id(log->idmap, (BMElem *)v);
}

/*Get a vertex from its unique ID */
ATTR_NO_OPT static BMElem *bm_log_elem_from_id(BMLog *log, uint id)
{

  if (log->idmap->map && id >= ((unsigned int)log->idmap->map_size)) {
    return NULL;
  }

  return BM_idmap_lookup(log->idmap, id);
}

/* Get a vertex from its unique ID */
ATTR_NO_OPT static BMVert *bm_log_vert_from_id(BMLog *log, uint id)
{
  return (BMVert *)bm_log_elem_from_id(log, id);
}

BMVert *BM_log_id_vert_get(BMLog *log, uint id)
{
  return bm_log_vert_from_id(log, id);
}

/* Get the edges's unique ID from the log */
ATTR_NO_OPT static uint bm_log_edge_id_get(BMLog *log, BMEdge *e)
{
  return BM_idmap_get_id(log->idmap, (BMElem *)e);
}

static uint bm_log_loop_id_get(BMLog *log, BMLoop *l)
{
  return BM_idmap_get_id(log->idmap, (BMElem *)l);
}

/* Get a vertex from its unique ID */
static BMEdge *bm_log_edge_from_id(BMLog *log, uint id)
{
  return (BMEdge *)bm_log_elem_from_id(log, id);
}

/* Get the face's unique ID from the log */
ATTR_NO_OPT static uint bm_log_face_id_get(BMLog *log, BMFace *f)
{
  return BM_idmap_get_id(log->idmap, (BMElem *)f);
}

ATTR_NO_OPT static uint bm_log_elem_id_get(BMLog *log, BMElem *elem)
{
  return BM_idmap_get_id(log->idmap, elem);
}

/* Get a face from its unique ID */
ATTR_NO_OPT static BMFace *bm_log_face_from_id(BMLog *log, uint id)
{
  return (BMFace *)bm_log_elem_from_id(log, id);
}

#else
ATTR_NO_OPT static void bm_log_set_id_raw(BMLog *log, BMElem *elem, int id)
{
  BM_ELEM_CD_SET_INT(elem, log->bm->idmap.cd_id_off[elem->head.htype], id);
}

/* Get the vertex's unique ID from the log */
static uint bm_log_vert_id_get(BMLog *log, BMVert *v)
{
  return (uint)BM_ELEM_GET_ID(log->bm, v);
}

#  undef BLI_ghash_lookup

/*Get a vertex from its unique ID */
static BMElem *bm_log_elem_from_id(BMLog *log, uint id)
{
  if (log->bm->idmap.map && id >= ((unsigned int)log->bm->idmap.map_size)) {
    return NULL;
  }

  return (BMElem *)BM_ELEM_FROM_ID(log->bm, id);
}

/* Get a vertex from its unique ID */
static BMVert *bm_log_vert_from_id(BMLog *log, uint id)
{
  if (log->bm->idmap.map && id >= ((unsigned int)log->bm->idmap.map_size)) {
    return NULL;
  }

  return (BMVert *)BM_ELEM_FROM_ID(log->bm, id);
}

BMVert *BM_log_id_vert_get(BMLog *log, uint id)
{
  return bm_log_vert_from_id(log, id);
}

/* Get the edges's unique ID from the log */
static uint bm_log_edge_id_get(BMLog *log, BMEdge *e)
{
  return BM_ELEM_GET_ID(log->bm, e);
}

static uint bm_log_loop_id_get(BMLog *log, BMLoop *l)
{
  return BM_ELEM_GET_ID(log->bm, l);
}

/* Get a vertex from its unique ID */
static BMEdge *bm_log_edge_from_id(BMLog *log, uint id)
{
  return (BMEdge *)BM_ELEM_FROM_ID(log->bm, id);
}

/* Get the face's unique ID from the log */
static uint bm_log_face_id_get(BMLog *log, BMFace *f)
{
  return BM_ELEM_GET_ID(log->bm, f);
}

static uint bm_log_elem_id_get(BMLog *log, BMElem *elem)
{
  return BM_ELEM_GET_ID(log->bm, elem);
}

/* Get a face from its unique ID */
static BMFace *bm_log_face_from_id(BMLog *log, uint id)
{
  if (log->bm->idmap.map && id >= ((unsigned int)log->bm->idmap.map_size)) {
    return NULL;
  }

  return (BMFace *)BM_ELEM_FROM_ID(log->bm, id);
}
#  define BLI_ghash_lookup(sh, key) BLI_smallhash_lookup((sh), (uintptr_t)(key))

#endif

uint BM_log_vert_id_get(BMLog *log, BMVert *v)
{
  return bm_log_vert_id_get(log, v);
}

BMEdge *BM_log_id_edge_get(BMLog *log, uint id)
{
  return bm_log_edge_from_id(log, id);
}

uint BM_log_face_id_get(BMLog *log, BMFace *f)
{
  return bm_log_face_id_get(log, f);
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
    CustomData_bmesh_asan_unpoison(&entry->vdata, lv->customdata);
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
    CustomData_bmesh_asan_unpoison(&entry->edata, le->customdata);
    BLI_mempool_free(entry->edata.pool, le->customdata);
    le->customdata = NULL;
  }

  CustomData_bmesh_copy_data(&bm->edata, &entry->edata, e->head.data, &le->customdata);
}

static void bm_log_face_customdata(BMesh *bm, BMLog *log, BMFace *f, BMLogFace *lf)
{
  BMLogEntry *entry = log->current_entry;

  if (!entry || !lf) {
    fprintf(DEBUG_FILE, "%s: bmlog error\n", __func__);
    return;
  }

  if (lf->customdata_f) {
    CustomData_bmesh_asan_unpoison(&entry->pdata, lf->customdata_f);
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
      CustomData_bmesh_asan_unpoison(&entry->ldata, lf->customdata[i]);
      BLI_mempool_free(entry->ldata.pool, lf->customdata[i]);
      lf->customdata[i] = NULL;
    }

    CustomData_bmesh_copy_data(&bm->ldata, &entry->ldata, l->head.data, &lf->customdata[i]);
  } while ((i++, l = l->next) != f->l_first);
}

static void bm_log_face_customdata_reuse(BMesh *bm, BMLog *log, BMFace *f, BMLogFace *lf)
{
  BMLogEntry *entry = log->current_entry;

  if (!entry || !lf) {
    fprintf(DEBUG_FILE, "%s: bmlog error\n", __func__);
    return;
  }

  CustomData_bmesh_copy_data(&bm->pdata, &entry->pdata, f->head.data, &lf->customdata_f);

  BMLoop *l = f->l_first;
  int i = 0;
  do {
    CustomData_bmesh_copy_data(&bm->ldata, &entry->ldata, l->head.data, &lf->customdata[i]);
  } while ((i++, l = l->next) != f->l_first);
}

/* Update a BMLogVert with data from a BMVert */
static void bm_log_vert_bmvert_copy(
    BMLog *log, BMLogEntry *entry, BMLogVert *lv, BMVert *v, bool copy_customdata)
{
  copy_v3_v3(lv->co, v->co);
  copy_v3_v3(lv->no, v->no);

  lv->hflag = v->head.hflag;

  if (copy_customdata) {
    bm_log_vert_customdata(log->bm, log, entry, v, lv);
  }
}

/* Allocate and initialize a BMLogVert */
static BMLogVert *bm_log_vert_alloc(BMLog *log, BMVert *v, bool log_customdata)
{
  BMLogEntry *entry = log->current_entry;
  BMLogVert *lv = BLI_mempool_alloc(entry->pool_verts);
  lv->customdata = NULL;

  SET_TRACE(lv);

  bm_log_vert_bmvert_copy(log, entry, lv, v, log_customdata);

  return lv;
}

static void bm_log_edge_bmedge_copy(
    BMLog *log, BMLogEntry *entry, BMLogEdge *le, BMEdge *e, bool copy_customdata)
{
  if (e->head.htype != BM_EDGE) {
    fprintf(
        DEBUG_FILE, "%s: e is not an edge; htype: %d\n", GET_TRACE(le, entry), (int)e->head.htype);
  }

  BM_idmap_check_assign(log->idmap, (BMElem *)e->v1);
  BM_idmap_check_assign(log->idmap, (BMElem *)e->v2);

  le->v1 = (uint)bm_log_vert_id_get(log, e->v1);
  le->v2 = (uint)bm_log_vert_id_get(log, e->v2);

  le->head.id = (uint)bm_log_edge_id_get(log, e);
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

  SET_TRACE(le);

#ifdef DO_LOG_PRINT
  le->head.msg[0] = 0;
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
  lf->head.id = (uint)bm_log_face_id_get(log, f);
  lf->mat_nr = f->mat_nr;

  SET_TRACE(lf);

#ifdef USE_NEW_IDMAP
  bool have_loop_ids = (log->idmap->flag & BM_LOOP);
#else
  bool have_loop_ids = (log->bm->idmap.flag & BM_LOOP);
#endif

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
      BM_idmap_check_assign(log->idmap, (BMElem *)l);
      lf->l_ids[i] = (uint)bm_log_loop_id_get(log, l);
    }
    else {
      lf->l_ids[i] = (uint)-1;
    }

    BM_idmap_check_assign(log->idmap, (BMElem *)l->v);
    lf->v_ids[i] = bm_log_vert_id_get(log, l->v);

    lf->customdata[i] = NULL;
  } while ((i++, l = l->next) != f->l_first);

  lf->hflag = f->head.hflag;
  return lf;
}

/* Allocate and initialize a BMLogFace */
static void bm_log_face_bmface_copy(
    BMLog *log, BMFace *f, BMLogFace *lf, BMLogEntry *entry, bool copy_customdata)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)f);

  if ((int)lf->len != (int)f->len) {
    fprintf(DEBUG_FILE,
            "%s: face %d's topology mismatches log entry's\n",
            GET_TRACE(lf, entry),
            lf->head.id);
    bm_logstack_pop();
    return;
  }

  if ((int)bm_log_face_id_get(log, f) != (int)lf->head.id) {
    fprintf(DEBUG_FILE,
            "%s: face %d's id mismstaches log entry's\n",
            GET_TRACE(lf, entry),
            lf->head.id);
    bm_logstack_pop();
    return;
  }

  if (0 && copy_customdata) {
    // free existing customdata blocks

    if (lf->customdata_f) {
      CustomData_bmesh_asan_unpoison(&entry->pdata, lf->customdata_f);
      BLI_mempool_free(entry->pdata.pool, lf->customdata_f);
      lf->customdata_f = NULL;
    }

    for (uint i = 0; i < lf->len; i++) {
      if (lf->customdata[i]) {
        CustomData_bmesh_asan_unpoison(&entry->ldata, lf->customdata[i]);
        BLI_mempool_free(entry->ldata.pool, lf->customdata[i]);
        lf->customdata[i] = NULL;
      }
    }
  }

  copy_v3_v3(lf->no, f->no);

  if (copy_customdata) {
    bm_log_face_customdata_reuse(log->bm, log, f, lf);
  }

#ifdef DEBUG_LOG_CALL_STACKS
  char buf[2048];
  strcpy(buf, lf->head.tag);

  SET_TRACE(lf);

  strcat(buf, "->");
  strcat(buf, lf->head.tag);

  lf->head.tag = strdup(buf);
#endif

  lf->hflag = f->head.hflag;

  bm_logstack_pop();
  return;
}

/************************ Helpers for undo/redo ***********************/

// exec vert kill callbacks before killing faces
static void bm_log_verts_unmake_pre(
    BMesh *bm, BMLog *log, GHash *verts, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  BMLogVert *lv;

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, verts) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    lv = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMVert *v = bm_log_vert_from_id(log, id);

    if (!v) {
      fprintf(
          DEBUG_FILE, "%s[%s]: missing vertex for id: %d\n", GET_TRACE(lv, entry), __func__, id);
      continue;
    }

    if (v->head.htype != BM_VERT) {
      fprintf(DEBUG_FILE,
              "%s[%s]: vertex id: %d, type was: %d\n",
              GET_TRACE(lv, entry),
              __func__,
              id,
              v->head.htype);
      continue;
    }

    /* Ensure the log has the final values of the vertex before
     * deleting it */

    bm_log_vert_bmvert_copy(log, entry, lv, v, true);

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
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    BMEdge *e = bm_log_edge_from_id(log, le->head.id);

    if (!e) {
      fprintf(DEBUG_FILE,
              "%s: missing edge; id: %d [%s]\n",
              GET_TRACE(le, entry),
              le->head.id,
              GET_MSG(&le->head));
      continue;
    }

    if (e->head.htype != BM_EDGE) {
      fprintf(DEBUG_FILE,
              "%s: not an edge; edge id: %d, type was: %d [%s]\n",
              GET_TRACE(le, entry),
              le->head.id,
              e->head.htype,
              GET_MSG(&le->head));
      continue;
    }

    /* Ensure the log has the final values of the edge before
     * deleting it */
    bm_log_edge_bmedge_copy(log, entry, le, e, true);

    if (callbacks) {
      callbacks->on_edge_kill(e, callbacks->userdata);
    }
  }
}

static void bm_log_faces_unmake_pre(
    BMesh *bm, BMLog *log, GHash *faces, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, faces) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
    uint id = POINTER_AS_UINT(key);
    BMFace *f = bm_log_face_from_id(log, id);

    if (!f) {
      fprintf(DEBUG_FILE, "%s: vertex id: %d\n", GET_TRACE(lf, entry), id);
      continue;
    }

    if (f->head.htype != BM_FACE) {
      fprintf(DEBUG_FILE,
              "%s: vertex id: %d, type was: %d\n",
              GET_TRACE(lf, entry),
              id,
              f->head.htype);
      continue;
    }

    /* Ensure the log has the final values of the face before
     * deleting it */

    bm_log_face_bmface_copy(log, f, lf, entry, true);

    if (callbacks) {
      callbacks->on_face_kill(f, callbacks->userdata);
    }
  }
}

static void bm_log_edges_unmake(
    BMesh *bm, BMLog *log, GHash *edges, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;

  GHASH_ITER (gh_iter, edges) {
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    BMEdge *e = bm_log_edge_from_id(log, le->head.id);

    if (!e) {
      fprintf(DEBUG_FILE,
              "%s: missing edge; edge id: %d [%s]\n",
              GET_TRACE(le, entry),
              le->head.id,
              GET_MSG(&le->head));
      continue;
    }

    if (e->head.htype != BM_EDGE) {
      fprintf(DEBUG_FILE,
              "%s: not an edge; edge id: %d, type: %d [%s]\n",
              GET_TRACE(le, entry),
              le->head.id,
              e->head.htype,
              GET_MSG(&le->head));
      continue;
    }

#ifdef USE_NEW_IDMAP
    BM_idmap_release(log->idmap, (BMElem *)e, false);
#endif
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
    BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);

    // avoid unused variable warning/error if GET_TRACE is turned off
    (void *)lv;

    if (!v || v->head.htype != BM_VERT) {
      fprintf(DEBUG_FILE,
              "%s[%s]: missing vertex error, vertex id: %d\n",
              GET_TRACE(lv, entry),
              __func__,
              (int)id);
      continue;
    }

#ifdef USE_NEW_IDMAP
    BM_idmap_release(log->idmap, (BMElem *)v, false);
#endif
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
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
    BMFace *f = bm_log_face_from_id(log, lf->head.id);

    if (!f) {
      fprintf(DEBUG_FILE, "%s: missing face %d\n", GET_TRACE(lf, entry), lf->head.id);
      continue;
    }

    if (f->head.htype != BM_FACE) {
      fprintf(
          DEBUG_FILE, "%s: f was not a face, type was: %d\n", GET_TRACE(lf, entry), f->head.htype);
      continue;
    }

    BLI_array_clear(e_tri);

#ifdef USE_NEW_IDMAP
    BM_idmap_release(log->idmap, (BMElem *)f, false);
#endif
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
    BMElem *elem;
    const char *tag = __func__;

    if ((elem = (BMElem *)bm_log_vert_from_id(log, POINTER_AS_UINT(key)))) {
#ifdef DEBUG_LOG_CALL_STACKS
      if (elem->head.htype == BM_VERT) {
        BMLogVert *lv = log_ghash_lookup(log,
                                         verts == entry->topo_modified_verts_pre ?
                                             entry->topo_modified_verts_post :
                                             entry->topo_modified_verts_pre,
                                         key);
        tag = lv ? lv->head.tag : tag;
      }
      else if (elem->head.htype == BM_EDGE) {
        BMLogEdge *lv = log_ghash_lookup(log,
                                         verts == entry->topo_modified_verts_pre ?
                                             entry->topo_modified_edges_post :
                                             entry->topo_modified_edges_pre,
                                         key);

        if (!lv) {
          lv = log_ghash_lookup(log,
                                verts == entry->topo_modified_verts_pre ?
                                    entry->topo_modified_edges_pre :
                                    entry->topo_modified_edges_post,
                                key);
        }
        tag = lv ? lv->head.tag : tag;
      }
      else if (elem->head.htype == BM_FACE) {
        BMLogFace *lv = log_ghash_lookup(log,
                                         verts == entry->topo_modified_verts_pre ?
                                             entry->topo_modified_faces_post :
                                             entry->topo_modified_faces_pre,
                                         key);

        if (!lv) {
          lv = log_ghash_lookup(log,
                                verts == entry->topo_modified_verts_pre ?
                                    entry->topo_modified_faces_pre :
                                    entry->topo_modified_faces_post,
                                key);
        }
        tag = lv ? lv->head.tag : tag;
      }
#endif
      fprintf(DEBUG_FILE,
              "%s: element already exists in place of vert; type: %d, \n    trace: %s\n",
              GET_TRACE(lv, entry),
              elem->head.htype,
              tag);

      continue;
    }

    BMVert *v = BM_vert_create(bm, lv->co, NULL, BM_CREATE_SKIP_ID);

    v->head.hflag = lv->hflag;
    copy_v3_v3(v->no, lv->no);

#ifdef CUSTOMDATA
    if (lv->customdata) {
      CustomData_bmesh_copy_data(&entry->vdata, &bm->vdata, lv->customdata, &v->head.data);
    }
#endif

    bm_log_assign_id(log, (BMElem *)v, POINTER_AS_UINT(key), true);

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
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    bool assign_id = true;

    BMVert *v1 = bm_log_vert_from_id(log, le->v1);
    BMVert *v2 = bm_log_vert_from_id(log, le->v2);

    if (!v1 || !v2) {
      fprintf(DEBUG_FILE, "%s: missing edge verts: %p %p\n", GET_TRACE(le, entry), v1, v2);
      continue;
    }

    if (v1->head.htype != BM_VERT || v2->head.htype != BM_VERT) {
      fprintf(DEBUG_FILE,
              "%s: edge verts were not verts: %d %d\n",
              GET_TRACE(le, entry),
              v1->head.htype,
              v2->head.htype);
      continue;
    }

    BMEdge *e = BM_edge_exists(v1, v2);
    if (e) {
      fprintf(DEBUG_FILE,
              "%s: edge already %d existed (but id was %d):\n",
              GET_TRACE(le, entry),
              (int)le->head.id,
              bm_log_edge_id_get(log, e));

      if (bm_log_edge_id_get(log, e) != (int)le->head.id) {
        bm_log_free_id(log, (BMElem *)e, true);
      }
      else {
        assign_id = false;
      }
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

    if (assign_id) {
      bm_log_assign_id(log, (BMElem *)e, le->head.id, true);
    }

    if ((uint)bm_log_edge_id_get(log, e) != le->head.id) {
      fprintf(DEBUG_FILE, "%s: error assigning id\n", GET_TRACE(le, entry));
    }

    if (callbacks) {
      callbacks->on_edge_add(e, callbacks->userdata);
    }
  }
}

ATTR_NO_OPT static void bm_log_faces_restore(
    BMesh *bm, BMLog *log, GHash *faces, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  GHashIterator gh_iter;
  BMVert **vs_tmp = NULL;
  BLI_array_staticdeclare(vs_tmp, 32);

#ifdef USE_NEW_IDMAP
  void *_scratch = alloca(log->idmap->cd_id_off[BM_FACE] + sizeof(void *));
  bool have_loop_ids = (log->idmap->flag & BM_LOOP);
#else
  void *_scratch = alloca(bm->idmap.cd_id_off[BM_FACE] + sizeof(void *));
  bool have_loop_ids = (log->bm->idmap.flag & BM_LOOP);
#endif

  GHASH_ITER (gh_iter, faces) {
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
    BMElem id_holder = {0};

    id_holder.head.htype = BM_FACE;
    id_holder.head.data = _scratch;
    bm_log_assign_id(log, &id_holder, lf->head.id, true);

    BLI_array_clear(vs_tmp);
    bool bad = false;

    for (int i = 0; i < (int)lf->len; i++) {
      BMVert *v = bm_log_vert_from_id(log, lf->v_ids[i]);

      if (v) {
        BMVert *v2 = bm_log_vert_from_id(log, lf->v_ids[(i + 1) % lf->len]);

        if (v2 && !BM_edge_exists(v, v2)) {
          fprintf(DEBUG_FILE,
                  "%s: missing edge for face %d\n",
                  GET_TRACE(lf, entry),
                  (int)lf->head.id);
        }
      }

      if (!v) {
        BMIter iter;
        BMVert *v2;

#ifdef USE_NEW_IDMAP
        const int cd_id = log->idmap->cd_id_off[BM_VERT];
#else
        const int cd_id = bm->idmap.cd_id_off[BM_VERT];
#endif

        bad = true;

        BM_ITER_MESH (v2, &iter, bm, BM_VERTS_OF_MESH) {
          int id = BM_ELEM_CD_GET_INT(v2, cd_id);

          if (lf->v_ids[i] == (uint)id) {
            fprintf(DEBUG_FILE, "found vertex %d\n", id);
            bad = false;
            v = v2;
            break;
          }
        }

        if (bad) {
          fprintf(DEBUG_FILE, "%s: Undo error! %p\n", GET_TRACE(lf, entry), v);
          break;
        }
      }

      if (bad) {
        continue;
      }

      if (v->head.htype != BM_VERT) {
        fprintf(DEBUG_FILE,
                "%s: vert %d in face %d was not a vertex; type: %d\n",
                GET_TRACE(lf, entry),
                (int)lf->v_ids[i],
                lf->head.id,
                v->head.htype);
        continue;
      }
      BLI_array_append(vs_tmp, v);
    }

    if ((int)BLI_array_len(vs_tmp) < 2) {
      fprintf(DEBUG_FILE,
              "%s: severely malformed face %d in %s\n",
              GET_TRACE(lf, entry),
              lf->head.id,
              __func__);
      continue;
    }

#if 0
    for (size_t j = 0; j < lf->len; j++) {
      BMVert* v1 = bm_log_vert_from_id(log, lf->v_ids[j]);
      BMVert* v2 = bm_log_vert_from_id(log, lf->v_ids[(j + 1) % lf->len]);

      if (!v1 || !v2) {
        continue;
      }

      if (!BM_edge_exists(v1, v2)) {
        int id = POINTER_AS_INT(key);
        fprintf(DEBUG_FILE, "%s: missing edge, face %d had to create it\n", __func__, (int)id);
      }
    }
#endif

    BMFace *f = BM_face_create_verts(
        bm, vs_tmp, (int)BLI_array_len(vs_tmp), NULL, BM_CREATE_SKIP_ID, true);

    f->head.hflag = lf->hflag;
    f->mat_nr = lf->mat_nr;

    copy_v3_v3(f->no, lf->no);

    if (lf->customdata_f) {
      CustomData_bmesh_copy_data(&entry->pdata, &bm->pdata, lf->customdata_f, &f->head.data);
    }

    bm_log_free_id(log, &id_holder, true);
    bm_log_assign_id(log, (BMElem *)f, lf->head.id, true);

    BMLoop *l = f->l_first;
    int j = 0;

    do {
      if (have_loop_ids) {
        bm_log_assign_id(log, (BMElem *)l, lf->l_ids[j], true);
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
      fprintf(
          DEBUG_FILE, "%s[%s]: missing vert in bmlog! %d\n", GET_TRACE(lv, entry), __func__, id);
      continue;
    }

    if (v->head.htype != BM_VERT) {
      fprintf(DEBUG_FILE,
              "%s[%s]: %d is not a vertex; type: %d\n",
              GET_TRACE(lv, entry),
              __func__,
              id,
              v->head.htype);
      continue;
    }

    swap_v3_v3(v->co, lv->co);
    swap_v3_v3(v->no, lv->no);

    SWAP(char, v->head.hflag, lv->hflag);

    void *old_cdata = NULL;

    if (lv->customdata) {
      if (v->head.data) {
        old_cdata = scratch;

        CustomData_bmesh_asan_unpoison(&bm->vdata, v->head.data);
        memcpy(old_cdata, v->head.data, (size_t)bm->vdata.totsize);
        CustomData_bmesh_asan_poison(&bm->vdata, v->head.data);
      }

      CustomData_bmesh_swap_data(&entry->vdata, &bm->vdata, lv->customdata, &v->head.data);

      /* Ensure we have the correct id */
      bm_log_set_id_raw(log, (BMElem *)v, id);
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
    BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
    BMEdge *e = bm_log_edge_from_id(log, le->head.id);

    SWAP(char, e->head.hflag, le->hflag);

    void *old_cdata = NULL;

    if (le->customdata) {
      if (e->head.data) {
        old_cdata = scratch;
        CustomData_bmesh_asan_unpoison(&bm->edata, e->head.data);
        memcpy(old_cdata, e->head.data, (size_t)bm->edata.totsize);
        CustomData_bmesh_asan_poison(&bm->edata, e->head.data);
      }

      CustomData_bmesh_swap_data(&entry->edata, &bm->edata, le->customdata, &e->head.data);

      /* Ensure we have the correct id. */
      bm_log_set_id_raw(log, (BMElem *)e, le->head.id);
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
    BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
    BMFace *f = bm_log_face_from_id(log, lf->head.id);

    if (!f) {
      fprintf(stderr, "%s: Failed to find face %d!\n", __func__, (int)lf->head.id);
      continue;
    }

    if (f->head.htype != BM_FACE) {
      fprintf(stderr,
              "%s: Got non-face for face ID %d, type was %d\n",
              __func__,
              (int)lf->head.id,
              (int)f->head.htype);
      continue;
    }

    swap_v3_v3(f->no, lf->no);
    SWAP(char, f->head.hflag, lf->hflag);
    SWAP(short, f->mat_nr, lf->mat_nr);

    void *old_cdata = NULL;

    if (f->head.data) {
      old_cdata = scratch;
      CustomData_bmesh_asan_unpoison(&log->bm->pdata, f->head.data);
      memcpy(old_cdata, f->head.data, (size_t)log->bm->pdata.totsize);
      CustomData_bmesh_asan_poison(&log->bm->pdata, f->head.data);
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

    /* Ensure we have the correct id. */
    bm_log_set_id_raw(log, (BMElem *)f, lf->head.id);

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
  // keep shapekey as explicit cd layers since we
  // don't have access to the original mesh's ->key member.

  CustomData_MeshMasks cd_mask_extra = {CD_MASK_DYNTOPO_VERT | CD_MASK_SHAPEKEY, 0, 0, 0, 0};

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

static int log_entry_idgen = 0;

/* Allocate an empty log entry */
static BMLogEntry *bm_log_entry_create(BMLogEntryType type)
{
  BMLogEntry *entry = MEM_callocN(sizeof(BMLogEntry), __func__);

  entry->type = type;
  entry->id = log_entry_idgen++;

  if (type == LOG_ENTRY_PARTIAL) {
    entry->topo_modified_verts_pre = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->topo_modified_verts_post = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->topo_modified_edges_pre = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->topo_modified_edges_post = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->topo_modified_faces_pre = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);
    entry->topo_modified_faces_post = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);

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
      BLI_ghash_free(entry->topo_modified_verts_pre, NULL, NULL);
      BLI_ghash_free(entry->topo_modified_verts_post, NULL, NULL);
      BLI_ghash_free(entry->topo_modified_edges_pre, NULL, NULL);
      BLI_ghash_free(entry->topo_modified_edges_post, NULL, NULL);
      BLI_ghash_free(entry->topo_modified_faces_pre, NULL, NULL);
      BLI_ghash_free(entry->topo_modified_faces_post, NULL, NULL);

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
      fprintf(DEBUG_FILE, "BMLog refcount error\n");
      log->refcount = 0;
    }

    kill_log = !log->refcount;
  }

  bm_log_entry_free_direct(entry);

  if (kill_log) {
#ifdef PRINT_LOG_REF_COUNTING
    fprintf(DEBUG_FILE, "killing log! %p\n", log);
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

void BM_log_set_cd_offsets(BMLog *log, int cd_sculpt_vert)
{
  log->cd_sculpt_vert = cd_sculpt_vert;
}

void BM_log_set_bm(BMesh *bm, BMLog *log)
{
  log->bm = bm;
}

/* Allocate, initialize, and assign a new BMLog */
BMLog *BM_log_create(BMesh *bm, BMIdMap *idmap, int cd_sculpt_vert)
{
  BMLog *log = MEM_callocN(sizeof(*log), __func__);

  log->idmap = idmap;

#ifdef DEBUG_LOG_TO_FILE
  if (!DEBUG_FILE) {
    DEBUG_FILE = fopen("bmlog_debug.txt", "w");
  }
#endif

  BLI_rw_mutex_init(&log->lock);

  BM_log_set_cd_offsets(log, cd_sculpt_vert);

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
BMLog *BM_log_from_existing_entries_create(BMesh *bm, BMIdMap *idmap, BMLogEntry *entry)
{
  BMLog *log = BM_log_create(bm, idmap, -1);

  bm_log_from_existing_entries_create(bm, log, entry);

  return log;
}

BMLog *BM_log_unfreeze(BMesh *bm, BMLogEntry *entry)
{
  if (!entry || !entry->log) {
    return NULL;
  }

#if 0
  BMLogEntry* frozen = entry->log->frozen_full_mesh;
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

  fprintf(DEBUG_FILE, "==bmlog step==\n");

  while (first) {
    switch (first->type) {
      case LOG_ENTRY_FULL_MESH:
        fprintf(DEBUG_FILE, " ==full mesh copy==\n");
        break;
      case LOG_ENTRY_MESH_IDS:
        fprintf(DEBUG_FILE, "==element IDs snapshot\n");
        break;
      case LOG_ENTRY_PARTIAL:
        fprintf(DEBUG_FILE, "==modified: ");
        fprintf(DEBUG_FILE, "v: %d ", BLI_ghash_len(first->modified_verts));
        fprintf(DEBUG_FILE, "e: %d ", BLI_ghash_len(first->modified_edges));
        fprintf(DEBUG_FILE, "f: %d ", BLI_ghash_len(first->modified_faces));
        fprintf(DEBUG_FILE, "\n");
        fprintf(DEBUG_FILE, " topo_modified_pre:");
        fprintf(DEBUG_FILE, "v: %d ", BLI_ghash_len(first->topo_modified_verts_pre));
        fprintf(DEBUG_FILE, "e: %d ", BLI_ghash_len(first->topo_modified_edges_pre));
        fprintf(DEBUG_FILE, "f: %d ", BLI_ghash_len(first->topo_modified_faces_pre));
        fprintf(DEBUG_FILE, "\n");
        fprintf(DEBUG_FILE, " topo_modified_post:");
        fprintf(DEBUG_FILE, "v: %d ", BLI_ghash_len(first->topo_modified_verts_post));
        fprintf(DEBUG_FILE, "e: %d ", BLI_ghash_len(first->topo_modified_edges_post));
        fprintf(DEBUG_FILE, "f: %d ", BLI_ghash_len(first->topo_modified_faces_post));
        fprintf(DEBUG_FILE, "\n");
        break;
    }

    first = first->combined_next;
  }
}

/* Apply a consistent ordering to BMesh vertices */
void BM_log_mesh_elems_reorder(BMesh *bm, BMLog *log)
{
#if 0  // TODO: make sure no edge cases relying on this function still exist
  uint* varr;
  uint* farr;

  GHash* id_to_idx;

  BMIter bm_iter;
  BMVert* v;
  BMFace* f;

  uint i;

  /* Put all vertex IDs into an array */
  varr = MEM_mallocN(sizeof(int) * (size_t)bm->totvert, __func__);
  BM_ITER_MESH_INDEX(v, &bm_iter, bm, BM_VERTS_OF_MESH, i) {
    varr[i] = bm_log_vert_id_get(log, v);
  }

  /* Put all face IDs into an array */
  farr = MEM_mallocN(sizeof(int) * (size_t)bm->totface, __func__);
  BM_ITER_MESH_INDEX(f, &bm_iter, bm, BM_FACES_OF_MESH, i) {
    farr[i] = bm_log_face_id_get(log, f);
  }

  /* Create BMVert index remap array */
  id_to_idx = bm_log_compress_ids_to_indices(varr, (uint)bm->totvert);
  BM_ITER_MESH_INDEX(v, &bm_iter, bm, BM_VERTS_OF_MESH, i) {
    const uint id = bm_log_vert_id_get(log, v);
    const void* key = POINTER_FROM_UINT(id);
    const void* val = log_ghash_lookup(log, id_to_idx, key);
    varr[i] = POINTER_AS_UINT(val);
  }
  BLI_ghash_free(id_to_idx, NULL, NULL);

  /* Create BMFace index remap array */
  id_to_idx = bm_log_compress_ids_to_indices(farr, (uint)bm->totface);
  BM_ITER_MESH_INDEX(f, &bm_iter, bm, BM_FACES_OF_MESH, i) {
    const uint id = bm_log_face_id_get(log, f);
    const void* key = POINTER_FROM_UINT(id);
    const void* val = log_ghash_lookup(log, id_to_idx, key);
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
    fprintf(DEBUG_FILE, "no current entry; creating...\n");
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
      fprintf(DEBUG_FILE, "Customdata changed for undo\n");
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
    fprintf(DEBUG_FILE, "BMLog Error: log is dead\n");
    fflush(DEBUG_FILE);
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
    BMLogEntry* next;
    for (entry = entry->next; entry; entry = next) {
      next = entry->next;
      bm_log_entry_free(entry);
      BLI_freelinkN(&log->entries, entry);
    }
  }
#endif

  /* Create and append the new entry */
  entry = bm_log_entry_create(type);

  if (combine_with_last) {
    bm_log_message(" == add subentry %d ==", entry->id);
  }

  if (!last_entry || last_entry == log->current_entry) {
    BLI_addtail(&log->entries, entry);
  }

  entry->log = log;

  namestack_push();

  bm_log_addref(log);

  namestack_pop();

  if (combine_with_last && log->current_entry) {
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
  CustomData_MeshMasks cd_mask_extra = {CD_MASK_DYNTOPO_VERT | CD_MASK_SHAPEKEY, 0, 0, 0, 0};

  int shapenr = bm->shapenr;

  BM_mesh_clear(bm);
  BM_mesh_bm_from_me(NULL,
                     bm,
                     entry->full_copy_mesh,  // note we stored shapekeys as customdata layers,
                                             // that's why the shapekey params are false
                     (&(struct BMeshFromMeshParams){.calc_face_normal = false,
                                                    .add_key_index = false,
                                                    .use_shapekey = false,
                                                    .create_shapekey_layers = false,
                                                    .cd_mask_extra = cd_mask_extra,
                                                    .copy_temp_cdlayers = true,
                                                    .ignore_id_layers = false}));

  bm->shapenr = shapenr;
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

      if (!reported && (BMElem *)bm_log_elem_from_id(log, id) != elem) {
        fprintf(DEBUG_FILE, "IDMap error for elem type %d\n", elem->head.htype);
        fprintf(DEBUG_FILE, "  further errors suppressed\n");
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
      fprintf(DEBUG_FILE, "mesh doesn't have ids for elem type %d\n", type);
      continue;
    }

    if (idmap->elemtots[type] != tots[i]) {
      fprintf(DEBUG_FILE, "idmap elem count mismatch error");
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
      bm_log_assign_id(log, elem, (uint)map[j], true);

      // deal with loops
      if (type == BM_FACE && cd_loop_id >= 0) {
        BMFace *f = (BMFace *)elem;
        BMLoop *l = f->l_first;

        do {
          bm_log_assign_id(log, (BMElem *)l, (uint)lmap[loopi], true);

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
      fprintf(DEBUG_FILE, "mesh doesn't have ids for elem type %d\n", type);
      continue;
    }

    if (idmap->elemtots[type] != tots[i]) {
      fprintf(DEBUG_FILE, "idmap elem count mismatch error");
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

      bm_log_assign_id(log, elem, (uint)map[j], true);
      map[j] = id;

      // deal with loops
      if (type == BM_FACE && cd_loop_id >= 0) {
        BMFace *f = (BMFace *)elem;
        BMLoop *l = f->l_first;

        do {
          int id2 = BM_ELEM_CD_GET_INT(l, cd_loop_id);

          bm_log_assign_id(log, (BMElem *)l, (uint)lmap[loopi], true);
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
  CustomData_MeshMasks cd_mask_extra = {CD_MASK_DYNTOPO_VERT | CD_MASK_SHAPEKEY, 0, 0, 0, 0};

  BMLogEntry tmp = {0};

  bm_log_full_mesh_intern(bm, log, &tmp);

  int shapenr = bm->shapenr;

  BM_mesh_clear(bm);
  BM_mesh_bm_from_me(NULL,
                     bm,
                     entry->full_copy_mesh,  // note we stored shapekeys as customdata layers,
                                             // that's why the shapekey params are false
                     (&(struct BMeshFromMeshParams){.calc_face_normal = false,
                                                    .add_key_index = false,
                                                    .use_shapekey = false,
                                                    .create_shapekey_layers = false,
                                                    .cd_mask_extra = cd_mask_extra,
                                                    .copy_temp_cdlayers = true,
                                                    .ignore_id_layers = false}));

  bm->shapenr = shapenr;

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
static void bm_log_undo_intern(BMesh *bm, BMLog *log, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  log->bm = bm;

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  BM_idmap_check_attributes(log->idmap);

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

    BM_idmap_check_attributes(log->idmap);
    return;
  }

  // XXX
  bm_update_idmap_cdlayers(bm);

  /* Delete added faces and verts */
  bm_log_faces_unmake_pre(bm, log, entry->topo_modified_faces_post, entry, callbacks);
  bm_log_edges_unmake_pre(bm, log, entry->topo_modified_edges_post, entry, callbacks);
  bm_log_verts_unmake_pre(bm, log, entry->topo_modified_verts_post, entry, callbacks);

  bm_log_faces_unmake(bm, log, entry->topo_modified_faces_post, entry, callbacks);
  bm_log_edges_unmake(bm, log, entry->topo_modified_edges_post, entry, callbacks);
  bm_log_verts_unmake(bm, log, entry->topo_modified_verts_post, entry, callbacks);

  /* Restore deleted verts and faces */
  bm_log_verts_restore(bm, log, entry->topo_modified_verts_pre, entry, callbacks);
  bm_log_edges_restore(bm, log, entry->topo_modified_edges_pre, entry, callbacks);
  bm_log_faces_restore(bm, log, entry->topo_modified_faces_pre, entry, callbacks);

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

void BM_log_undo_single(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks)
{
  BMLogEntry *entry = log->current_entry;
  log->bm = bm;

  if (!entry) {
    return;
  }

  BMLogEntry *preventry = entry->prev;

  bm_log_undo_intern(bm, log, entry, callbacks);
  entry = entry->combined_prev;

  log->current_entry = entry ? entry : preventry;
}

void BM_log_undo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks)
{
  BMLogEntry *entry = log->current_entry;
  log->bm = bm;

  if (!entry) {
    return;
  }

  BMLogEntry *preventry = entry->prev;

  while (entry) {
    bm_log_undo_intern(bm, log, entry, callbacks);
    entry = entry->combined_prev;
  }

  log->current_entry = preventry;
}

/* Redo one BMLogEntry
 *
 * Has no effect if there's nothing left to redo */
static void bm_log_redo_intern(BMesh *bm, BMLog *log, BMLogEntry *entry, BMLogCallbacks *callbacks)
{
  BM_idmap_check_attributes(log->idmap);

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

    BM_idmap_check_attributes(log->idmap);
    return;
  }

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  /* Re-delete previously deleted faces and verts */
  bm_log_faces_unmake_pre(bm, log, entry->topo_modified_faces_pre, entry, callbacks);
  bm_log_edges_unmake_pre(bm, log, entry->topo_modified_edges_pre, entry, callbacks);
  bm_log_verts_unmake_pre(bm, log, entry->topo_modified_verts_pre, entry, callbacks);

  bm_log_faces_unmake(bm, log, entry->topo_modified_faces_pre, entry, callbacks);
  bm_log_edges_unmake(bm, log, entry->topo_modified_edges_pre, entry, callbacks);
  bm_log_verts_unmake(bm, log, entry->topo_modified_verts_pre, entry, callbacks);

  /* Restore previously added verts and faces */
  bm_log_verts_restore(bm, log, entry->topo_modified_verts_post, entry, callbacks);
  bm_log_edges_restore(bm, log, entry->topo_modified_edges_post, entry, callbacks);
  bm_log_faces_restore(bm, log, entry->topo_modified_faces_post, entry, callbacks);

  /* Restore vertex coordinates, mask, and hflag */
  bm_log_vert_values_swap(bm, log, entry->modified_verts, entry, callbacks);
  bm_log_edge_values_swap(bm, log, entry->modified_edges, entry, callbacks);
  bm_log_face_values_swap(log, entry->modified_faces, entry, callbacks);
}

void bm_log_vert_copydata(BMLogEntry *entry, BMLogVert *dst, BMLogVert *src, bool free)
{
  if (free && dst->customdata) {
    BLI_mempool_free(entry->vdata.pool, dst->customdata);
  }

  copy_v3_v3(dst->co, src->co);
  copy_v3_v3(dst->no, src->no);
  dst->hflag = src->hflag;

  if (free) {
    BLI_mempool_free(entry->pool_verts, src);
  }
}

/* does not copy id */
void bm_log_edge_copydata(BMLogEntry *entry, BMLogEdge *dst, BMLogEdge *src, bool free)
{
  if (free && dst->customdata) {
    BLI_mempool_free(entry->edata.pool, dst->customdata);
  }

  dst->hflag = src->hflag;
  dst->customdata = src->customdata;

  if (free) {
    BLI_mempool_free(entry->pool_edges, src);
  }
}

void bm_log_face_copydata(BMLogEntry *entry, BMLogFace *dst, BMLogFace *src, bool free)
{
  if (free && dst->customdata) {
    BLI_mempool_free(entry->pdata.pool, dst->customdata);

    if (dst->len == src->len) {
      for (uint i = 0; i < dst->len; i++) {
        if (dst->customdata[i]) {
          BLI_mempool_free(entry->ldata.pool, dst->customdata[i]);
        }
      }
    }
  }

  if (dst->len != src->len) {
    fprintf(stderr, "%s: mismatched face sizes!\n", __func__);
  }

  if (dst->len == src->len) {
    dst->customdata_f = src->customdata_f;

    for (uint i = 0; i < src->len; i++) {
      dst->customdata[i] = src->customdata[i];
    }
  }

  dst->hflag = src->hflag;
  dst->mat_nr = src->mat_nr;
  copy_v3_v3(dst->no, src->no);

  if (free) {
    BLI_mempool_free(entry->pool_faces, src);
  }
}

BMLogEntry *BM_log_entry_prev(BMLogEntry *entry)
{
  return entry->prev;
}

BMLogEntry *BM_log_entry_next(BMLogEntry *entry)
{
  return entry->next;
}

void BM_log_redo(BMesh *bm, BMLog *log, BMLogCallbacks *callbacks)
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
    bm_log_redo_intern(bm, log, entry, callbacks);
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
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)v);

  BMLogEntry *entry = log->current_entry;
  BMLogVert *lv;
  uint v_id = (uint)bm_log_vert_id_get(log, v);
  void *key = POINTER_FROM_UINT(v_id);
  void **val_p;

  // LOGPRINT("key %d\n", (int)key);

  if (!log_ghash_ensure_p(log, entry->modified_verts, key, &val_p)) {
    lv = bm_log_vert_alloc(log, v, true);
    *val_p = lv;
  }

  bm_logstack_pop();
  return;

  /* Find or create the BMLogVert entry */
  if ((lv = log_ghash_lookup(log, entry->topo_modified_verts_pre, key))) {
    bm_log_vert_bmvert_copy(log, entry, lv, v, log_customdata);
  }
  else if ((lv = log_ghash_lookup(log, entry->topo_modified_verts_post, key))) {
    bm_log_vert_bmvert_copy(log, entry, lv, v, log_customdata);
  }
  else if (!log_ghash_ensure_p(log, entry->modified_verts, key, &val_p)) {
    lv = bm_log_vert_alloc(log, v, true);
    *val_p = lv;
  }

  bm_logstack_pop();
}

void BM_log_edge_before_modified(BMLog *log, BMEdge *e, bool log_customdata)
{
  BM_idmap_check_assign(log->idmap, (BMElem *)e);

  BMLogEntry *entry = log->current_entry;
  BMLogEdge *le;
  uint e_id = (uint)bm_log_edge_id_get(log, e);
  void *key = POINTER_FROM_UINT(e_id);
  void **val_p;

  /* Find or create the BMLogVert entry */
  if ((le = log_ghash_lookup(log, entry->topo_modified_edges_pre, key))) {
    bm_log_edge_bmedge_copy(log, entry, le, e, log_customdata);
  }
  else if (!log_ghash_ensure_p(log, entry->modified_edges, key, &val_p)) {
    le = bm_log_edge_alloc(log, e, true);
    *val_p = le;
  }
}

/* Log a new edge as added to the BMesh
 */
void _BM_log_edge_added(BMLog *log, BMEdge *e BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  _BM_log_edge_post(log, e BMLOG_DEBUG_ARGS_VALUES);

  bm_logstack_pop();
}

/* Log a new vertex as added to the BMesh
 */
void _BM_log_vert_added(BMLog *log, BMVert *v, const int cd_vert_mask_offset BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  _BM_log_vert_post(log, v BMLOG_DEBUG_ARGS_VALUES);

  bm_logstack_pop();
}

/* Log a face before it is modified
 *
 * We always assume face has been added before
 */
void _BM_log_face_modified(BMLog *log, BMFace *f BMLOG_DEBUG_ARGS)
{
  BM_idmap_check_assign(log->idmap, (BMElem *)f);

  BMLogFace *lf;
  uint f_id = (uint)bm_log_face_id_get(log, f);
  void *key = POINTER_FROM_UINT(f_id);

  // LOGPRINT("key %d\n", (int)key);

  lf = bm_log_face_alloc(log, f);

#ifdef BM_LOG_TRACE
  lf->head.func = func;
  lf->head.line = line;
#endif

  log_ghash_insert(log, log->current_entry->modified_faces, key, lf);
  bm_log_face_customdata(log->bm, log, f, lf);
}

bool BM_log_has_vert(BMLog *log, BMVert *v)
{
  BM_idmap_check_assign(log->idmap, (BMElem *)v);

  int id = bm_log_vert_id_get(log, v);

  bool ret = BLI_ghash_haskey(log->current_entry->topo_modified_verts_pre, POINTER_FROM_INT(id));
  ret = ret ||
        BLI_ghash_haskey(log->current_entry->topo_modified_verts_post, POINTER_FROM_INT(id));
  ret = ret || BLI_ghash_haskey(log->current_entry->modified_verts, POINTER_FROM_INT(id));

  return ret;
}

bool BM_log_has_edge(BMLog *log, BMEdge *e)
{
  BM_idmap_check_assign(log->idmap, (BMElem *)e);
  int id = bm_log_edge_id_get(log, e);

  bool ret = BLI_ghash_haskey(log->current_entry->topo_modified_edges_pre, POINTER_FROM_INT(id));
  ret = ret ||
        BLI_ghash_haskey(log->current_entry->topo_modified_edges_post, POINTER_FROM_INT(id));
  ret = ret || BLI_ghash_haskey(log->current_entry->modified_edges, POINTER_FROM_INT(id));

  return ret;
}

bool BM_log_has_face(BMLog *log, BMFace *f)
{
  BM_idmap_check_assign(log->idmap, (BMElem *)f);
  int id = bm_log_face_id_get(log, f);

  bool ret = BLI_ghash_haskey(log->current_entry->modified_faces, POINTER_FROM_INT(id));
  ret = ret ||
        BLI_ghash_haskey(log->current_entry->topo_modified_faces_post, POINTER_FROM_INT(id));
  ret = ret || BLI_ghash_haskey(log->current_entry->topo_modified_faces_pre, POINTER_FROM_INT(id));

  return ret;
}

/* Log a new face as added to the BMesh
 *
 * The new face gets a unique ID assigned. It is then added to a map
 * of added faces, with the key being its ID and the value containing
 * everything needed to reconstruct that face.
 */
void _BM_log_face_added(BMLog *log, BMFace *f BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  _BM_log_face_post(log, f BMLOG_DEBUG_ARGS_VALUES);

  bm_logstack_pop();
}

void _BM_log_face_pre(BMLog *log, BMFace *f BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)f);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_face_id_get(log, f);
  void *key = POINTER_FROM_UINT(f_id);

  if (log_ghash_remove(entry->topo_modified_faces_post, key, entry->pool_faces)) {
    // do nothing
    bm_logstack_pop();
    return;
  }

  void **val = NULL;

  if (!BLI_ghash_ensure_p(entry->topo_modified_faces_pre, key, &val)) {
    BMLogFace *lf;

    LOGPRINT(entry, "key %d\n", POINTER_AS_UINT(key));

    lf = bm_log_face_alloc(log, f);
    bm_log_face_customdata(log->bm, log, f, lf);

#ifdef BM_LOG_TRACE
    lf->head.func = func;
    lf->head.line = line;
#endif

    BMLogFace *old = BLI_ghash_popkey(entry->modified_faces, key, NULL);
    if (old) {
      bm_log_face_copydata(entry, lf, old, true);
    }

    *val = (void *)lf;
  }

  bm_logstack_pop();
}

void _BM_log_face_post(BMLog *log, BMFace *f BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)f);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_face_id_get(log, f);
  void *key = POINTER_FROM_UINT(f_id);
  BMLogFace *lf;

  LOGPRINT(entry, "key %d\n", POINTER_AS_UINT(key));

  lf = bm_log_face_alloc(log, f);
  bm_log_face_customdata(log->bm, log, f, lf);

#ifdef BM_LOG_TRACE
  lf->head.func = func;
  lf->head.line = line;
#endif

  void **val = NULL;

  if (BLI_ghash_ensure_p(entry->topo_modified_faces_post, key, &val)) {
    BMLogFace *lf_old = (BMLogFace *)*val;
    *lf_old = *lf;

    BLI_mempool_free(entry->pool_faces, lf);
  }
  else {
    *val = (void *)lf;

    BMLogFace *old = BLI_ghash_popkey(entry->modified_faces, key, NULL);
    if (old) {
      bm_log_face_copydata(entry, lf, old, true);
    }
  }

  bm_logstack_pop();
}

void _BM_log_edge_pre(BMLog *log, BMEdge *e BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)e);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_edge_id_get(log, e);
  void *key = POINTER_FROM_UINT(f_id);

  void **val = NULL;

  if (log_ghash_remove(entry->topo_modified_edges_post, key, entry->pool_edges)) {
    // do nothing
    bm_logstack_pop();
    return;
  }

  if (BLI_ghash_haskey(entry->topo_modified_edges_post, key)) {
    // do nothing
    bm_logstack_pop();
    return;
  }

  if (!BLI_ghash_ensure_p(entry->topo_modified_edges_pre, key, &val)) {
    BMLogEdge *le;

    LOGPRINT(entry, "key %d\n", POINTER_AS_UINT(key));

    le = bm_log_edge_alloc(log, e, true);

    BMLogEdge *old = BLI_ghash_popkey(entry->modified_edges, key, NULL);
    if (old) {
      bm_log_edge_copydata(entry, le, old, true);
    }

#ifdef BM_LOG_TRACE
    le->head.func = func;
    le->head.line = line;
#endif

    *val = (void *)le;
  }

  bm_logstack_pop();
}

void _BM_log_edge_post(BMLog *log, BMEdge *e BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)e);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_edge_id_get(log, e);
  void *key = POINTER_FROM_UINT(f_id);
  BMLogEdge *le;

  LOGPRINT(entry, "key %d\n", POINTER_AS_UINT(key));

  le = bm_log_edge_alloc(log, e, true);

#ifdef BM_LOG_TRACE
  le->head.func = func;
  le->head.line = line;
#endif

  void **val = NULL;

  log_ghash_remove(entry->modified_edges, key, entry->pool_edges);

  if (BLI_ghash_ensure_p(entry->topo_modified_edges_post, key, &val)) {
    BMLogEdge *le_old = (BMLogEdge *)*val;
    *le_old = *le;

    if (le_old->customdata) {
      BLI_mempool_free(entry->edata.pool, le_old->customdata);
    }
    BLI_mempool_free(entry->pool_edges, le);
  }
  else {
    *val = (void *)le;
  }

  bm_logstack_pop();
}

void _BM_log_vert_pre(BMLog *log, BMVert *v BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)v);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_vert_id_get(log, v);
  void *key = POINTER_FROM_UINT(f_id);

  void **val = NULL;

  if (log_ghash_remove(entry->topo_modified_verts_post, key, entry->pool_verts)) {
    // do nothing
    bm_logstack_pop();
    return;
  }

  if (!BLI_ghash_ensure_p(entry->topo_modified_verts_pre, key, &val)) {
    BMLogVert *lv;

    LOGPRINT(entry, "key %d\n", POINTER_AS_UINT(key));

    lv = bm_log_vert_alloc(log, v, true);

    BMLogVert *old = (BMLogVert *)BLI_ghash_popkey(entry->modified_verts, key, keyfree);

    if (old) {
      bm_log_vert_copydata(entry, lv, old, true);
    }

#ifdef BM_LOG_TRACE
    lv->head.line = line;
    lv->head.func = func;
#endif

    *val = (void *)lv;
  }

  bm_logstack_pop();
}

void _BM_log_vert_post(BMLog *log, BMVert *v BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)v);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_vert_id_get(log, v);
  void *key = POINTER_FROM_UINT(f_id);
  BMLogVert *lv;

  LOGPRINT(entry, "key %d\n", POINTER_AS_UINT(key));

  lv = bm_log_vert_alloc(log, v, true);

  void **val = NULL;

  if (BLI_ghash_ensure_p(entry->topo_modified_verts_post, key, &val)) {
    BMLogVert *lv_old = (BMLogVert *)*val;
    *lv_old = *lv;

#ifdef BM_LOG_TRACE
    lv_old->head.func = func;
    lv_old->head.line = line;
#endif

    if (lv_old->customdata) {
      BLI_mempool_free(entry->vdata.pool, lv_old->customdata);
    }

    BLI_mempool_free(entry->pool_verts, lv);
  }
  else {
    *val = (void *)lv;

    BMLogVert *old = (BMLogVert *)BLI_ghash_popkey(entry->modified_verts, key, keyfree);

    if (old) {
      bm_log_vert_copydata(entry, lv, old, true);

#ifdef BM_LOG_TRACE
      lv->head.func = func;
      lv->head.line = line;
#endif
    }
  }

  bm_logstack_pop();
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

void _BM_log_vert_removed(BMLog *log, BMVert *v, int UNUSED(cd_vert_mask_offset) BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)v);

  if (bm_log_vert_from_id(log, (uint)bm_log_vert_id_get(log, v)) != v) {
    fprintf(DEBUG_FILE, "%s: idmap error\n", __func__);
    bm_logstack_pop();
    return;
  }

  _BM_log_vert_pre(log, v BMLOG_DEBUG_ARGS_VALUES);

  bm_logstack_pop();
}

/**
Splits e and logs the new edge and vertex.
e is assigned a new ID.
*/
BMVert *BM_log_edge_split_do(BMLog *log, BMEdge *e, BMVert *v, BMEdge **newe, float t)
{
  bm_logstack_push();

  bm_log_message("edge split");

  BM_idmap_check_assign(log->idmap, (BMElem *)e->v1);
  BM_idmap_check_assign(log->idmap, (BMElem *)e->v2);
  BM_idmap_check_assign(log->idmap, (BMElem *)e);

  BMEdge *tmp = NULL;
  if (!newe) {
    newe = &tmp;
  }

  BM_log_edge_pre(log, e);
  BMVert *newv = BM_edge_split(log->bm, e, v, newe, t);

  BM_idmap_alloc(log->idmap, (BMElem *)newv);
  BM_idmap_alloc(log->idmap, (BMElem *)*newe);

  BMIter iter;
  BMLoop *l;
  BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
    BM_idmap_check_assign(log->idmap, (BMElem *)l->e);
    BM_idmap_check_assign(log->idmap, (BMElem *)l->f);
  }

  BM_log_vert_added(log, newv, -1);

  BM_log_edge_post(log, e);
  BM_log_edge_post(log, *newe);

  bm_logstack_pop();

  return newv;

#if 0
  BMEdge* tmp = NULL;
  if (!newe) {
    newe = &tmp;
  }

  BMesh* bm = log->bm;

  int eid0 = bm_log_vert_id_get(log, e);

  bm_log_message("edge split");
  bm_log_message(" esplit: remove edge %d", eid0);
  BM_log_edge_removed(log, e);

  BMVert* v1 = e->v1, * v2 = e->v2;
  uint id1 = (uint)bm_log_vert_id_get(log, v1);
  uint id2 = (uint)bm_log_vert_id_get(log, v2);

  bm_log_message(" esplit: split edge %d (v1=%d v2=%d)", eid0, id1, id2);
  BMVert* newv = BM_edge_split(log->bm, e, v, newe, t);

  uint id3 = (uint)bm_log_vert_id_get(log, newv);
  uint nid = (uint)bm_log_vert_id_get(log, (*newe));

  // get a new id
#  ifndef WITH_BM_ID_FREELIST
  uint id = range_tree_uint_take_any(log->bm->idmap.idtree);
  bm_log_free_id(log, (BMElem*)e, true);
  bm_log_assign_id(log, (BMElem*)e, id, true);
#  else
  bm_log_free_id(log, (BMElem*)e, true);
  bm_log_alloc_id(log, (BMElem*)e);

  uint id = bm_log_vert_id_get(log, e);
#  endif

  bm_log_message(" esplit: add new vert %d", id3);
  BM_log_vert_added(log, newv, -1);

  bm_log_message(" esplit: add old edge (with new id %d)", id);
  BM_log_edge_added(log, e);

  bm_log_message(" esplit: add new edge %d", nid);
  BM_log_edge_added(log, *newe);

  bm_logstack_pop();

  return newv;
#endif
}

void _BM_log_edge_removed(BMLog *log, BMEdge *e BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)e);

  if (bm_log_edge_from_id(log, (uint)bm_log_edge_id_get(log, e)) != e) {
    fprintf(DEBUG_FILE, "%s: idmap error\n", __func__);
    bm_logstack_pop();
    return;
  }

  BMLogEntry *entry = log->current_entry;
  uint e_id = (uint)bm_log_edge_id_get(log, e);
  void *key = POINTER_FROM_UINT(e_id);

  bool ok = !BLI_ghash_haskey(entry->topo_modified_edges_pre, key);
  bool ok2 = false;

  ok2 |= !BLI_ghash_remove(entry->topo_modified_edges_post, key, NULL, NULL);

  ok = ok && ok2;

  if (ok) {
    _BM_log_edge_pre(log, e BMLOG_DEBUG_ARGS_VALUES);
    bm_logstack_pop();
    return;
  }

  bm_logstack_pop();
}

/* Log a face as removed from the BMesh
 */
void _BM_log_face_removed(BMLog *log, BMFace *f BMLOG_DEBUG_ARGS)
{
  bm_logstack_push();

  BM_idmap_check_assign(log->idmap, (BMElem *)f);

  BMLogEntry *entry = log->current_entry;
  uint f_id = (uint)bm_log_face_id_get(log, f);
  void *key = POINTER_FROM_UINT(f_id);

  bool ok = !BLI_ghash_haskey(entry->topo_modified_faces_pre, key);
  bool ok2 = false;

  ok2 |= !BLI_ghash_remove(entry->topo_modified_faces_post, key, NULL, NULL);

  ok = ok && ok2;

  if (ok) {
    _BM_log_face_pre(log, f BMLOG_DEBUG_ARGS_VALUES);
    bm_logstack_pop();
    return;
  }

  bm_logstack_pop();
}

/* Log all vertices/faces in the BMesh as added */
void BM_log_all_added(BMesh *bm, BMLog *log)
{
  BM_log_entry_add_ex(bm, log, true);

  BMIter bm_iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  /* avoid unnecessary resizing on initialization */

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
    add |= BLI_ghash_len(entry->modified_verts) > 0;
    add |= BLI_ghash_len(entry->modified_faces) > 0;
    add |= BLI_ghash_len(entry->topo_modified_verts_post) > 0;
    add |= BLI_ghash_len(entry->topo_modified_verts_pre) > 0;
    add |= BLI_ghash_len(entry->topo_modified_edges_post) > 0;
    add |= BLI_ghash_len(entry->topo_modified_edges_pre) > 0;
    add |= BLI_ghash_len(entry->topo_modified_faces_post) > 0;
    add |= BLI_ghash_len(entry->topo_modified_faces_pre) > 0;
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
void bm_log_print(const BMLog* log, const char* description)
{
  const BMLogEntry* entry;
  const char* current = " <-- current";
  int i;

  fprintf(DEBUG_FILE, "%s:\n", description);
  fprintf(DEBUG_FILE, "    % 2d: [ initial ]%s\n", 0, (!log->current_entry) ? current : "");
  for (entry = log->entries.first, i = 1; entry; entry = entry->next, i++) {
    fprintf(DEBUG_FILE, "    % 2d: [%p]%s\n", i, entry, (entry == log->current_entry) ? current : "");
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

    ret += BLI_memarena_size(entry->arena);

    if (BLI_memarena_size(entry->arena)) {
      fprintf(DEBUG_FILE, "%d\n", BLI_memarena_size(entry->arena));
    }
    // estimate ghash memory usage
    ret += (int)BLI_ghash_len(entry->modified_verts) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->modified_edges) * (int)sizeof(void *) * 4;
    ret += (int)BLI_ghash_len(entry->modified_faces) * (int)sizeof(void *) * 4;
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

BMesh *BM_mesh_copy_ex(BMesh *bm_old, struct BMeshCreateParams *params);

int type_idx_map[] = {
    0,  // 0
    0,  // 1 BM_VERT
    1,  // 2 BM_EDGE
    0,  // 3
    2,  // 4 BM_LOOP
    0,  // 5
    0,  // 6
    0,  // 7
    3,  // 8 BM_FACE
};

static GHash *bm_clone_ghash(BMLogEntry *entry, GHash *ghash, int type)
{
  GHash *ghash2 = BLI_ghash_new(logkey_hash, logkey_cmp, __func__);

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, ghash) {
    void *key = BLI_ghashIterator_getKey(&gh_iter);
    void *newval = NULL;

    switch (type) {
      case BM_VERT: {
        BMLogVert *lv = BLI_ghashIterator_getValue(&gh_iter);
        BMLogVert *lv2 = BLI_mempool_alloc(entry->pool_verts);

        *lv2 = *lv;
        if (lv2->customdata) {
          void *cdata = BLI_mempool_alloc(entry->vdata.pool);
          memcpy(cdata, lv->customdata, entry->vdata.totsize);
          lv2->customdata = cdata;
        }

        newval = (void *)lv2;
        break;
      }
      case BM_EDGE: {
        BMLogEdge *le = BLI_ghashIterator_getValue(&gh_iter);
        BMLogEdge *le2 = BLI_mempool_alloc(entry->pool_edges);

        *le2 = *le;
        if (le2->customdata) {
          void *cdata = BLI_mempool_alloc(entry->edata.pool);
          memcpy(cdata, le->customdata, entry->edata.totsize);
          le2->customdata = cdata;
        }

        newval = (void *)le2;
        break;
      }
      case BM_FACE: {
        BMLogFace *lf = BLI_ghashIterator_getValue(&gh_iter);
        BMLogFace *lf2 = BLI_mempool_alloc(entry->pool_faces);

        *lf2 = *lf;
        if (lf2->customdata_f) {
          void *cdata = BLI_mempool_alloc(entry->pdata.pool);
          memcpy(cdata, lf->customdata_f, entry->pdata.totsize);
          lf2->customdata_f = cdata;
        }

        for (int i = 0; i < lf->len; i++) {
          if (lf->customdata[i]) {
            lf2->customdata[i] = BLI_mempool_alloc(entry->ldata.pool);
            memcpy(lf2->customdata[i], lf->customdata[i], entry->ldata.totsize);
          }
        }

        newval = (void *)lf2;
        break;
      }
    }

    BLI_ghash_insert(ghash2, key, newval);
  }

  return ghash2;
}

static BMLogEntry *bm_log_entry_clone_intern(BMLogEntry *entry, BMLog *newlog)
{
  BMLogEntry *newentry = MEM_callocN(sizeof(*entry), "BMLogEntry cloned");

  *newentry = *entry;

  newentry->combined_next = newentry->combined_prev = newentry->next = newentry->prev = NULL;

  if (entry->type == LOG_ENTRY_PARTIAL) {
    newentry->pool_verts = BLI_mempool_create(sizeof(BMLogVert), 0, 64, BLI_MEMPOOL_NOP);
    newentry->pool_edges = BLI_mempool_create(sizeof(BMLogEdge), 0, 64, BLI_MEMPOOL_NOP);
    newentry->pool_faces = BLI_mempool_create(sizeof(BMLogFace), 0, 64, BLI_MEMPOOL_NOP);

    CustomData *cdata = &newentry->vdata;
    for (int i = 0; i < 4; i++) {
      cdata->pool = NULL;
      CustomData_bmesh_init_pool(cdata, 0, 1 << i);
    }

    entry->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "bmlog arena");

    newentry->modified_verts = bm_clone_ghash(newentry, entry->modified_verts, BM_VERT);
    newentry->topo_modified_verts_pre = bm_clone_ghash(
        newentry, entry->topo_modified_verts_pre, BM_VERT);
    newentry->topo_modified_verts_post = bm_clone_ghash(
        newentry, entry->topo_modified_verts_post, BM_VERT);

    newentry->modified_edges = bm_clone_ghash(newentry, entry->modified_edges, BM_EDGE);
    newentry->topo_modified_edges_pre = bm_clone_ghash(
        newentry, entry->topo_modified_edges_pre, BM_EDGE);
    newentry->topo_modified_edges_post = bm_clone_ghash(
        newentry, entry->topo_modified_edges_post, BM_EDGE);

    newentry->modified_faces = bm_clone_ghash(newentry, entry->modified_faces, BM_FACE);
    newentry->topo_modified_faces_pre = bm_clone_ghash(
        newentry, entry->topo_modified_faces_pre, BM_FACE);
    newentry->topo_modified_faces_post = bm_clone_ghash(
        newentry, entry->topo_modified_faces_post, BM_FACE);

    newentry->log = newlog;
  }

  return newentry;
}

static BMLogEntry *bm_log_entry_clone(BMLogEntry *entry, BMLog *newlog)
{
  BMLogEntry *cur = entry;
  BMLogEntry *ret = NULL;
  BMLogEntry *last = NULL;

  while (cur) {
    BMLogEntry *cpy = bm_log_entry_clone_intern(cur, newlog);

    if (!ret) {
      ret = cpy;
    }

    if (last) {
      last->combined_prev = cpy;
      cpy->combined_next = last;
    }

    last = cpy;
    cur = cur->combined_prev;
  }

  return ret;
}

#include <stdarg.h>

static void debuglog(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

static const char *elem_type_to_str(int type)
{
  switch (type) {
    case BM_VERT:
      return "vertex";
    case BM_EDGE:
      return "edge";
    case BM_LOOP:
      return "loop";
    case BM_FACE:
      return "face";
    default:
      return "(error)";
  }
}

static bool check_log_elem(BMesh *bm, BMLog *newlog, int id, int type, bool expected)
{
  /* id should be in mesh and of right type */
  BMElem *elem = bm_log_elem_from_id(newlog, id);

  if (!!elem != expected) {
    debuglog("%s: Missing %s %d\n", __func__, elem_type_to_str(type), id);
    return false;
  }
  else if (elem && (elem->head.htype == type) != expected) {
    debuglog("%s: Expected %s at id %d; got %s instead\n",
             __func__,
             elem_type_to_str(type),
             id,
             elem_type_to_str(elem->head.htype));
    return false;
  }

  return true;
}

ATTR_NO_OPT static bool bm_check_ghash_set(
    GHash *ghashes[4], BMesh *bm, BMLog *newlog, BMLogEntry *entry, bool shouldExist)
{
  bool ok = true;

  for (int i = 0; i < 4; i++) {
    if (!ghashes[i]) {
      continue;
    }

    int type = 1 << i;
    GHashIterator gh_iter;

    GHASH_ITER (gh_iter, ghashes[i]) {
      void *key = BLI_ghashIterator_getKey(&gh_iter);
      uint id = POINTER_AS_UINT(key);

      if (!check_log_elem(bm, newlog, id, type, shouldExist)) {
        ok = false;
        continue;
      }

      if (type == BM_EDGE) {
        BMLogEdge *le = (BMLogEdge *)BLI_ghashIterator_getValue(&gh_iter);

        ok = ok && check_log_elem(bm, newlog, le->v1, BM_VERT, shouldExist);
      }
      else if (type == BM_FACE) {
        BMLogFace *lf = (BMLogFace *)BLI_ghashIterator_getValue(&gh_iter);

        for (int i = 0; i < lf->len; i++) {
          ok = ok && check_log_elem(bm, newlog, lf->v_ids[i], BM_VERT, shouldExist);
        }
      }
    }
  }

  return ok;
}

ATTR_NO_OPT static bool bm_log_validate_intern(
    BMesh *bm, BMLog *newlog, BMLogEntry *srcEntry, bool is_applied, bool do_apply)
{
  bool precopy = do_apply;

  if (srcEntry->type != LOG_ENTRY_PARTIAL) {
    printf("%s: not a partial log entry!\n", __func__);
    return true;
  }

  BMLogEntry *entry = precopy ? bm_log_entry_clone(srcEntry, newlog) : srcEntry;
  bool ok = true;

  GHash *ghashes1[] = {entry->topo_modified_verts_pre,
                       entry->topo_modified_edges_pre,
                       NULL,
                       entry->topo_modified_faces_pre};

  GHash *ghashes2[] = {entry->topo_modified_verts_post,
                       entry->topo_modified_edges_post,
                       NULL,
                       entry->topo_modified_faces_post};

  if (!is_applied) {
    ok = ok && bm_check_ghash_set(ghashes2, bm, newlog, entry, true);
  }
  else {
    ok = ok && bm_check_ghash_set(ghashes1, bm, newlog, entry, true);
  }

  int iters[4] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, -1, BM_FACES_OF_MESH};

  for (int i = 0; i < 4; i++) {
    // int type = 1 << i;
    BMIter iter;
    BMElem *elem;

    if (i == 2) {  // skip loops
      continue;
    }

    BM_ITER_MESH (elem, &iter, bm, iters[i]) {
      uint id = bm_log_elem_id_get(newlog, elem);
      void *key = POINTER_FROM_UINT(id);

      bool exist1 = BLI_ghash_haskey(ghashes1[i], POINTER_FROM_UINT(id));
      bool exist2 = BLI_ghash_haskey(ghashes2[i], POINTER_FROM_UINT(id));

      for (int j = 0; j < 4; j++) {
        if (j != i && ghashes1[j] && BLI_ghash_haskey(ghashes1[j], key) && exist1) {
          int type1 = 1 << i;
          int type2 = 1 << j;
          debuglog("pre:  id %d used by multiple element types: %s and %s\n",
                   elem_type_to_str(type1),
                   elem_type_to_str(type2));
        }
        if (j != i && ghashes2[j] && BLI_ghash_haskey(ghashes2[j], key) && exist2) {
          int type1 = 1 << i;
          int type2 = 1 << j;
          debuglog("post: id %d used by multiple element types: %s and %s\n",
                   elem_type_to_str(type1),
                   elem_type_to_str(type2));
        }
      }

      bool exist_bad = exist1 && !exist2;

      if (is_applied) {
        exist_bad = !exist_bad;
      }

      /*element should exist in post but not in pre, or in neither*/
      if (exist_bad) {
        debuglog("element %u:%s should not exist\n", id, elem_type_to_str(1 << i));

        // debuglog("%s %u existes in both pre and post log sets!\n", elem_type_to_str(type),
        // id); ok = false;
      }
    }
  }

  if (do_apply) {
    if (!is_applied) {
      bm_log_undo_intern(bm, newlog, entry, NULL);
    }
    else {
      bm_log_redo_intern(bm, newlog, entry, NULL);
    }
  }

  if (precopy) {
    bm_log_entry_free_direct(entry);
    MEM_freeN(entry);
  }

  return ok;
}

bool BM_log_validate_cur(BMLog *log)
{
  return BM_log_validate(log->bm, log->current_entry, false);
}

bool BM_log_validate(BMesh *inbm, BMLogEntry *entry, bool is_applied)
{
  return bm_log_validate_intern(inbm, entry->log, entry, is_applied, false);

  BMLogEntry *cur;
  bool ret = true;

  BMLog newlog = {0};

  struct BMeshCreateParams params = {.create_unique_ids = true,
                                     .id_elem_mask = BM_VERT | BM_EDGE | BM_FACE,
                                     .no_reuse_ids = false,
                                     .temporary_ids = false,
                                     .copy_all_layers = true,
                                     .id_map = true};

  BMesh *bm = BM_mesh_copy_ex(inbm, &params);

  newlog.bm = bm;
  newlog.has_edges = true;
  newlog.refcount = 1;
  newlog.cd_sculpt_vert = entry->log->cd_sculpt_vert;

  if (!is_applied) {
    cur = entry;
    while (cur) {
      ret &= bm_log_validate_intern(bm, &newlog, cur, is_applied, true);
      cur = cur->combined_prev;
    }
  }
  else {
    cur = entry;
    while (cur->combined_prev) {
      cur = cur->combined_prev;
    }

    while (cur) {
      ret &= bm_log_validate_intern(bm, &newlog, cur, is_applied, true);
      cur = cur->combined_next;
    }
  }

  BM_mesh_free(bm);

  return ret;
}

bool BM_log_has_vert_pre(BMLog *log, BMVert *v)
{
  return BLI_ghash_haskey(log->current_entry->topo_modified_verts_pre,
                          POINTER_FROM_UINT(bm_log_vert_id_get(log, v)));
}
bool BM_log_has_edge_pre(BMLog *log, BMEdge *e)
{
  return BLI_ghash_haskey(log->current_entry->topo_modified_edges_pre,
                          POINTER_FROM_UINT(bm_log_edge_id_get(log, e)));
}
bool BM_log_has_face_pre(BMLog *log, BMFace *f)
{
  return BLI_ghash_haskey(log->current_entry->topo_modified_faces_pre,
                          POINTER_FROM_UINT(bm_log_face_id_get(log, f)));
}

bool BM_log_has_vert_post(BMLog *log, BMVert *v)
{
  return BLI_ghash_haskey(log->current_entry->topo_modified_verts_post,
                          POINTER_FROM_UINT(bm_log_vert_id_get(log, v)));
}

bool BM_log_has_edge_post(BMLog *log, BMEdge *e)
{
  return BLI_ghash_haskey(log->current_entry->topo_modified_edges_post,
                          POINTER_FROM_UINT(bm_log_edge_id_get(log, e)));
}
bool BM_log_has_face_post(BMLog *log, BMFace *f)
{
  return BLI_ghash_haskey(log->current_entry->topo_modified_faces_post,
                          POINTER_FROM_UINT(bm_log_face_id_get(log, f)));
}

void BM_log_get_changed(BMesh *bm, BMIdMap *idmap, BMLogEntry *_entry, SmallHash *sh)
{
  BMLogEntry *entry = _entry;

  while (entry->combined_prev) {
    entry = entry->combined_prev;
  }

  while (entry) {
    GHashIterator gh_iter;

    GHash **ghashes = &entry->topo_modified_verts_pre;

    for (int i = 0; i < 9; i++) {
      GHASH_ITER (gh_iter, ghashes[i]) {
        void *key = BLI_ghashIterator_getKey(&gh_iter);
        uint id = POINTER_AS_UINT(key);

        /* Note: elements are not guaranteed to exist */
#ifdef USE_NEW_IDMAP
        if (id >= idmap->map_size) {
          continue;
        }
#else
        if (id >= bm->idmap.map_size) {
          continue;
        }
#endif

#undef BLI_ghash_lookup
        BMElem *elem = BM_ELEM_FROM_ID(bm, id);
#define BLI_ghash_lookup(sh, key) BLI_smallhash_lookup((sh), (uintptr_t)(key))

        if (!elem) {
          continue;
        }

        BLI_smallhash_reinsert(sh, (uintptr_t)elem, (void *)elem);
      }
    }

    entry = entry->combined_next;
  }
}

void BM_log_set_idmap(BMLog *log, struct BMIdMap *idmap)
{
  log->idmap = idmap;
}
