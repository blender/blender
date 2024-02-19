/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Given a contiguous region of faces,
 * find multiple matching regions (based on topology) and return them.
 *
 * Implementation:
 *
 * - Given a face region, find its topological center.
 * - Compare this with other vertices surrounding geometry with this ones.
 *   (reduce the search space by creating a connectivity ID per vertex
 *   and only run comprehensive tests on those).
 * - All hashes must be order independent so matching topology can be identified.
 * - The term UID here doesn't mean each ID is initially unique.
 *   (uniqueness is improved by re-hashing with connected data).
 */

#include <cstring>

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "MEM_guardedalloc.h"

#include "bmesh.hh"

#include "tools/bmesh_region_match.hh" /* own include */

/* avoid re-creating ghash and pools for each search */
#define USE_WALKER_REUSE

/* do a first-pass id of all vertices,
 * this avoids expensive checks on every item later on
 * (works fine without, just slower) */
#define USE_PIVOT_FASTMATCH

/* otherwise use active element as pivot, for quick tests only */
#define USE_PIVOT_SEARCH

// #define DEBUG_TIME
// #define DEBUG_PRINT

#ifdef DEBUG_TIME
#  include "BLI_time.h"
#  include "BLI_time_utildefines.h"
#endif

#include "BLI_strict_flags.h" /* Keep last. */

/* -------------------------------------------------------------------- */
/** \name Internal UIDWalk API
 * \{ */

#define PRIME_VERT_INIT 100003

typedef uintptr_t UID_Int;

struct UIDWalk {

  /* List of faces we can step onto (UIDFaceStep's) */
  ListBase faces_step;

  /* Face & Vert UID's */
  GHash *verts_uid;
  GHash *faces_uid;

  /* memory pool for LinkNode's */
  BLI_mempool *link_pool;

  /* memory pool for LinkBase's */
  BLI_mempool *lbase_pool;

  /* memory pool for UIDFaceStep's */
  BLI_mempool *step_pool;
  BLI_mempool *step_pool_items;

  /* Optionally use face-tag to isolate search */
  bool use_face_isolate;

  /* Increment for each pass added */
  UID_Int pass;

  /* runtime vars, avoid re-creating each pass */
  struct {
    GHash *verts_uid; /* BMVert -> UID */
    GSet *faces_step; /* BMFace */

    GHash *faces_from_uid; /* UID -> UIDFaceStepItem */

    UID_Int *rehash_store;
    uint rehash_store_len;
  } cache;
};

/* stores a set of potential faces to step onto */
struct UIDFaceStep {
  UIDFaceStep *next, *prev;

  /* unsorted 'BMFace' */
  LinkNode *faces;

  /* faces sorted into 'UIDFaceStepItem' */
  ListBase items;
};

/* store face-lists with same HID. */
struct UIDFaceStepItem {
  UIDFaceStepItem *next, *prev;
  uintptr_t uid;

  LinkNode *list;
  uint list_len;
};

BLI_INLINE bool bm_uidwalk_face_test(UIDWalk *uidwalk, BMFace *f)
{
  if (uidwalk->use_face_isolate) {
    return BM_elem_flag_test_bool(f, BM_ELEM_TAG);
  }
  return true;
}

BLI_INLINE bool bm_uidwalk_vert_lookup(UIDWalk *uidwalk, BMVert *v, UID_Int *r_uid)
{
  void **ret;
  ret = BLI_ghash_lookup_p(uidwalk->verts_uid, v);
  if (ret) {
    *r_uid = (UID_Int)(*ret);
    return true;
  }
  return false;
}

BLI_INLINE bool bm_uidwalk_face_lookup(UIDWalk *uidwalk, BMFace *f, UID_Int *r_uid)
{
  void **ret;
  ret = BLI_ghash_lookup_p(uidwalk->faces_uid, f);
  if (ret) {
    *r_uid = (UID_Int)(*ret);
    return true;
  }
  return false;
}

static uint ghashutil_bmelem_indexhash(const void *key)
{
  const BMElem *ele = static_cast<const BMElem *>(key);
  return uint(BM_elem_index_get(ele));
}

static bool ghashutil_bmelem_indexcmp(const void *a, const void *b)
{
  BLI_assert((a != b) == (BM_elem_index_get((BMElem *)a) != BM_elem_index_get((BMElem *)b)));
  return (a != b);
}

static GHash *ghash_bmelem_new_ex(const char *info, const uint nentries_reserve)
{
  return BLI_ghash_new_ex(
      ghashutil_bmelem_indexhash, ghashutil_bmelem_indexcmp, info, nentries_reserve);
}

static GSet *gset_bmelem_new_ex(const char *info, const uint nentries_reserve)
{
  return BLI_gset_new_ex(
      ghashutil_bmelem_indexhash, ghashutil_bmelem_indexcmp, info, nentries_reserve);
}

static GHash *ghash_bmelem_new(const char *info)
{
  return ghash_bmelem_new_ex(info, 0);
}

static GSet *gset_bmelem_new(const char *info)
{
  return gset_bmelem_new_ex(info, 0);
}

static void bm_uidwalk_init(UIDWalk *uidwalk,
                            const uint faces_src_region_len,
                            const uint verts_src_region_len)
{
  BLI_listbase_clear(&uidwalk->faces_step);

  uidwalk->verts_uid = ghash_bmelem_new_ex(__func__, verts_src_region_len);
  uidwalk->faces_uid = ghash_bmelem_new_ex(__func__, faces_src_region_len);

  uidwalk->cache.verts_uid = ghash_bmelem_new(__func__);
  uidwalk->cache.faces_step = gset_bmelem_new(__func__);

  /* works because 'int' ghash works for intptr_t too */
  uidwalk->cache.faces_from_uid = BLI_ghash_int_new(__func__);

  uidwalk->cache.rehash_store = nullptr;
  uidwalk->cache.rehash_store_len = 0;

  uidwalk->use_face_isolate = false;

  /* smaller pool's for faster clearing */
  uidwalk->link_pool = BLI_mempool_create(sizeof(LinkNode), 64, 64, BLI_MEMPOOL_NOP);
  uidwalk->step_pool = BLI_mempool_create(sizeof(UIDFaceStep), 64, 64, BLI_MEMPOOL_NOP);
  uidwalk->step_pool_items = BLI_mempool_create(sizeof(UIDFaceStepItem), 64, 64, BLI_MEMPOOL_NOP);

  uidwalk->pass = 1;
}

