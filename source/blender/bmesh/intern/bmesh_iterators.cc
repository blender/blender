/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Functions to abstract looping over bmesh data structures.
 *
 * See: bmesh_iterators_inlin.c too, some functions are here for speed reasons.
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_utildefines.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

const char bm_iter_itype_htype_map[BM_ITYPE_MAX] = {
    '\0',
    BM_VERT, /* BM_VERTS_OF_MESH */
    BM_EDGE, /* BM_EDGES_OF_MESH */
    BM_FACE, /* BM_FACES_OF_MESH */
    BM_EDGE, /* BM_EDGES_OF_VERT */
    BM_FACE, /* BM_FACES_OF_VERT */
    BM_LOOP, /* BM_LOOPS_OF_VERT */
    BM_VERT, /* BM_VERTS_OF_EDGE */
    BM_FACE, /* BM_FACES_OF_EDGE */
    BM_VERT, /* BM_VERTS_OF_FACE */
    BM_EDGE, /* BM_EDGES_OF_FACE */
    BM_LOOP, /* BM_LOOPS_OF_FACE */
    BM_LOOP, /* BM_LOOPS_OF_LOOP */
    BM_LOOP, /* BM_LOOPS_OF_EDGE */
};

int BM_iter_mesh_count(const char itype, BMesh *bm)
{
  int count;

  switch (itype) {
    case BM_VERTS_OF_MESH:
      count = bm->totvert;
      break;
    case BM_EDGES_OF_MESH:
      count = bm->totedge;
      break;
    case BM_FACES_OF_MESH:
      count = bm->totface;
      break;
    default:
      count = 0;
      BLI_assert(0);
      break;
  }

  return count;
}

void *BM_iter_at_index(BMesh *bm, const char itype, void *data, int index)
{
  BMIter iter;
  void *val;
  int i;

  /* sanity check */
  if (index < 0) {
    return nullptr;
  }

  val = BM_iter_new(&iter, bm, itype, data);

  i = 0;
  while (i < index) {
    val = BM_iter_step(&iter);
    i++;
  }

  return val;
}

int BM_iter_as_array(BMesh *bm, const char itype, void *data, void **array, const int len)
{
  int i = 0;

  /* sanity check */
  if (len > 0) {
    BMIter iter;
    void *ele;

    for (ele = BM_iter_new(&iter, bm, itype, data); ele; ele = BM_iter_step(&iter)) {
      array[i] = ele;
      i++;
      if (i == len) {
        return len;
      }
    }
  }

  return i;
}
int BMO_iter_as_array(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                      const char *slot_name,
                      const char restrictmask,
                      void **array,
                      const int len)
{
  int i = 0;

  /* sanity check */
  if (len > 0) {
    BMOIter oiter;
    void *ele;

    for (ele = BMO_iter_new(&oiter, slot_args, slot_name, restrictmask); ele;
         ele = BMO_iter_step(&oiter))
    {
      array[i] = ele;
      i++;
      if (i == len) {
        return len;
      }
    }
  }

  return i;
}

void *BM_iter_as_arrayN(BMesh *bm,
                        const char itype,
                        void *data,
                        int *r_len,
                        /* optional args to avoid an alloc (normally stack array) */
                        void **stack_array,
                        int stack_array_size)
{
  BMIter iter;

  BLI_assert(stack_array_size == 0 || (stack_array_size && stack_array));

  /* We can't rely on #BMIter.count being set. */
  switch (itype) {
    case BM_VERTS_OF_MESH:
      iter.count = bm->totvert;
      break;
    case BM_EDGES_OF_MESH:
      iter.count = bm->totedge;
      break;
    case BM_FACES_OF_MESH:
      iter.count = bm->totface;
      break;
    default:
      break;
  }

  if (BM_iter_init(&iter, bm, itype, data) && iter.count > 0) {
    BMElem *ele;
    BMElem **array = iter.count > stack_array_size ?
                         static_cast<BMElem **>(MEM_mallocN(sizeof(ele) * iter.count, __func__)) :
                         reinterpret_cast<BMElem **>(stack_array);
    int i = 0;

    *r_len = iter.count; /* set before iterating */

    while ((ele = static_cast<BMElem *>(BM_iter_step(&iter)))) {
      array[i++] = ele;
    }
    return array;
  }

  *r_len = 0;
  return nullptr;
}