static void bm_uidwalk_clear(UIDWalk *uidwalk)
{
  BLI_listbase_clear(&uidwalk->faces_step);

  BLI_ghash_clear(uidwalk->verts_uid, nullptr, nullptr);
  BLI_ghash_clear(uidwalk->faces_uid, nullptr, nullptr);

  BLI_ghash_clear(uidwalk->cache.verts_uid, nullptr, nullptr);
  BLI_gset_clear(uidwalk->cache.faces_step, nullptr);
  BLI_ghash_clear(uidwalk->cache.faces_from_uid, nullptr, nullptr);

  /* keep rehash_store as-is, for reuse */

  uidwalk->use_face_isolate = false;

  BLI_mempool_clear(uidwalk->link_pool);
  BLI_mempool_clear(uidwalk->step_pool);
  BLI_mempool_clear(uidwalk->step_pool_items);

  uidwalk->pass = 1;
}

static void bm_uidwalk_free(UIDWalk *uidwalk)
{
  /**
   * Handled by pools
   *
   * - uidwalk->faces_step
   */

  BLI_ghash_free(uidwalk->verts_uid, nullptr, nullptr);
  BLI_ghash_free(uidwalk->faces_uid, nullptr, nullptr);

  /* cache */
  BLI_ghash_free(uidwalk->cache.verts_uid, nullptr, nullptr);
  BLI_gset_free(uidwalk->cache.faces_step, nullptr);
  BLI_ghash_free(uidwalk->cache.faces_from_uid, nullptr, nullptr);
  MEM_SAFE_FREE(uidwalk->cache.rehash_store);

  BLI_mempool_destroy(uidwalk->link_pool);
  BLI_mempool_destroy(uidwalk->step_pool);
  BLI_mempool_destroy(uidwalk->step_pool_items);
}

static UID_Int bm_uidwalk_calc_vert_uid(UIDWalk *uidwalk, BMVert *v)
{
#define PRIME_VERT_SMALL 7
#define PRIME_VERT_MID 43
#define PRIME_VERT_LARGE 1031

#define PRIME_FACE_SMALL 13
#define PRIME_FACE_MID 53

  UID_Int uid;

  uid = uidwalk->pass * PRIME_VERT_LARGE;

  /* vert -> other */
  {
    uint tot = 0;
    BMIter eiter;
    BMEdge *e;
    BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
      BMVert *v_other = BM_edge_other_vert(e, v);
      UID_Int uid_other;
      if (bm_uidwalk_vert_lookup(uidwalk, v_other, &uid_other)) {
        uid ^= (uid_other * PRIME_VERT_SMALL);
        tot += 1;
      }
    }
    uid ^= (tot * PRIME_VERT_MID);
  }

  /* faces */
  {
    uint tot = 0;
    BMIter iter;
    BMFace *f;

    BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
      UID_Int uid_other;
      if (bm_uidwalk_face_lookup(uidwalk, f, &uid_other)) {
        uid ^= (uid_other * PRIME_FACE_SMALL);
        tot += 1;
      }
    }
    uid ^= (tot * PRIME_FACE_MID);
  }

  return uid;

#undef PRIME_VERT_SMALL
#undef PRIME_VERT_MID
#undef PRIME_VERT_LARGE

#undef PRIME_FACE_SMALL
#undef PRIME_FACE_MID
}