void *BMO_iter_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                         const char *slot_name,
                         const char restrictmask,
                         int *r_len,
                         /* optional args to avoid an alloc (normally stack array) */
                         void **stack_array,
                         int stack_array_size)
{
  BMOIter iter;
  BMElem *ele;
  const int slot_len = BMO_slot_buffer_len(slot_args, slot_name);

  BLI_assert(stack_array_size == 0 || (stack_array_size && stack_array));

  if ((ele = static_cast<BMElem *>(BMO_iter_new(&iter, slot_args, slot_name, restrictmask))) &&
      slot_len > 0)
  {
    BMElem **array = slot_len > stack_array_size ?
                         static_cast<BMElem **>(MEM_mallocN(sizeof(ele) * slot_len, __func__)) :
                         reinterpret_cast<BMElem **>(stack_array);
    int i = 0;

    do {
      array[i++] = ele;
    } while ((ele = static_cast<BMElem *>(BMO_iter_step(&iter))));
    BLI_assert(i <= slot_len);

    if (i != slot_len) {
      if ((void **)array != stack_array) {
        array = static_cast<BMElem **>(MEM_reallocN(array, sizeof(ele) * i));
      }
    }
    *r_len = i;
    return array;
  }

  *r_len = 0;
  return nullptr;
}

int BM_iter_mesh_bitmap_from_filter(const char itype,
                                    BMesh *bm,
                                    blender::MutableBitSpan bitmap,
                                    bool (*test_fn)(BMElem *, void *user_data),
                                    void *user_data)
{
  BMIter iter;
  BMElem *ele;
  int i;
  int bitmap_enabled = 0;

  BM_ITER_MESH_INDEX (ele, &iter, bm, itype, i) {
    if (test_fn(ele, user_data)) {
      bitmap[i].set();
      bitmap_enabled++;
    }
    else {
      bitmap[i].reset();
    }
  }

  return bitmap_enabled;
}

int BM_iter_mesh_bitmap_from_filter_tessface(BMesh *bm,
                                             blender::MutableBitSpan bitmap,
                                             bool (*test_fn)(BMFace *, void *user_data),
                                             void *user_data)
{
  BMIter iter;
  BMFace *f;
  int i;
  int j = 0;
  int bitmap_enabled = 0;

  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    if (test_fn(f, user_data)) {
      for (int tri = 2; tri < f->len; tri++) {
        bitmap[j].set();
        bitmap_enabled++;
        j++;
      }
    }
    else {
      for (int tri = 2; tri < f->len; tri++) {
        bitmap[j].reset();
        j++;
      }
    }
  }

  return bitmap_enabled;
}

int BM_iter_elem_count_flag(const char itype, void *data, const char hflag, const bool value)
{
  BMIter iter;
  BMElem *ele;
  int count = 0;

  BM_ITER_ELEM (ele, &iter, data, itype) {
    if (BM_elem_flag_test_bool(ele, hflag) == value) {
      count++;
    }
  }

  return count;
}

int BMO_iter_elem_count_flag(
    BMesh *bm, const char itype, void *data, const short oflag, const bool value)
{
  BMIter iter;
  int count = 0;

  /* loops have no header flags */
  BLI_assert(bm_iter_itype_htype_map[itype] != BM_LOOP);

  switch (bm_iter_itype_htype_map[itype]) {
    case BM_VERT: {
      BMVert *ele;
      BM_ITER_ELEM (ele, &iter, data, itype) {
        if (BMO_vert_flag_test_bool(bm, ele, oflag) == value) {
          count++;
        }
      }
      break;
    }
    case BM_EDGE: {
      BMEdge *ele;
      BM_ITER_ELEM (ele, &iter, data, itype) {
        if (BMO_edge_flag_test_bool(bm, ele, oflag) == value) {
          count++;
        }
      }
      break;
    }
    case BM_FACE: {
      BMFace *ele;
      BM_ITER_ELEM (ele, &iter, data, itype) {
        if (BMO_face_flag_test_bool(bm, ele, oflag) == value) {
          count++;
        }
      }
      break;
    }
  }
  return count;
}

int BM_iter_mesh_count_flag(const char itype, BMesh *bm, const char hflag, const bool value)
{
  BMIter iter;
  BMElem *ele;
  int count = 0;

  BM_ITER_MESH (ele, &iter, bm, itype) {
    if (BM_elem_flag_test_bool(ele, hflag) == value) {
      count++;
    }
  }

  return count;
}