static UID_Int bm_uidwalk_calc_face_uid(UIDWalk *uidwalk, BMFace *f)
{
#define PRIME_VERT_SMALL 11

#define PRIME_FACE_SMALL 17
#define PRIME_FACE_LARGE 1013

  UID_Int uid;

  uid = uidwalk->pass * uint(f->len) * PRIME_FACE_LARGE;

  /* face-verts */
  {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      UID_Int uid_other;
      if (bm_uidwalk_vert_lookup(uidwalk, l_iter->v, &uid_other)) {
        uid ^= (uid_other * PRIME_VERT_SMALL);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  /* face-faces (connected by edge) */
  {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (l_iter->radial_next != l_iter) {
        BMLoop *l_iter_radial = l_iter->radial_next;
        do {
          UID_Int uid_other;
          if (bm_uidwalk_face_lookup(uidwalk, l_iter_radial->f, &uid_other)) {
            uid ^= (uid_other * PRIME_FACE_SMALL);
          }
        } while ((l_iter_radial = l_iter_radial->radial_next) != l_iter);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  return uid;

#undef PRIME_VERT_SMALL

#undef PRIME_FACE_SMALL
#undef PRIME_FACE_LARGE
}

static void bm_uidwalk_rehash_reserve(UIDWalk *uidwalk, uint rehash_store_len_new)
{
  if (UNLIKELY(rehash_store_len_new > uidwalk->cache.rehash_store_len)) {
    /* Avoid re-allocations. */
    rehash_store_len_new *= 2;
    uidwalk->cache.rehash_store = static_cast<UID_Int *>(MEM_reallocN(
        uidwalk->cache.rehash_store, rehash_store_len_new * sizeof(*uidwalk->cache.rehash_store)));
    uidwalk->cache.rehash_store_len = rehash_store_len_new;
  }
}

/**
 * Re-hash all elements, delay updating so as not to create feedback loop.
 */
static void bm_uidwalk_rehash(UIDWalk *uidwalk)
{
  GHashIterator gh_iter;
  UID_Int *uid_store;
  uint i;

  uint rehash_store_len_new = std::max(BLI_ghash_len(uidwalk->verts_uid),
                                       BLI_ghash_len(uidwalk->faces_uid));

  bm_uidwalk_rehash_reserve(uidwalk, rehash_store_len_new);
  uid_store = uidwalk->cache.rehash_store;

  /* verts */
  i = 0;
  GHASH_ITER (gh_iter, uidwalk->verts_uid) {
    BMVert *v = static_cast<BMVert *>(BLI_ghashIterator_getKey(&gh_iter));
    uid_store[i++] = bm_uidwalk_calc_vert_uid(uidwalk, v);
  }
  i = 0;
  GHASH_ITER (gh_iter, uidwalk->verts_uid) {
    void **uid_p = BLI_ghashIterator_getValue_p(&gh_iter);
    *((UID_Int *)uid_p) = uid_store[i++];
  }

  /* faces */
  i = 0;
  GHASH_ITER (gh_iter, uidwalk->faces_uid) {
    BMFace *f = static_cast<BMFace *>(BLI_ghashIterator_getKey(&gh_iter));
    uid_store[i++] = bm_uidwalk_calc_face_uid(uidwalk, f);
  }
  i = 0;
  GHASH_ITER (gh_iter, uidwalk->faces_uid) {
    void **uid_p = BLI_ghashIterator_getValue_p(&gh_iter);
    *((UID_Int *)uid_p) = uid_store[i++];
  }
}

static void bm_uidwalk_rehash_facelinks(UIDWalk *uidwalk,
                                        LinkNode *faces,
                                        const uint faces_len,
                                        const bool is_init)
{
  UID_Int *uid_store;
  LinkNode *f_link;
  uint i;

  bm_uidwalk_rehash_reserve(uidwalk, faces_len);
  uid_store = uidwalk->cache.rehash_store;

  i = 0;
  for (f_link = faces; f_link; f_link = f_link->next) {
    BMFace *f = static_cast<BMFace *>(f_link->link);
    uid_store[i++] = bm_uidwalk_calc_face_uid(uidwalk, f);
  }

  i = 0;
  if (is_init) {
    for (f_link = faces; f_link; f_link = f_link->next) {
      BMFace *f = static_cast<BMFace *>(f_link->link);
      BLI_ghash_insert(uidwalk->faces_uid, f, (void *)uid_store[i++]);
    }
  }
  else {
    for (f_link = faces; f_link; f_link = f_link->next) {
      BMFace *f = static_cast<BMFace *>(f_link->link);
      void **uid_p = BLI_ghash_lookup_p(uidwalk->faces_uid, f);
      *((UID_Int *)uid_p) = uid_store[i++];
    }
  }
}

static bool bm_vert_is_uid_connect(UIDWalk *uidwalk, BMVert *v)
{
  BMIter eiter;
  BMEdge *e;

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    BMVert *v_other = BM_edge_other_vert(e, v);
    if (BLI_ghash_haskey(uidwalk->verts_uid, v_other)) {
      return true;
    }
  }
  return false;
}

static void bm_uidwalk_pass_add(UIDWalk *uidwalk, LinkNode *faces_pass, const uint faces_pass_len)
{
  GHashIterator gh_iter;
  GHash *verts_uid_pass;
  GSet *faces_step_next;
  LinkNode *f_link;

  UIDFaceStep *fstep;

  BLI_assert(faces_pass_len == uint(BLI_linklist_count(faces_pass)));

  /* rehash faces now all their verts have been added */
  bm_uidwalk_rehash_facelinks(uidwalk, faces_pass, faces_pass_len, true);

  /* create verts_new */
  verts_uid_pass = uidwalk->cache.verts_uid;
  faces_step_next = uidwalk->cache.faces_step;

  BLI_assert(BLI_ghash_len(verts_uid_pass) == 0);
  BLI_assert(BLI_gset_len(faces_step_next) == 0);

  /* Add the face_step data from connected faces, creating new passes */
  fstep = static_cast<UIDFaceStep *>(BLI_mempool_alloc(uidwalk->step_pool));
  BLI_addhead(&uidwalk->faces_step, fstep);
  fstep->faces = nullptr;
  BLI_listbase_clear(&fstep->items);

  for (f_link = faces_pass; f_link; f_link = f_link->next) {
    BMFace *f = static_cast<BMFace *>(f_link->link);
    BMLoop *l_iter, *l_first;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      /* fill verts_new */
      void **val_p;
      if (!BLI_ghash_haskey(uidwalk->verts_uid, l_iter->v) &&
          !BLI_ghash_ensure_p(verts_uid_pass, l_iter->v, &val_p) &&
          (bm_vert_is_uid_connect(uidwalk, l_iter->v) == true))
      {
        const UID_Int uid = bm_uidwalk_calc_vert_uid(uidwalk, l_iter->v);
        *val_p = (void *)uid;
      }

      /* fill faces_step_next */
      if (l_iter->radial_next != l_iter) {
        BMLoop *l_iter_radial = l_iter->radial_next;
        do {
          if (!BLI_ghash_haskey(uidwalk->faces_uid, l_iter_radial->f) &&
              !BLI_gset_haskey(faces_step_next, l_iter_radial->f) &&
              bm_uidwalk_face_test(uidwalk, l_iter_radial->f))
          {
            BLI_gset_insert(faces_step_next, l_iter_radial->f);

            /* add to fstep */
            BLI_linklist_prepend_pool(&fstep->faces, l_iter_radial->f, uidwalk->link_pool);
          }
        } while ((l_iter_radial = l_iter_radial->radial_next) != l_iter);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  /* faces_uid.update(verts_new) */
  GHASH_ITER (gh_iter, verts_uid_pass) {
    BMVert *v = static_cast<BMVert *>(BLI_ghashIterator_getKey(&gh_iter));
    void *uid_p = BLI_ghashIterator_getValue(&gh_iter);
    BLI_ghash_insert(uidwalk->verts_uid, v, uid_p);
  }

  /* rehash faces now all their verts have been added */
  bm_uidwalk_rehash_facelinks(uidwalk, faces_pass, faces_pass_len, false);

  uidwalk->pass += 1;

  BLI_ghash_clear(uidwalk->cache.verts_uid, nullptr, nullptr);
  BLI_gset_clear(uidwalk->cache.faces_step, nullptr);
}

static int bm_face_len_cmp(const void *v1, const void *v2)
{
  const BMFace *f1 = *((BMFace **)v1);
  const BMFace *f2 = *((BMFace **)v2);

  if (f1->len > f2->len) {
    return 1;
  }
  if (f1->len < f2->len) {
    return -1;
  }
  return 0;
}

static uint bm_uidwalk_init_from_edge(UIDWalk *uidwalk, BMEdge *e)
{
  BMLoop *l_iter = e->l;
  uint f_arr_len = uint(BM_edge_face_count(e));
  BMFace **f_arr = BLI_array_alloca(f_arr, f_arr_len);
  uint fstep_num = 0, i = 0;

  do {
    BMFace *f = l_iter->f;
    if (bm_uidwalk_face_test(uidwalk, f)) {
      f_arr[i++] = f;
    }
  } while ((l_iter = l_iter->radial_next) != e->l);
  BLI_assert(i <= f_arr_len);
  f_arr_len = i;

  qsort(f_arr, f_arr_len, sizeof(*f_arr), bm_face_len_cmp);

  /* start us off! */
  {
    const UID_Int uid = PRIME_VERT_INIT;
    BLI_ghash_insert(uidwalk->verts_uid, e->v1, (void *)uid);
    BLI_ghash_insert(uidwalk->verts_uid, e->v2, (void *)uid);
  }

  /* turning an array into LinkNode's seems odd,
   * but this is just for initialization,
   * elsewhere using LinkNode's makes more sense */
  for (i = 0; i < f_arr_len;) {
    LinkNode *faces_pass = nullptr;
    const uint i_init = i;
    const int f_len = f_arr[i]->len;

    do {
      BLI_linklist_prepend_pool(&faces_pass, f_arr[i++], uidwalk->link_pool);
    } while (i < f_arr_len && (f_len == f_arr[i]->len));

    bm_uidwalk_pass_add(uidwalk, faces_pass, i - i_init);
    BLI_linklist_free_pool(faces_pass, nullptr, uidwalk->link_pool);
    fstep_num += 1;
  }

  return fstep_num;
}

#undef PRIME_VERT_INIT

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal UIDFaceStep API
 * \{ */

static int facestep_sort(const void *a, const void *b)
{
  const UIDFaceStepItem *fstep_a = static_cast<const UIDFaceStepItem *>(a);
  const UIDFaceStepItem *fstep_b = static_cast<const UIDFaceStepItem *>(b);
  return (fstep_a->uid > fstep_b->uid) ? 1 : 0;
}

/**
 * Put faces in lists based on their UID's,
 * re-run for each pass since rehashing may differentiate face-groups.
 */
static bool bm_uidwalk_facestep_begin(UIDWalk *uidwalk, UIDFaceStep *fstep)
{
  LinkNode *f_link, *f_link_next, **f_link_prev_p;
  bool ok = false;

  BLI_assert(BLI_ghash_len(uidwalk->cache.faces_from_uid) == 0);
  BLI_assert(BLI_listbase_is_empty(&fstep->items));

  f_link_prev_p = &fstep->faces;
  for (f_link = fstep->faces; f_link; f_link = f_link_next) {
    BMFace *f = static_cast<BMFace *>(f_link->link);
    f_link_next = f_link->next;

    /* possible another pass added this face already, free in that case */
    if (!BLI_ghash_haskey(uidwalk->faces_uid, f)) {
      const UID_Int uid = bm_uidwalk_calc_face_uid(uidwalk, f);
      UIDFaceStepItem *fstep_item;
      void **val_p;

      ok = true;

      if (BLI_ghash_ensure_p(uidwalk->cache.faces_from_uid, (void *)uid, &val_p)) {
        fstep_item = static_cast<UIDFaceStepItem *>(*val_p);
      }
      else {
        fstep_item = static_cast<UIDFaceStepItem *>(*val_p = BLI_mempool_alloc(
                                                        uidwalk->step_pool_items));

        /* add to start, so its handled on the next round of passes */
        BLI_addhead(&fstep->items, fstep_item);
        fstep_item->uid = uid;
        fstep_item->list = nullptr;
        fstep_item->list_len = 0;
      }

      BLI_linklist_prepend_pool(&fstep_item->list, f, uidwalk->link_pool);
      fstep_item->list_len += 1;

      f_link_prev_p = &f_link->next;
    }
    else {
      *f_link_prev_p = f_link->next;
      BLI_mempool_free(uidwalk->link_pool, f_link);
    }
  }

  BLI_ghash_clear(uidwalk->cache.faces_from_uid, nullptr, nullptr);

  BLI_listbase_sort(&fstep->items, facestep_sort);

  return ok;
}

/**
 * Cleans up temp data from #bm_uidwalk_facestep_begin
 */
static void bm_uidwalk_facestep_end(UIDWalk *uidwalk, UIDFaceStep *fstep)
{
  while (UIDFaceStepItem *fstep_item = static_cast<UIDFaceStepItem *>(BLI_pophead(&fstep->items)))
  {
    BLI_mempool_free(uidwalk->step_pool_items, fstep_item);
  }
}

static void bm_uidwalk_facestep_free(UIDWalk *uidwalk, UIDFaceStep *fstep)
{
  LinkNode *f_link, *f_link_next;

  BLI_assert(BLI_listbase_is_empty(&fstep->items));

  for (f_link = fstep->faces; f_link; f_link = f_link_next) {
    f_link_next = f_link->next;
    BLI_mempool_free(uidwalk->link_pool, f_link);
  }

  BLI_remlink(&uidwalk->faces_step, fstep);
  BLI_mempool_free(uidwalk->step_pool, fstep);
}

/** \} */

/* -------------------------------------------------------------------- */
/* Main Loop to match up regions */

/**
 * Given a face region and 2 candidate verts to begin mapping.
 * return the matching region or nullptr.
 */
static BMFace **bm_mesh_region_match_pair(
#ifdef USE_WALKER_REUSE
    UIDWalk *w_src,
    UIDWalk *w_dst,
#endif
    BMEdge *e_src,
    BMEdge *e_dst,
    const uint faces_src_region_len,
    const uint verts_src_region_len,
    uint *r_faces_result_len)
{
#ifndef USE_WALKER_REUSE
  UIDWalk w_src_, w_dst_;
  UIDWalk *w_src = &w_src_, *w_dst = &w_dst_;
#endif
  BMFace **faces_result = nullptr;
  bool found = false;

  BLI_assert(e_src != e_dst);

#ifndef USE_WALKER_REUSE
  bm_uidwalk_init(w_src, faces_src_region_len, verts_src_region_len);
  bm_uidwalk_init(w_dst, faces_src_region_len, verts_src_region_len);
#endif

  w_src->use_face_isolate = true;

  /* setup the initial state */
  if (UNLIKELY(bm_uidwalk_init_from_edge(w_src, e_src) != bm_uidwalk_init_from_edge(w_dst, e_dst)))
  {
    /* should never happen, if verts passed are compatible, but to be safe... */
    goto finally;
  }

  bm_uidwalk_rehash_reserve(w_src, std::max(faces_src_region_len, verts_src_region_len));
  bm_uidwalk_rehash_reserve(w_dst, std::max(faces_src_region_len, verts_src_region_len));

  while (true) {
    bool ok = false;

    UIDFaceStep *fstep_src = static_cast<UIDFaceStep *>(w_src->faces_step.first);
    UIDFaceStep *fstep_dst = static_cast<UIDFaceStep *>(w_dst->faces_step.first);

    BLI_assert(BLI_listbase_count(&w_src->faces_step) == BLI_listbase_count(&w_dst->faces_step));

    while (fstep_src) {

      /* even if the destination has faces,
       * it's not important, since the source doesn't, free and move-on. */
      if (fstep_src->faces == nullptr) {
        UIDFaceStep *fstep_src_next = fstep_src->next;
        UIDFaceStep *fstep_dst_next = fstep_dst->next;
        bm_uidwalk_facestep_free(w_src, fstep_src);
        bm_uidwalk_facestep_free(w_dst, fstep_dst);
        fstep_src = fstep_src_next;
        fstep_dst = fstep_dst_next;
        continue;
      }

      if (bm_uidwalk_facestep_begin(w_src, fstep_src) &&
          bm_uidwalk_facestep_begin(w_dst, fstep_dst))
      {
        /* Step over face-lists with matching UID's
         * both lists are sorted, so no need for lookups.
         * The data is created on 'begin' and cleared on 'end' */
        UIDFaceStepItem *fstep_item_src;
        UIDFaceStepItem *fstep_item_dst;
        for (fstep_item_src = static_cast<UIDFaceStepItem *>(fstep_src->items.first),
            fstep_item_dst = static_cast<UIDFaceStepItem *>(fstep_dst->items.first);
             fstep_item_src && fstep_item_dst;
             fstep_item_src = fstep_item_src->next, fstep_item_dst = fstep_item_dst->next)
        {
          while ((fstep_item_dst != nullptr) && (fstep_item_dst->uid < fstep_item_src->uid)) {
            fstep_item_dst = fstep_item_dst->next;
          }

          if ((fstep_item_dst == nullptr) || (fstep_item_src->uid != fstep_item_dst->uid) ||
              (fstep_item_src->list_len > fstep_item_dst->list_len))
          {
            /* if the target walker has less than the source
             * then the islands don't match, bail early */
            ok = false;
            break;
          }

          if (fstep_item_src->list_len == fstep_item_dst->list_len) {
            /* found a match */
            bm_uidwalk_pass_add(w_src, fstep_item_src->list, fstep_item_src->list_len);
            bm_uidwalk_pass_add(w_dst, fstep_item_dst->list, fstep_item_dst->list_len);

            BLI_linklist_free_pool(fstep_item_src->list, nullptr, w_src->link_pool);
            BLI_linklist_free_pool(fstep_item_dst->list, nullptr, w_dst->link_pool);

            fstep_item_src->list = nullptr;
            fstep_item_src->list_len = 0;

            fstep_item_dst->list = nullptr;
            fstep_item_dst->list_len = 0;

            ok = true;
          }
        }
      }

      bm_uidwalk_facestep_end(w_src, fstep_src);
      bm_uidwalk_facestep_end(w_dst, fstep_dst);

      /* lock-step */
      fstep_src = fstep_src->next;
      fstep_dst = fstep_dst->next;
    }

    if (!ok) {
      break;
    }

    found = (BLI_ghash_len(w_dst->faces_uid) == faces_src_region_len);
    if (found) {
      break;
    }

    /* Expensive! but some cases fails without.
     * (also faster in other cases since it can rule-out invalid regions) */
    bm_uidwalk_rehash(w_src);
    bm_uidwalk_rehash(w_dst);
  }

  if (found) {
    GHashIterator gh_iter;
    const uint faces_result_len = BLI_ghash_len(w_dst->faces_uid);
    uint i;

    faces_result = static_cast<BMFace **>(
        MEM_mallocN(sizeof(*faces_result) * (faces_result_len + 1), __func__));
    GHASH_ITER_INDEX (gh_iter, w_dst->faces_uid, i) {
      BMFace *f = static_cast<BMFace *>(BLI_ghashIterator_getKey(&gh_iter));
      faces_result[i] = f;
    }
    faces_result[faces_result_len] = nullptr;
    *r_faces_result_len = faces_result_len;
  }
  else {
    *r_faces_result_len = 0;
  }

finally:

#ifdef USE_WALKER_REUSE
  bm_uidwalk_clear(w_src);
  bm_uidwalk_clear(w_dst);
#else
  bm_uidwalk_free(w_src);
  bm_uidwalk_free(w_dst);
#endif

  return faces_result;
}

/**
 * Tag as visited, avoid re-use.
 */
static void bm_face_array_visit(BMFace **faces,
                                const uint faces_len,
                                uint *r_verts_len,
                                bool visit_faces)
{
  uint verts_len = 0;
  uint i;
  for (i = 0; i < faces_len; i++) {
    BMFace *f = faces[i];
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (r_verts_len) {
        if (!BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
          verts_len += 1;
        }
      }

      BM_elem_flag_enable(l_iter->e, BM_ELEM_TAG);
      BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
    } while ((l_iter = l_iter->next) != l_first);

    if (visit_faces) {
      BM_elem_flag_enable(f, BM_ELEM_TAG);
    }
  }

  if (r_verts_len) {
    *r_verts_len = verts_len;
  }
}

#ifdef USE_PIVOT_SEARCH

/* -------------------------------------------------------------------- */
/** \name Internal UIDWalk API
 * \{ */

/* signed user id */
typedef intptr_t SUID_Int;

BLI_INLINE intptr_t abs_intptr(intptr_t a)
{
  return (a < 0) ? -a : a;
}

static bool bm_edge_is_region_boundary(BMEdge *e)
{
  if (e->l->radial_next != e->l) {
    BMLoop *l_iter = e->l;
    do {
      if (!BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
        return true;
      }
    } while ((l_iter = l_iter->radial_next) != e->l);
    return false;
  }
  /* boundary */
  return true;
}

static void bm_face_region_pivot_edge_use_best(GHash *gh,
                                               BMEdge *e_test,
                                               BMEdge **r_e_pivot_best,
                                               SUID_Int e_pivot_best_id[2])
{
  SUID_Int e_pivot_test_id[2];

  e_pivot_test_id[0] = (SUID_Int)BLI_ghash_lookup(gh, e_test->v1);
  e_pivot_test_id[1] = (SUID_Int)BLI_ghash_lookup(gh, e_test->v2);
  if (e_pivot_test_id[0] > e_pivot_test_id[1]) {
    std::swap(e_pivot_test_id[0], e_pivot_test_id[1]);
  }

  if ((*r_e_pivot_best == nullptr) ||
      ((e_pivot_best_id[0] != e_pivot_test_id[0]) ? (e_pivot_best_id[0] < e_pivot_test_id[0]) :
                                                    (e_pivot_best_id[1] < e_pivot_test_id[1])))
  {
    e_pivot_best_id[0] = e_pivot_test_id[0];
    e_pivot_best_id[1] = e_pivot_test_id[1];

    /* both verts are from the same pass, record this! */
    *r_e_pivot_best = e_test;
  }
}

/* quick id from a boundary vertex */
static SUID_Int bm_face_region_vert_boundary_id(BMVert *v)
{
#  define PRIME_VERT_SMALL_A 7
#  define PRIME_VERT_SMALL_B 13
#  define PRIME_VERT_MID_A 103
#  define PRIME_VERT_MID_B 131

  int tot = 0;
  BMIter iter;
  BMLoop *l;
  SUID_Int id = PRIME_VERT_MID_A;

  BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
    const bool is_boundary_vert = (bm_edge_is_region_boundary(l->e) ||
                                   bm_edge_is_region_boundary(l->prev->e));
    id ^= l->f->len * (is_boundary_vert ? PRIME_VERT_SMALL_A : PRIME_VERT_SMALL_B);
    tot += 1;
  }

  id ^= (tot * PRIME_VERT_MID_B);

  return id ? abs_intptr(id) : 1;

#  undef PRIME_VERT_SMALL_A
#  undef PRIME_VERT_SMALL_B
#  undef PRIME_VERT_MID_A
#  undef PRIME_VERT_MID_B
}

/**
 * Accumulate id's from a previous pass (swap sign each pass)
 */
static SUID_Int bm_face_region_vert_pass_id(GHash *gh, BMVert *v)
{
  BMIter eiter;
  BMEdge *e;
  SUID_Int tot = 0;
  SUID_Int v_sum_face_len = 0;
  SUID_Int v_sum_id = 0;
  SUID_Int id;
  SUID_Int id_min = INTPTR_MIN + 1;

#  define PRIME_VERT_MID_A 23
#  define PRIME_VERT_MID_B 31

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
      BMVert *v_other = BM_edge_other_vert(e, v);
      if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
        /* non-zero values aren't allowed... so no need to check haskey */
        SUID_Int v_other_id = (SUID_Int)BLI_ghash_lookup(gh, v_other);
        if (v_other_id > 0) {
          v_sum_id += v_other_id;
          tot += 1;

          /* face-count */
          {
            BMLoop *l_iter = e->l;
            do {
              if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
                v_sum_face_len += l_iter->f->len;
              }
            } while ((l_iter = l_iter->radial_next) != e->l);
          }
        }
      }
    }
  }

  id = (tot * PRIME_VERT_MID_A);
  id ^= (v_sum_face_len * PRIME_VERT_MID_B);
  id ^= v_sum_id;

  /* disallow 0 & min (since it can't be flipped) */
  id = (UNLIKELY(id == 0) ? 1 : UNLIKELY(id < id_min) ? id_min : id);

  return abs_intptr(id);

#  undef PRIME_VERT_MID_A
#  undef PRIME_VERT_MID_B
}