/**
 * Notes on iterator implementation:
 *
 * Iterators keep track of the next element in a sequence.
 * When a step() callback is invoked the current value of 'next'
 * is stored to be returned later and the next variable is incremented.
 *
 * When the end of a sequence is reached, next should always equal nullptr
 *
 * The 'bmiter__' prefix is used because these are used in
 * bmesh_iterators_inine.c but should otherwise be seen as
 * private.
 */

/*
 * VERT OF MESH CALLBACKS
 */

/* see bug #36923 for why we need this,
 * allow adding but not removing, this isn't _totally_ safe since
 * you could add/remove within the same loop, but catches common cases
 */
#ifdef DEBUG
#  define USE_IMMUTABLE_ASSERT
#endif

void bmiter__elem_of_mesh_begin(BMIter__elem_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
  ((BMIter *)iter)->count = BLI_mempool_len(iter->pooliter.pool);
#endif
  BLI_mempool_iternew(iter->pooliter.pool, &iter->pooliter);
}

void *bmiter__elem_of_mesh_step(BMIter__elem_of_mesh *iter)
{
#ifdef USE_IMMUTABLE_ASSERT
  BLI_assert(((BMIter *)iter)->count <= BLI_mempool_len(iter->pooliter.pool));
#endif
  return BLI_mempool_iterstep(&iter->pooliter);
}

#ifdef USE_IMMUTABLE_ASSERT
#  undef USE_IMMUTABLE_ASSERT
#endif

/*
 * EDGE OF VERT CALLBACKS
 */

void bmiter__edge_of_vert_begin(BMIter__edge_of_vert *iter)
{
  if (iter->vdata->e) {
    iter->e_first = iter->vdata->e;
    iter->e_next = iter->vdata->e;
  }
  else {
    iter->e_first = nullptr;
    iter->e_next = nullptr;
  }
}

void *bmiter__edge_of_vert_step(BMIter__edge_of_vert *iter)
{
  BMEdge *e_curr = iter->e_next;

  if (iter->e_next) {
    iter->e_next = bmesh_disk_edge_next(iter->e_next, iter->vdata);
    if (iter->e_next == iter->e_first) {
      iter->e_next = nullptr;
    }
  }

  return e_curr;
}

/*
 * FACE OF VERT CALLBACKS
 */

void bmiter__face_of_vert_begin(BMIter__face_of_vert *iter)
{
  ((BMIter *)iter)->count = bmesh_disk_facevert_count(iter->vdata);
  if (((BMIter *)iter)->count) {
    iter->l_first = bmesh_disk_faceloop_find_first(iter->vdata->e, iter->vdata);
    iter->e_first = iter->l_first->e;
    iter->e_next = iter->e_first;
    iter->l_next = iter->l_first;
  }
  else {
    iter->l_first = iter->l_next = nullptr;
    iter->e_first = iter->e_next = nullptr;
  }
}
void *bmiter__face_of_vert_step(BMIter__face_of_vert *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (((BMIter *)iter)->count && iter->l_next) {
    ((BMIter *)iter)->count--;
    iter->l_next = bmesh_radial_faceloop_find_next(iter->l_next, iter->vdata);
    if (iter->l_next == iter->l_first) {
      iter->e_next = bmesh_disk_faceedge_find_next(iter->e_next, iter->vdata);
      iter->l_first = bmesh_radial_faceloop_find_first(iter->e_next->l, iter->vdata);
      iter->l_next = iter->l_first;
    }
  }

  if (!((BMIter *)iter)->count) {
    iter->l_next = nullptr;
  }

  return l_curr ? l_curr->f : nullptr;
}

/*
 * LOOP OF VERT CALLBACKS
 */

void bmiter__loop_of_vert_begin(BMIter__loop_of_vert *iter)
{
  ((BMIter *)iter)->count = bmesh_disk_facevert_count(iter->vdata);
  if (((BMIter *)iter)->count) {
    iter->l_first = bmesh_disk_faceloop_find_first(iter->vdata->e, iter->vdata);
    iter->e_first = iter->l_first->e;
    iter->e_next = iter->e_first;
    iter->l_next = iter->l_first;
  }
  else {
    iter->l_first = iter->l_next = nullptr;
    iter->e_first = iter->e_next = nullptr;
  }
}
void *bmiter__loop_of_vert_step(BMIter__loop_of_vert *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (((BMIter *)iter)->count) {
    ((BMIter *)iter)->count--;
    iter->l_next = bmesh_radial_faceloop_find_next(iter->l_next, iter->vdata);
    if (iter->l_next == iter->l_first) {
      iter->e_next = bmesh_disk_faceedge_find_next(iter->e_next, iter->vdata);
      iter->l_first = bmesh_radial_faceloop_find_first(iter->e_next->l, iter->vdata);
      iter->l_next = iter->l_first;
    }
  }

  if (!((BMIter *)iter)->count) {
    iter->l_next = nullptr;
  }

  /* nullptr on finish */
  return l_curr;
}