/**
 * Take a face region and find the inner-most vertex.
 * also calculate the number of connections to the boundary,
 * and the total number unique of verts used by this face region.
 *
 * This is only called once on the source region (no need to be highly optimized).
 */
static BMEdge *bm_face_region_pivot_edge_find(BMFace **faces_region,
                                              uint faces_region_len,
                                              uint verts_region_len,
                                              uint *r_depth)
{
  /* NOTE: keep deterministic where possible (geometry order independent)
   * this function assumed all visit faces & edges are tagged */

  BLI_LINKSTACK_DECLARE(vert_queue_prev, BMVert *);
  BLI_LINKSTACK_DECLARE(vert_queue_next, BMVert *);

  GHash *gh = BLI_ghash_ptr_new(__func__);
  uint i;

  BMEdge *e_pivot = nullptr;
  /* pick any non-boundary edge (not ideal) */
  BMEdge *e_pivot_fallback = nullptr;

  SUID_Int pass = 0;

  /* total verts in 'gs' we have visited - aka - not v_init_none */
  uint vert_queue_used = 0;

  BLI_LINKSTACK_INIT(vert_queue_prev);
  BLI_LINKSTACK_INIT(vert_queue_next);

  /* face-verts */
  for (i = 0; i < faces_region_len; i++) {
    BMFace *f = faces_region[i];

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BMEdge *e = l_iter->e;
      if (bm_edge_is_region_boundary(e)) {
        uint j;
        for (j = 0; j < 2; j++) {
          void **val_p;
          if (!BLI_ghash_ensure_p(gh, (&e->v1)[j], &val_p)) {
            SUID_Int v_id = bm_face_region_vert_boundary_id((&e->v1)[j]);
            *val_p = (void *)v_id;
            BLI_LINKSTACK_PUSH(vert_queue_prev, (&e->v1)[j]);
            vert_queue_used += 1;
          }
        }
      }
      else {
        /* Use in case (depth == 0), no interior verts. */
        e_pivot_fallback = e;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  while (BLI_LINKSTACK_SIZE(vert_queue_prev)) {
    BMVert *v;
    while ((v = BLI_LINKSTACK_POP(vert_queue_prev))) {
      BMIter eiter;
      BMEdge *e;
      BLI_assert(BLI_ghash_haskey(gh, v));
      BLI_assert((SUID_Int)BLI_ghash_lookup(gh, v) > 0);
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
          BMVert *v_other = BM_edge_other_vert(e, v);
          if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
            void **val_p;
            if (!BLI_ghash_ensure_p(gh, v_other, &val_p)) {
              /* add as negative, so we know not to read from them this pass */
              const SUID_Int v_id_other = -bm_face_region_vert_pass_id(gh, v_other);
              *val_p = (void *)v_id_other;
              BLI_LINKSTACK_PUSH(vert_queue_next, v_other);
              vert_queue_used += 1;
            }
          }
        }
      }
    }

    /* flip all the newly added hashes to positive */
    {
      LinkNode *v_link;
      for (v_link = vert_queue_next; v_link; v_link = v_link->next) {
        SUID_Int *v_id_p = (SUID_Int *)BLI_ghash_lookup_p(gh, v_link->link);
        *v_id_p = -(*v_id_p);
        BLI_assert(*v_id_p > 0);
      }
    }

    BLI_LINKSTACK_SWAP(vert_queue_prev, vert_queue_next);
    pass += 1;

    if (vert_queue_used == verts_region_len) {
      break;
    }
  }

  if (BLI_LINKSTACK_SIZE(vert_queue_prev) >= 2) {
    /* common case - we managed to find some interior verts */
    LinkNode *v_link;
    BMEdge *e_pivot_best = nullptr;
    SUID_Int e_pivot_best_id[2] = {0, 0};

    /* temp untag, so we can quickly know what other verts are in this last pass */
    for (v_link = vert_queue_prev; v_link; v_link = v_link->next) {
      BMVert *v = static_cast<BMVert *>(v_link->link);
      BM_elem_flag_disable(v, BM_ELEM_TAG);
    }

    /* restore correct tagging */
    for (v_link = vert_queue_prev; v_link; v_link = v_link->next) {
      BMIter eiter;
      BMEdge *e_test;

      BMVert *v = static_cast<BMVert *>(v_link->link);
      BM_elem_flag_enable(v, BM_ELEM_TAG);

      BM_ITER_ELEM (e_test, &eiter, v, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(e_test, BM_ELEM_TAG)) {
          BMVert *v_other = BM_edge_other_vert(e_test, v);
          if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == false) {
            bm_face_region_pivot_edge_use_best(gh, e_test, &e_pivot_best, e_pivot_best_id);
          }
        }
      }
    }

    e_pivot = e_pivot_best;
  }

  if ((e_pivot == nullptr) && BLI_LINKSTACK_SIZE(vert_queue_prev)) {
    /* find the best single edge */
    BMEdge *e_pivot_best = nullptr;
    SUID_Int e_pivot_best_id[2] = {0, 0};

    LinkNode *v_link;

    /* reduce a pass since we're having to step into a previous passes vert,
     * and will be closer to the boundary */
    BLI_assert(pass != 0);
    pass -= 1;

    for (v_link = vert_queue_prev; v_link; v_link = v_link->next) {
      BMVert *v = static_cast<BMVert *>(v_link->link);

      BMIter eiter;
      BMEdge *e_test;
      BM_ITER_ELEM (e_test, &eiter, v, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(e_test, BM_ELEM_TAG)) {
          BMVert *v_other = BM_edge_other_vert(e_test, v);
          if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
            bm_face_region_pivot_edge_use_best(gh, e_test, &e_pivot_best, e_pivot_best_id);
          }
        }
      }
    }

    e_pivot = e_pivot_best;
  }

  BLI_LINKSTACK_FREE(vert_queue_prev);
  BLI_LINKSTACK_FREE(vert_queue_next);

  BLI_ghash_free(gh, nullptr, nullptr);

  if (e_pivot == nullptr) {
#  ifdef DEBUG_PRINT
    printf("%s: using fallback edge!\n", __func__);
#  endif
    e_pivot = e_pivot_fallback;
    pass = 0;
  }

  *r_depth = uint(pass);

  return e_pivot;
}

/** \} */

#endif /* USE_PIVOT_SEARCH */

/* Quick UID pass - identify candidates */

#ifdef USE_PIVOT_FASTMATCH

/* -------------------------------------------------------------------- */
/** \name Fast Match
 * \{ */

typedef uintptr_t UIDFashMatch;

static UIDFashMatch bm_vert_fasthash_single(BMVert *v)
{
  BMIter eiter;
  BMEdge *e;
  UIDFashMatch e_num = 0, f_num = 0, l_num = 0;

#  define PRIME_EDGE 7
#  define PRIME_FACE 31
#  define PRIME_LOOP 61

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    if (!BM_edge_is_wire(e)) {
      BMLoop *l_iter = e->l;
      e_num += 1;
      do {
        f_num += 1;
        l_num += uint(l_iter->f->len);
      } while ((l_iter = l_iter->radial_next) != e->l);
    }
  }

  return ((e_num * PRIME_EDGE) ^ (f_num * PRIME_FACE) * (l_num * PRIME_LOOP));

#  undef PRIME_EDGE
#  undef PRIME_FACE
#  undef PRIME_LOOP
}

static UIDFashMatch *bm_vert_fasthash_create(BMesh *bm, const uint depth)
{
  UIDFashMatch *id_prev;
  UIDFashMatch *id_curr;
  uint pass, i;
  BMVert *v;
  BMIter iter;

  id_prev = static_cast<UIDFashMatch *>(
      MEM_mallocN(sizeof(*id_prev) * uint(bm->totvert), __func__));
  id_curr = static_cast<UIDFashMatch *>(
      MEM_mallocN(sizeof(*id_curr) * uint(bm->totvert), __func__));

  BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
    id_prev[i] = bm_vert_fasthash_single(v);
  }

  for (pass = 0; pass < depth; pass++) {
    BMEdge *e;

    memcpy(id_curr, id_prev, sizeof(*id_prev) * uint(bm->totvert));

    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_edge_is_wire(e) == false) {
        const int i1 = BM_elem_index_get(e->v1);
        const int i2 = BM_elem_index_get(e->v2);

        id_curr[i1] += id_prev[i2];
        id_curr[i2] += id_prev[i1];
      }
    }
  }
  MEM_freeN(id_prev);

  return id_curr;
}