/*
 * LOOP OF EDGE CALLBACKS
 */

void bmiter__loop_of_edge_begin(BMIter__loop_of_edge *iter)
{
  iter->l_first = iter->l_next = iter->edata->l;
}

void *bmiter__loop_of_edge_step(BMIter__loop_of_edge *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (iter->l_next) {
    iter->l_next = iter->l_next->radial_next;
    if (iter->l_next == iter->l_first) {
      iter->l_next = nullptr;
    }
  }

  /* nullptr on finish */
  return l_curr;
}

/*
 * LOOP OF LOOP CALLBACKS
 */

void bmiter__loop_of_loop_begin(BMIter__loop_of_loop *iter)
{
  iter->l_first = iter->ldata;
  iter->l_next = iter->l_first->radial_next;

  if (iter->l_next == iter->l_first) {
    iter->l_next = nullptr;
  }
}

void *bmiter__loop_of_loop_step(BMIter__loop_of_loop *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (iter->l_next) {
    iter->l_next = iter->l_next->radial_next;
    if (iter->l_next == iter->l_first) {
      iter->l_next = nullptr;
    }
  }

  /* nullptr on finish */
  return l_curr;
}

/*
 * FACE OF EDGE CALLBACKS
 */

void bmiter__face_of_edge_begin(BMIter__face_of_edge *iter)
{
  iter->l_first = iter->l_next = iter->edata->l;
}

void *bmiter__face_of_edge_step(BMIter__face_of_edge *iter)
{
  BMLoop *current = iter->l_next;

  if (iter->l_next) {
    iter->l_next = iter->l_next->radial_next;
    if (iter->l_next == iter->l_first) {
      iter->l_next = nullptr;
    }
  }

  return current ? current->f : nullptr;
}

/*
 * VERTS OF EDGE CALLBACKS
 */

void bmiter__vert_of_edge_begin(BMIter__vert_of_edge *iter)
{
  ((BMIter *)iter)->count = 0;
}

void *bmiter__vert_of_edge_step(BMIter__vert_of_edge *iter)
{
  switch (((BMIter *)iter)->count++) {
    case 0:
      return iter->edata->v1;
    case 1:
      return iter->edata->v2;
    default:
      return nullptr;
  }
}

/*
 * VERT OF FACE CALLBACKS
 */

void bmiter__vert_of_face_begin(BMIter__vert_of_face *iter)
{
  iter->l_first = iter->l_next = BM_FACE_FIRST_LOOP(iter->pdata);
}

void *bmiter__vert_of_face_step(BMIter__vert_of_face *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (iter->l_next) {
    iter->l_next = iter->l_next->next;
    if (iter->l_next == iter->l_first) {
      iter->l_next = nullptr;
    }
  }

  return l_curr ? l_curr->v : nullptr;
}

/*
 * EDGE OF FACE CALLBACKS
 */

void bmiter__edge_of_face_begin(BMIter__edge_of_face *iter)
{
  iter->l_first = iter->l_next = BM_FACE_FIRST_LOOP(iter->pdata);
}

void *bmiter__edge_of_face_step(BMIter__edge_of_face *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (iter->l_next) {
    iter->l_next = iter->l_next->next;
    if (iter->l_next == iter->l_first) {
      iter->l_next = nullptr;
    }
  }

  return l_curr ? l_curr->e : nullptr;
}

/*
 * LOOP OF FACE CALLBACKS
 */

void bmiter__loop_of_face_begin(BMIter__loop_of_face *iter)
{
  iter->l_first = iter->l_next = BM_FACE_FIRST_LOOP(iter->pdata);
}

void *bmiter__loop_of_face_step(BMIter__loop_of_face *iter)
{
  BMLoop *l_curr = iter->l_next;

  if (iter->l_next) {
    iter->l_next = iter->l_next->next;
    if (iter->l_next == iter->l_first) {
      iter->l_next = nullptr;
    }
  }

  return l_curr;
}