static void bm_vert_fasthash_edge_order(const UIDFashMatch *fm,
                                        const BMEdge *e,
                                        UIDFashMatch e_fm[2])
{
  e_fm[0] = fm[BM_elem_index_get(e->v1)];
  e_fm[1] = fm[BM_elem_index_get(e->v2)];

  if (e_fm[0] > e_fm[1]) {
    std::swap(e_fm[0], e_fm[1]);
  }
}

static bool bm_vert_fasthash_edge_is_match(UIDFashMatch *fm, const BMEdge *e_a, const BMEdge *e_b)
{
  UIDFashMatch e_a_fm[2];
  UIDFashMatch e_b_fm[2];

  bm_vert_fasthash_edge_order(fm, e_a, e_a_fm);
  bm_vert_fasthash_edge_order(fm, e_b, e_b_fm);

  return ((e_a_fm[0] == e_b_fm[0]) && (e_a_fm[1] == e_b_fm[1]));
}

static void bm_vert_fasthash_destroy(UIDFashMatch *fm)
{
  MEM_freeN(fm);
}

/** \} */

#endif /* USE_PIVOT_FASTMATCH */

int BM_mesh_region_match(BMesh *bm,
                         BMFace **faces_region,
                         uint faces_region_len,
                         ListBase *r_face_regions)
{
  BMEdge *e_src;
  BMEdge *e_dst;
  BMIter iter;
  uint verts_region_len = 0;
  uint faces_result_len = 0;
  /* number of steps from e_src to a boundary vert */
  uint depth;

#ifdef USE_WALKER_REUSE
  UIDWalk w_src, w_dst;
#endif

#ifdef USE_PIVOT_FASTMATCH
  UIDFashMatch *fm;
#endif

#ifdef DEBUG_PRINT
  int search_num = 0;
#endif

#ifdef DEBUG_TIME
  TIMEIT_START(region_match);
#endif

  /* initialize visited verts */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  bm_face_array_visit(faces_region, faces_region_len, &verts_region_len, true);

  /* needed for 'ghashutil_bmelem_indexhash' */
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

#ifdef USE_PIVOT_SEARCH
  e_src = bm_face_region_pivot_edge_find(faces_region, faces_region_len, verts_region_len, &depth);

/* see which edge is added */
#  if 0
  BM_select_history_clear(bm);
  if (e_src) {
    BM_select_history_store(bm, e_src);
  }
#  endif

#else
  /* quick test only! */
  e_src = BM_mesh_active_edge_get(bm);
#endif

  if (e_src == nullptr) {
#ifdef DEBUG_PRINT
    printf("Couldn't find 'e_src'");
#endif
    return 0;
  }

  BLI_listbase_clear(r_face_regions);

#ifdef USE_PIVOT_FASTMATCH
  if (depth > 0) {
    fm = bm_vert_fasthash_create(bm, depth);
  }
  else {
    fm = nullptr;
  }
#endif

#ifdef USE_WALKER_REUSE
  bm_uidwalk_init(&w_src, faces_region_len, verts_region_len);
  bm_uidwalk_init(&w_dst, faces_region_len, verts_region_len);
#endif

  BM_ITER_MESH (e_dst, &iter, bm, BM_EDGES_OF_MESH) {
    BMFace **faces_result;
    uint faces_result_len_out;

    if (BM_elem_flag_test(e_dst, BM_ELEM_TAG) || BM_edge_is_wire(e_dst)) {
      continue;
    }

#ifdef USE_PIVOT_FASTMATCH
    if (fm && !bm_vert_fasthash_edge_is_match(fm, e_src, e_dst)) {
      continue;
    }
#endif

#ifdef DEBUG_PRINT
    search_num += 1;
#endif

    faces_result = bm_mesh_region_match_pair(
#ifdef USE_WALKER_REUSE
        &w_src,
        &w_dst,
#endif
        e_src,
        e_dst,
        faces_region_len,
        verts_region_len,
        &faces_result_len_out);

    /* tag verts as visited */
    if (faces_result) {
      LinkData *link;

      bm_face_array_visit(faces_result, faces_result_len_out, nullptr, false);

      link = BLI_genericNodeN(faces_result);
      BLI_addtail(r_face_regions, link);
      faces_result_len += 1;
    }
  }

#ifdef USE_WALKER_REUSE
  bm_uidwalk_free(&w_src);
  bm_uidwalk_free(&w_dst);
#else
  (void)bm_uidwalk_clear;
#endif

#ifdef USE_PIVOT_FASTMATCH
  if (fm) {
    bm_vert_fasthash_destroy(fm);
  }
#endif

#ifdef DEBUG_PRINT
  printf("%s: search: %d, found %d\n", __func__, search_num, faces_result_len);
#endif

#ifdef DEBUG_TIME
  TIMEIT_END(region_match);
#endif

  return int(faces_result_len);
}
