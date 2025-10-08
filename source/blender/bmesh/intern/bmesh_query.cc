/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * This file contains functions for answering common
 * Topological and geometric queries about a mesh, such
 * as, "What is the angle between these two faces?" or,
 * "How many faces are incident upon this vertex?" Tool
 * authors should use the functions in this file instead
 * of inspecting the mesh structure directly.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math_base.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines_stack.h"

#include "BKE_customdata.hh"

#include "bmesh.hh"
#include "intern/bmesh_private.hh"

BMLoop *BM_face_other_edge_loop(BMFace *f, BMEdge *e, BMVert *v)
{
  BMLoop *l = BM_face_edge_share_loop(f, e);
  BLI_assert(l != nullptr);
  return BM_loop_other_edge_loop(l, v);
}

BMLoop *BM_loop_other_edge_loop(BMLoop *l, BMVert *v)
{
  BLI_assert(BM_vert_in_edge(l->e, v));
  return l->v == v ? l->prev : l->next;
}

BMLoop *BM_face_other_vert_loop(BMFace *f, BMVert *v_prev, BMVert *v)
{
  BMLoop *l_iter = BM_face_vert_share_loop(f, v);

  BLI_assert(BM_edge_exists(v_prev, v) != nullptr);

  if (l_iter) {
    if (l_iter->prev->v == v_prev) {
      return l_iter->next;
    }
    if (l_iter->next->v == v_prev) {
      return l_iter->prev;
    }
    /* invalid args */
    BLI_assert(0);
    return nullptr;
  }
  /* invalid args */
  BLI_assert(0);
  return nullptr;
}

BMLoop *BM_loop_other_vert_loop(BMLoop *l, BMVert *v)
{
#if 0 /* works but slow */
  return BM_face_other_vert_loop(l->f, BM_edge_other_vert(l->e, v), v);
#else
  BMEdge *e = l->e;
  BMVert *v_prev = BM_edge_other_vert(e, v);
  if (l->v == v) {
    if (l->prev->v == v_prev) {
      return l->next;
    }
    BLI_assert(l->next->v == v_prev);

    return l->prev;
  }
  BLI_assert(l->v == v_prev);

  if (l->prev->v == v) {
    return l->prev->prev;
  }
  BLI_assert(l->next->v == v);
  return l->next->next;
#endif
}

BMLoop *BM_loop_other_vert_loop_by_edge(BMLoop *l, BMEdge *e)
{
  BLI_assert(BM_vert_in_edge(e, l->v));
  if (l->e == e) {
    return l->next;
  }
  if (l->prev->e == e) {
    return l->prev;
  }

  BLI_assert(0);
  return nullptr;
}

bool BM_vert_pair_share_face_check(BMVert *v_a, BMVert *v_b)
{
  if (v_a->e && v_b->e) {
    BMIter iter;
    BMFace *f;

    BM_ITER_ELEM (f, &iter, v_a, BM_FACES_OF_VERT) {
      if (BM_vert_in_face(v_b, f)) {
        return true;
      }
    }
  }

  return false;
}

bool BM_vert_pair_share_face_check_cb(BMVert *v_a,
                                      BMVert *v_b,
                                      bool (*test_fn)(BMFace *, void *user_data),
                                      void *user_data)
{
  if (v_a->e && v_b->e) {
    BMIter iter;
    BMFace *f;

    BM_ITER_ELEM (f, &iter, v_a, BM_FACES_OF_VERT) {
      if (test_fn(f, user_data)) {
        if (BM_vert_in_face(v_b, f)) {
          return true;
        }
      }
    }
  }

  return false;
}

BMFace *BM_vert_pair_shared_face_cb(BMVert *v_a,
                                    BMVert *v_b,
                                    const bool allow_adjacent,
                                    bool (*callback)(BMFace *, BMLoop *, BMLoop *, void *userdata),
                                    void *user_data,
                                    BMLoop **r_l_a,
                                    BMLoop **r_l_b)
{
  if (v_a->e && v_b->e) {
    BMIter iter;
    BMLoop *l_a, *l_b;

    BM_ITER_ELEM (l_a, &iter, v_a, BM_LOOPS_OF_VERT) {
      BMFace *f = l_a->f;
      l_b = BM_face_vert_share_loop(f, v_b);
      if (l_b && (allow_adjacent || !BM_loop_is_adjacent(l_a, l_b)) &&
          callback(f, l_a, l_b, user_data))
      {
        *r_l_a = l_a;
        *r_l_b = l_b;

        return f;
      }
    }
  }

  return nullptr;
}

BMFace *BM_vert_pair_share_face_by_len(
    BMVert *v_a, BMVert *v_b, BMLoop **r_l_a, BMLoop **r_l_b, const bool allow_adjacent)
{
  BMLoop *l_cur_a = nullptr, *l_cur_b = nullptr;
  BMFace *f_cur = nullptr;

  if (v_a->e && v_b->e) {
    BMIter iter;
    BMLoop *l_a, *l_b;

    BM_ITER_ELEM (l_a, &iter, v_a, BM_LOOPS_OF_VERT) {
      if ((f_cur == nullptr) || (l_a->f->len < f_cur->len)) {
        l_b = BM_face_vert_share_loop(l_a->f, v_b);
        if (l_b && (allow_adjacent || !BM_loop_is_adjacent(l_a, l_b))) {
          f_cur = l_a->f;
          l_cur_a = l_a;
          l_cur_b = l_b;
        }
      }
    }
  }

  *r_l_a = l_cur_a;
  *r_l_b = l_cur_b;

  return f_cur;
}

BMFace *BM_edge_pair_share_face_by_len(
    BMEdge *e_a, BMEdge *e_b, BMLoop **r_l_a, BMLoop **r_l_b, const bool allow_adjacent)
{
  BMLoop *l_cur_a = nullptr, *l_cur_b = nullptr;
  BMFace *f_cur = nullptr;

  if (e_a->l && e_b->l) {
    BMIter iter;
    BMLoop *l_a, *l_b;

    BM_ITER_ELEM (l_a, &iter, e_a, BM_LOOPS_OF_EDGE) {
      if ((f_cur == nullptr) || (l_a->f->len < f_cur->len)) {
        l_b = BM_face_edge_share_loop(l_a->f, e_b);
        if (l_b && (allow_adjacent || !BM_loop_is_adjacent(l_a, l_b))) {
          f_cur = l_a->f;
          l_cur_a = l_a;
          l_cur_b = l_b;
        }
      }
    }
  }

  *r_l_a = l_cur_a;
  *r_l_b = l_cur_b;

  return f_cur;
}

static float bm_face_calc_split_dot(BMLoop *l_a, BMLoop *l_b)
{
  float no[2][3];

  if ((BM_face_calc_normal_subset(l_a, l_b, no[0]) != 0.0f) &&
      (BM_face_calc_normal_subset(l_b, l_a, no[1]) != 0.0f))
  {
    return dot_v3v3(no[0], no[1]);
  }
  return -1.0f;
}

float BM_loop_point_side_of_loop_test(const BMLoop *l, const float co[3])
{
  const float *axis = l->f->no;
  return dist_signed_squared_to_corner_v3v3v3(co, l->prev->v->co, l->v->co, l->next->v->co, axis);
}

float BM_loop_point_side_of_edge_test(const BMLoop *l, const float co[3])
{
  const float *axis = l->f->no;
  float dir[3];
  float plane[4];

  sub_v3_v3v3(dir, l->next->v->co, l->v->co);
  cross_v3_v3v3(plane, axis, dir);

  plane[3] = -dot_v3v3(plane, l->v->co);
  return dist_signed_squared_to_plane_v3(co, plane);
}

BMFace *BM_vert_pair_share_face_by_angle(
    BMVert *v_a, BMVert *v_b, BMLoop **r_l_a, BMLoop **r_l_b, const bool allow_adjacent)
{
  BMLoop *l_cur_a = nullptr, *l_cur_b = nullptr;
  BMFace *f_cur = nullptr;

  if (v_a->e && v_b->e) {
    BMIter iter;
    BMLoop *l_a, *l_b;
    float dot_best = -1.0f;

    BM_ITER_ELEM (l_a, &iter, v_a, BM_LOOPS_OF_VERT) {
      l_b = BM_face_vert_share_loop(l_a->f, v_b);
      if (l_b && (allow_adjacent || !BM_loop_is_adjacent(l_a, l_b))) {

        if (f_cur == nullptr) {
          f_cur = l_a->f;
          l_cur_a = l_a;
          l_cur_b = l_b;
        }
        else {
          /* avoid expensive calculations if we only ever find one face */
          float dot;
          if (dot_best == -1.0f) {
            dot_best = bm_face_calc_split_dot(l_cur_a, l_cur_b);
          }

          dot = bm_face_calc_split_dot(l_a, l_b);
          if (dot > dot_best) {
            dot_best = dot;

            f_cur = l_a->f;
            l_cur_a = l_a;
            l_cur_b = l_b;
          }
        }
      }
    }
  }

  *r_l_a = l_cur_a;
  *r_l_b = l_cur_b;

  return f_cur;
}

BMLoop *BM_vert_find_first_loop(BMVert *v)
{
  return v->e ? bmesh_disk_faceloop_find_first(v->e, v) : nullptr;
}
BMLoop *BM_vert_find_first_loop_visible(BMVert *v)
{
  return v->e ? bmesh_disk_faceloop_find_first_visible(v->e, v) : nullptr;
}

bool BM_vert_in_face(BMVert *v, BMFace *f)
{
  BMLoop *l_iter, *l_first;

#ifdef USE_BMESH_HOLES
  BMLoopList *lst;
  for (lst = f->loops.first; lst; lst = lst->next)
#endif
  {
#ifdef USE_BMESH_HOLES
    l_iter = l_first = lst->first;
#else
    l_iter = l_first = f->l_first;
#endif
    do {
      if (l_iter->v == v) {
        return true;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  return false;
}

int BM_verts_in_face_count(BMVert **varr, int len, BMFace *f)
{
  BMLoop *l_iter, *l_first;

#ifdef USE_BMESH_HOLES
  BMLoopList *lst;
#endif

  int i, count = 0;

  for (i = 0; i < len; i++) {
    BM_ELEM_API_FLAG_ENABLE(varr[i], _FLAG_OVERLAP);
  }

#ifdef USE_BMESH_HOLES
  for (lst = f->loops.first; lst; lst = lst->next)
#endif
  {

#ifdef USE_BMESH_HOLES
    l_iter = l_first = lst->first;
#else
    l_iter = l_first = f->l_first;
#endif

    do {
      if (BM_ELEM_API_FLAG_TEST(l_iter->v, _FLAG_OVERLAP)) {
        count++;
      }

    } while ((l_iter = l_iter->next) != l_first);
  }

  for (i = 0; i < len; i++) {
    BM_ELEM_API_FLAG_DISABLE(varr[i], _FLAG_OVERLAP);
  }

  return count;
}

bool BM_verts_in_face(BMVert **varr, int len, BMFace *f)
{
  BMLoop *l_iter, *l_first;

#ifdef USE_BMESH_HOLES
  BMLoopList *lst;
#endif

  int i;
  bool ok = true;

  /* simple check, we know can't succeed */
  if (f->len < len) {
    return false;
  }

  for (i = 0; i < len; i++) {
    BM_ELEM_API_FLAG_ENABLE(varr[i], _FLAG_OVERLAP);
  }

#ifdef USE_BMESH_HOLES
  for (lst = f->loops.first; lst; lst = lst->next)
#endif
  {

#ifdef USE_BMESH_HOLES
    l_iter = l_first = lst->first;
#else
    l_iter = l_first = f->l_first;
#endif

    do {
      if (BM_ELEM_API_FLAG_TEST(l_iter->v, _FLAG_OVERLAP)) {
        /* pass */
      }
      else {
        ok = false;
        break;
      }

    } while ((l_iter = l_iter->next) != l_first);
  }

  for (i = 0; i < len; i++) {
    BM_ELEM_API_FLAG_DISABLE(varr[i], _FLAG_OVERLAP);
  }

  return ok;
}

bool BM_edge_in_face(const BMEdge *e, const BMFace *f)
{
  if (e->l) {
    const BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      if (l_iter->f == f) {
        return true;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return false;
}

BMLoop *BM_edge_other_loop(BMEdge *e, BMLoop *l)
{
  BMLoop *l_other;

  // BLI_assert(BM_edge_is_manifold(e)); /* Too strict, just check there is another radial face. */
  BLI_assert(e->l && e->l->radial_next != e->l);
  BLI_assert(BM_vert_in_edge(e, l->v));

  l_other = (l->e == e) ? l : l->prev;
  l_other = l_other->radial_next;
  BLI_assert(l_other->e == e);

  if (l_other->v == l->v) {
    /* pass */
  }
  else if (l_other->next->v == l->v) {
    l_other = l_other->next;
  }
  else {
    BLI_assert(0);
  }

  return l_other;
}

BMLoop *BM_vert_step_fan_loop(BMLoop *l, BMEdge **e_step)
{
  BMEdge *e_prev = *e_step;
  BMEdge *e_next;
  if (l->e == e_prev) {
    e_next = l->prev->e;
  }
  else if (l->prev->e == e_prev) {
    e_next = l->e;
  }
  else {
    BLI_assert(0);
    return nullptr;
  }

  if (BM_edge_is_manifold(e_next)) {
    return BM_edge_other_loop((*e_step = e_next), l);
  }
  return nullptr;
}

BMEdge *BM_vert_other_disk_edge(BMVert *v, BMEdge *e_first)
{
  BMLoop *l_a;
  int tot = 0;
  int i;

  BLI_assert(BM_vert_in_edge(e_first, v));

  l_a = e_first->l;
  do {
    l_a = BM_loop_other_vert_loop(l_a, v);
    l_a = BM_vert_in_edge(l_a->e, v) ? l_a : l_a->prev;
    if (BM_edge_is_manifold(l_a->e)) {
      l_a = l_a->radial_next;
    }
    else {
      return nullptr;
    }

    tot++;
  } while (l_a != e_first->l);

  /* we know the total, now loop half way */
  tot /= 2;
  i = 0;

  l_a = e_first->l;
  do {
    if (i == tot) {
      l_a = BM_vert_in_edge(l_a->e, v) ? l_a : l_a->prev;
      return l_a->e;
    }

    l_a = BM_loop_other_vert_loop(l_a, v);
    l_a = BM_vert_in_edge(l_a->e, v) ? l_a : l_a->prev;
    if (BM_edge_is_manifold(l_a->e)) {
      l_a = l_a->radial_next;
    }
    /* this won't have changed from the previous loop */

    i++;
  } while (l_a != e_first->l);

  return nullptr;
}

float BM_edge_calc_length(const BMEdge *e)
{
  return len_v3v3(e->v1->co, e->v2->co);
}

float BM_edge_calc_length_squared(const BMEdge *e)
{
  return len_squared_v3v3(e->v1->co, e->v2->co);
}

bool BM_edge_face_pair(BMEdge *e, BMFace **r_fa, BMFace **r_fb)
{
  BMLoop *la, *lb;

  if ((la = e->l) && (lb = la->radial_next) && (la != lb) && (lb->radial_next == la)) {
    *r_fa = la->f;
    *r_fb = lb->f;
    return true;
  }

  *r_fa = nullptr;
  *r_fb = nullptr;
  return false;
}

bool BM_edge_loop_pair(BMEdge *e, BMLoop **r_la, BMLoop **r_lb)
{
  BMLoop *la, *lb;

  if ((la = e->l) && (lb = la->radial_next) && (la != lb) && (lb->radial_next == la)) {
    *r_la = la;
    *r_lb = lb;
    return true;
  }

  *r_la = nullptr;
  *r_lb = nullptr;
  return false;
}

bool BM_vert_is_edge_pair(const BMVert *v)
{
  const BMEdge *e = v->e;
  if (e) {
    BMEdge *e_other = BM_DISK_EDGE_NEXT(e, v);
    return ((e_other != e) && (BM_DISK_EDGE_NEXT(e_other, v) == e));
  }
  return false;
}

bool BM_vert_is_edge_pair_manifold(const BMVert *v)
{
  const BMEdge *e = v->e;
  if (e) {
    BMEdge *e_other = BM_DISK_EDGE_NEXT(e, v);
    if ((e_other != e) && (BM_DISK_EDGE_NEXT(e_other, v) == e)) {
      return BM_edge_is_manifold(e) && BM_edge_is_manifold(e_other);
    }
  }
  return false;
}

bool BM_vert_edge_pair(const BMVert *v, BMEdge **r_e_a, BMEdge **r_e_b)
{
  BMEdge *e_a = v->e;
  if (e_a) {
    BMEdge *e_b = BM_DISK_EDGE_NEXT(e_a, v);
    if ((e_b != e_a) && (BM_DISK_EDGE_NEXT(e_b, v) == e_a)) {
      *r_e_a = e_a;
      *r_e_b = e_b;
      return true;
    }
  }

  *r_e_a = nullptr;
  *r_e_b = nullptr;
  return false;
}

int BM_vert_edge_count(const BMVert *v)
{
  return bmesh_disk_count(v);
}

int BM_vert_edge_count_at_most(const BMVert *v, const int count_max)
{
  return bmesh_disk_count_at_most(v, count_max);
}

int BM_vert_edge_count_nonwire(const BMVert *v)
{
  int count = 0;
  BMIter eiter;
  BMEdge *edge;
  BM_ITER_ELEM (edge, &eiter, (BMVert *)v, BM_EDGES_OF_VERT) {
    if (edge->l) {
      count++;
    }
  }
  return count;
}
int BM_edge_face_count(const BMEdge *e)
{
  int count = 0;

  if (e->l) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      count++;
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return count;
}

int BM_edge_face_count_at_most(const BMEdge *e, const int count_max)
{
  int count = 0;

  if (e->l) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      count++;
      if (count == count_max) {
        break;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return count;
}

int BM_vert_face_count(const BMVert *v)
{
  return bmesh_disk_facevert_count(v);
}

int BM_vert_face_count_at_most(const BMVert *v, int count_max)
{
  return bmesh_disk_facevert_count_at_most(v, count_max);
}

bool BM_vert_face_check(const BMVert *v)
{
  if (v->e != nullptr) {
    const BMEdge *e_iter, *e_first;
    e_first = e_iter = v->e;
    do {
      if (e_iter->l != nullptr) {
        return true;
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
  }
  return false;
}

bool BM_vert_is_wire(const BMVert *v)
{
  if (v->e) {
    BMEdge *e_first, *e_iter;

    e_first = e_iter = v->e;
    do {
      if (e_iter->l) {
        return false;
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);

    return true;
  }
  return false;
}

bool BM_vert_is_manifold(const BMVert *v)
{
  BMEdge *e_iter, *e_first, *e_prev;
  BMLoop *l_iter, *l_first;
  int loop_num = 0, loop_num_region = 0, boundary_num = 0;

  if (v->e == nullptr) {
    /* loose vert */
    return false;
  }

  /* count edges while looking for non-manifold edges */
  e_first = e_iter = v->e;
  /* may be null */
  l_first = e_iter->l;
  do {
    /* loose edge or edge shared by more than two faces,
     * edges with 1 face user are OK, otherwise we could
     * use BM_edge_is_manifold() here */
    if (e_iter->l == nullptr || (e_iter->l != e_iter->l->radial_next->radial_next)) {
      return false;
    }

    /* count radial loops */
    if (e_iter->l->v == v) {
      loop_num += 1;
    }

    if (!BM_edge_is_boundary(e_iter)) {
      /* non boundary check opposite loop */
      if (e_iter->l->radial_next->v == v) {
        loop_num += 1;
      }
    }
    else {
      /* start at the boundary */
      l_first = e_iter->l;
      boundary_num += 1;
      /* >2 boundaries can't be manifold */
      if (boundary_num == 3) {
        return false;
      }
    }
  } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);

  e_first = l_first->e;
  l_first = (l_first->v == v) ? l_first : l_first->next;
  BLI_assert(l_first->v == v);

  l_iter = l_first;
  e_prev = e_first;

  do {
    loop_num_region += 1;
  } while (((l_iter = BM_vert_step_fan_loop(l_iter, &e_prev)) != l_first) && (l_iter != nullptr));

  return (loop_num == loop_num_region);
}

#define LOOP_VISIT _FLAG_WALK
#define EDGE_VISIT _FLAG_WALK

static int bm_loop_region_count__recursive(BMEdge *e, BMVert *v)
{
  BMLoop *l_iter, *l_first;
  int count = 0;

  BLI_assert(!BM_ELEM_API_FLAG_TEST(e, EDGE_VISIT));
  BM_ELEM_API_FLAG_ENABLE(e, EDGE_VISIT);

  l_iter = l_first = e->l;
  do {
    if (l_iter->v == v) {
      BMEdge *e_other = l_iter->prev->e;
      if (!BM_ELEM_API_FLAG_TEST(l_iter, LOOP_VISIT)) {
        BM_ELEM_API_FLAG_ENABLE(l_iter, LOOP_VISIT);
        count += 1;
      }
      if (!BM_ELEM_API_FLAG_TEST(e_other, EDGE_VISIT)) {
        count += bm_loop_region_count__recursive(e_other, v);
      }
    }
    else if (l_iter->next->v == v) {
      BMEdge *e_other = l_iter->next->e;
      if (!BM_ELEM_API_FLAG_TEST(l_iter->next, LOOP_VISIT)) {
        BM_ELEM_API_FLAG_ENABLE(l_iter->next, LOOP_VISIT);
        count += 1;
      }
      if (!BM_ELEM_API_FLAG_TEST(e_other, EDGE_VISIT)) {
        count += bm_loop_region_count__recursive(e_other, v);
      }
    }
    else {
      BLI_assert(0);
    }
  } while ((l_iter = l_iter->radial_next) != l_first);

  return count;
}

static int bm_loop_region_count__clear(BMLoop *l)
{
  int count = 0;
  BMEdge *e_iter, *e_first;

  /* clear flags */
  e_iter = e_first = l->e;
  do {
    BM_ELEM_API_FLAG_DISABLE(e_iter, EDGE_VISIT);
    if (e_iter->l) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = e_iter->l;
      do {
        if (l_iter->v == l->v) {
          BM_ELEM_API_FLAG_DISABLE(l_iter, LOOP_VISIT);
          count += 1;
        }
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
  } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, l->v)) != e_first);

  return count;
}

int BM_loop_region_loops_count_at_most(BMLoop *l, int *r_loop_total)
{
  const int count = bm_loop_region_count__recursive(l->e, l->v);
  const int count_total = bm_loop_region_count__clear(l);
  if (r_loop_total) {
    *r_loop_total = count_total;
  }
  return count;
}

#undef LOOP_VISIT
#undef EDGE_VISIT

int BM_loop_region_loops_count(BMLoop *l)
{
  return BM_loop_region_loops_count_at_most(l, nullptr);
}

bool BM_vert_is_manifold_region(const BMVert *v)
{
  BMLoop *l_first = BM_vert_find_first_loop((BMVert *)v);
  if (l_first) {
    int count, count_total;
    count = BM_loop_region_loops_count_at_most(l_first, &count_total);
    return (count == count_total);
  }
  return true;
}

bool BM_edge_is_convex(const BMEdge *e)
{
  if (BM_edge_is_manifold(e)) {
    BMLoop *l1 = e->l;
    BMLoop *l2 = e->l->radial_next;
    if (!equals_v3v3(l1->f->no, l2->f->no)) {
      float cross[3];
      float l_dir[3];
      cross_v3_v3v3(cross, l1->f->no, l2->f->no);
      /* we assume contiguous normals, otherwise the result isn't meaningful */
      sub_v3_v3v3(l_dir, l1->next->v->co, l1->v->co);
      return (dot_v3v3(l_dir, cross) > 0.0f);
    }
  }
  return true;
}

bool BM_edge_is_contiguous_loop_cd(const BMEdge *e,
                                   const int cd_loop_type,
                                   const int cd_loop_offset)
{
  BLI_assert(cd_loop_offset != -1);

  if (e->l && e->l->radial_next != e->l) {
    const BMLoop *l_base_v1 = e->l;
    const BMLoop *l_base_v2 = e->l->next;
    const void *l_base_cd_v1 = BM_ELEM_CD_GET_VOID_P(l_base_v1, cd_loop_offset);
    const void *l_base_cd_v2 = BM_ELEM_CD_GET_VOID_P(l_base_v2, cd_loop_offset);
    const BMLoop *l_iter = e->l->radial_next;
    do {
      const BMLoop *l_iter_v1;
      const BMLoop *l_iter_v2;
      const void *l_iter_cd_v1;
      const void *l_iter_cd_v2;

      if (l_iter->v == l_base_v1->v) {
        l_iter_v1 = l_iter;
        l_iter_v2 = l_iter->next;
      }
      else {
        l_iter_v1 = l_iter->next;
        l_iter_v2 = l_iter;
      }
      BLI_assert((l_iter_v1->v == l_base_v1->v) && (l_iter_v2->v == l_base_v2->v));

      l_iter_cd_v1 = BM_ELEM_CD_GET_VOID_P(l_iter_v1, cd_loop_offset);
      l_iter_cd_v2 = BM_ELEM_CD_GET_VOID_P(l_iter_v2, cd_loop_offset);

      if ((CustomData_data_equals(eCustomDataType(cd_loop_type), l_base_cd_v1, l_iter_cd_v1) ==
           0) ||
          (CustomData_data_equals(eCustomDataType(cd_loop_type), l_base_cd_v2, l_iter_cd_v2) == 0))
      {
        return false;
      }

    } while ((l_iter = l_iter->radial_next) != e->l);
  }
  return true;
}

bool BM_vert_is_boundary(const BMVert *v)
{
  if (v->e) {
    BMEdge *e_first, *e_iter;

    e_first = e_iter = v->e;
    do {
      if (BM_edge_is_boundary(e_iter)) {
        return true;
      }
    } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);

    return false;
  }
  return false;
}

int BM_face_share_face_count(BMFace *f_a, BMFace *f_b)
{
  BMIter iter1, iter2;
  BMEdge *e;
  BMFace *f;
  int count = 0;

  BM_ITER_ELEM (e, &iter1, f_a, BM_EDGES_OF_FACE) {
    BM_ITER_ELEM (f, &iter2, e, BM_FACES_OF_EDGE) {
      if (f != f_a && f != f_b && BM_face_share_edge_check(f, f_b)) {
        count++;
      }
    }
  }

  return count;
}

bool BM_face_share_face_check(BMFace *f_a, BMFace *f_b)
{
  BMIter iter1, iter2;
  BMEdge *e;
  BMFace *f;

  BM_ITER_ELEM (e, &iter1, f_a, BM_EDGES_OF_FACE) {
    BM_ITER_ELEM (f, &iter2, e, BM_FACES_OF_EDGE) {
      if (f != f_a && f != f_b && BM_face_share_edge_check(f, f_b)) {
        return true;
      }
    }
  }

  return false;
}

int BM_face_share_edge_count(BMFace *f_a, BMFace *f_b)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  int count = 0;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
  do {
    if (BM_edge_in_face(l_iter->e, f_b)) {
      count++;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return count;
}

bool BM_face_share_edge_check(BMFace *f_a, BMFace *f_b)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
  do {
    if (BM_edge_in_face(l_iter->e, f_b)) {
      return true;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return false;
}

int BM_face_share_vert_count(BMFace *f_a, BMFace *f_b)
{
  BMLoop *l_iter;
  BMLoop *l_first;
  int count = 0;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
  do {
    if (BM_vert_in_face(l_iter->v, f_b)) {
      count++;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return count;
}

bool BM_face_share_vert_check(BMFace *f_a, BMFace *f_b)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
  do {
    if (BM_vert_in_face(l_iter->v, f_b)) {
      return true;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return false;
}

bool BM_loop_share_edge_check(BMLoop *l_a, BMLoop *l_b)
{
  BLI_assert(l_a->v == l_b->v);
  return (ELEM(l_a->e, l_b->e, l_b->prev->e) || ELEM(l_b->e, l_a->e, l_a->prev->e));
}

bool BM_edge_share_face_check(BMEdge *e1, BMEdge *e2)
{
  BMLoop *l;
  BMFace *f;

  if (e1->l && e2->l) {
    l = e1->l;
    do {
      f = l->f;
      if (BM_edge_in_face(e2, f)) {
        return true;
      }
      l = l->radial_next;
    } while (l != e1->l);
  }
  return false;
}

bool BM_edge_share_quad_check(BMEdge *e1, BMEdge *e2)
{
  BMLoop *l;
  BMFace *f;

  if (e1->l && e2->l) {
    l = e1->l;
    do {
      f = l->f;
      if (f->len == 4) {
        if (BM_edge_in_face(e2, f)) {
          return true;
        }
      }
      l = l->radial_next;
    } while (l != e1->l);
  }
  return false;
}

bool BM_edge_share_vert_check(BMEdge *e1, BMEdge *e2)
{
  return (e1->v1 == e2->v1 || e1->v1 == e2->v2 || e1->v2 == e2->v1 || e1->v2 == e2->v2);
}

BMVert *BM_edge_share_vert(BMEdge *e1, BMEdge *e2)
{
  BLI_assert(e1 != e2);
  if (BM_vert_in_edge(e2, e1->v1)) {
    return e1->v1;
  }
  if (BM_vert_in_edge(e2, e1->v2)) {
    return e1->v2;
  }
  return nullptr;
}

BMLoop *BM_edge_vert_share_loop(BMLoop *l, BMVert *v)
{
  BLI_assert(BM_vert_in_edge(l->e, v));
  if (l->v == v) {
    return l;
  }
  return l->next;
}

BMLoop *BM_face_vert_share_loop(BMFace *f, BMVert *v)
{
  BMLoop *l_first;
  BMLoop *l_iter;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (l_iter->v == v) {
      return l_iter;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return nullptr;
}

BMLoop *BM_face_edge_share_loop(BMFace *f, BMEdge *e)
{
  BMLoop *l_first;
  BMLoop *l_iter;

  l_iter = l_first = e->l;
  do {
    if (l_iter->f == f) {
      return l_iter;
    }
  } while ((l_iter = l_iter->radial_next) != l_first);

  return nullptr;
}

void BM_edge_ordered_verts_ex(const BMEdge *edge,
                              BMVert **r_v1,
                              BMVert **r_v2,
                              const BMLoop *edge_loop)
{
  BLI_assert(edge_loop->e == edge);
  (void)edge; /* quiet warning in release build */
  *r_v1 = edge_loop->v;
  *r_v2 = edge_loop->next->v;
}

void BM_edge_ordered_verts(const BMEdge *edge, BMVert **r_v1, BMVert **r_v2)
{
  BM_edge_ordered_verts_ex(edge, r_v1, r_v2, edge->l);
}

BMLoop *BM_loop_find_prev_nodouble(BMLoop *l, BMLoop *l_stop, const float eps_sq)
{
  BMLoop *l_step = l->prev;

  BLI_assert(!ELEM(l_stop, nullptr, l));

  while (UNLIKELY(len_squared_v3v3(l->v->co, l_step->v->co) < eps_sq)) {
    l_step = l_step->prev;
    BLI_assert(l_step != l);
    if (UNLIKELY(l_step == l_stop)) {
      return nullptr;
    }
  }

  return l_step;
}

BMLoop *BM_loop_find_next_nodouble(BMLoop *l, BMLoop *l_stop, const float eps_sq)
{
  BMLoop *l_step = l->next;

  BLI_assert(!ELEM(l_stop, nullptr, l));

  while (UNLIKELY(len_squared_v3v3(l->v->co, l_step->v->co) < eps_sq)) {
    l_step = l_step->next;
    BLI_assert(l_step != l);
    if (UNLIKELY(l_step == l_stop)) {
      return nullptr;
    }
  }

  return l_step;
}

bool BM_loop_is_convex(const BMLoop *l)
{
  float e_dir_prev[3];
  float e_dir_next[3];
  float l_no[3];

  sub_v3_v3v3(e_dir_prev, l->prev->v->co, l->v->co);
  sub_v3_v3v3(e_dir_next, l->next->v->co, l->v->co);
  cross_v3_v3v3(l_no, e_dir_next, e_dir_prev);
  return dot_v3v3(l_no, l->f->no) > 0.0f;
}

float BM_loop_calc_face_angle(const BMLoop *l)
{
  return angle_v3v3v3(l->prev->v->co, l->v->co, l->next->v->co);
}

float BM_loop_calc_face_normal_safe_ex(const BMLoop *l, const float epsilon_sq, float r_normal[3])
{
  /* NOTE: we cannot use result of normal_tri_v3 here to detect colinear vectors
   * (vertex on a straight line) from zero value,
   * because it does not normalize both vectors before making cross-product.
   * Instead of adding two costly normalize computations,
   * just check ourselves for colinear case. */
  /* NOTE: FEPSILON might need some finer tweaking at some point?
   * Seems to be working OK for now though. */
  float v1[3], v2[3], v_tmp[3];
  sub_v3_v3v3(v1, l->prev->v->co, l->v->co);
  sub_v3_v3v3(v2, l->next->v->co, l->v->co);

  const float fac = ((v2[0] == 0.0f) ?
                         ((v2[1] == 0.0f) ? ((v2[2] == 0.0f) ? 0.0f : v1[2] / v2[2]) :
                                            v1[1] / v2[1]) :
                         v1[0] / v2[0]);

  mul_v3_v3fl(v_tmp, v2, fac);
  sub_v3_v3(v_tmp, v1);
  if (fac != 0.0f && !is_zero_v3(v1) && len_squared_v3(v_tmp) > epsilon_sq) {
    /* Not co-linear, we can compute cross-product and normalize it into normal. */
    cross_v3_v3v3(r_normal, v1, v2);
    return normalize_v3(r_normal);
  }
  copy_v3_v3(r_normal, l->f->no);
  return 0.0f;
}

float BM_loop_calc_face_normal_safe_vcos_ex(const BMLoop *l,
                                            const float normal_fallback[3],
                                            float const (*vertexCos)[3],
                                            const float epsilon_sq,
                                            float r_normal[3])
{
  const int i_prev = BM_elem_index_get(l->prev->v);
  const int i_next = BM_elem_index_get(l->next->v);
  const int i = BM_elem_index_get(l->v);

  float v1[3], v2[3], v_tmp[3];
  sub_v3_v3v3(v1, vertexCos[i_prev], vertexCos[i]);
  sub_v3_v3v3(v2, vertexCos[i_next], vertexCos[i]);

  const float fac = ((v2[0] == 0.0f) ?
                         ((v2[1] == 0.0f) ? ((v2[2] == 0.0f) ? 0.0f : v1[2] / v2[2]) :
                                            v1[1] / v2[1]) :
                         v1[0] / v2[0]);

  mul_v3_v3fl(v_tmp, v2, fac);
  sub_v3_v3(v_tmp, v1);
  if (fac != 0.0f && !is_zero_v3(v1) && len_squared_v3(v_tmp) > epsilon_sq) {
    /* Not co-linear, we can compute cross-product and normalize it into normal. */
    cross_v3_v3v3(r_normal, v1, v2);
    return normalize_v3(r_normal);
  }
  copy_v3_v3(r_normal, normal_fallback);
  return 0.0f;
}

float BM_loop_calc_face_normal_safe(const BMLoop *l, float r_normal[3])
{
  return BM_loop_calc_face_normal_safe_ex(l, 1e-5f, r_normal);
}

float BM_loop_calc_face_normal_safe_vcos(const BMLoop *l,
                                         const float normal_fallback[3],
                                         float const (*vertexCos)[3],
                                         float r_normal[3])

{
  return BM_loop_calc_face_normal_safe_vcos_ex(l, normal_fallback, vertexCos, 1e-5f, r_normal);
}

float BM_loop_calc_face_normal(const BMLoop *l, float r_normal[3])
{
  float v1[3], v2[3];
  sub_v3_v3v3(v1, l->prev->v->co, l->v->co);
  sub_v3_v3v3(v2, l->next->v->co, l->v->co);

  cross_v3_v3v3(r_normal, v1, v2);
  const float len = normalize_v3(r_normal);
  if (UNLIKELY(len == 0.0f)) {
    copy_v3_v3(r_normal, l->f->no);
  }
  return len;
}

void BM_loop_calc_face_direction(const BMLoop *l, float r_dir[3])
{
  float v_prev[3];
  float v_next[3];

  sub_v3_v3v3(v_prev, l->v->co, l->prev->v->co);
  sub_v3_v3v3(v_next, l->next->v->co, l->v->co);

  normalize_v3(v_prev);
  normalize_v3(v_next);

  add_v3_v3v3(r_dir, v_prev, v_next);
  normalize_v3(r_dir);
}

void BM_loop_calc_face_tangent(const BMLoop *l, float r_tangent[3])
{
  float v_prev[3];
  float v_next[3];
  float dir[3];

  sub_v3_v3v3(v_prev, l->prev->v->co, l->v->co);
  sub_v3_v3v3(v_next, l->v->co, l->next->v->co);

  normalize_v3(v_prev);
  normalize_v3(v_next);
  add_v3_v3v3(dir, v_prev, v_next);

  if (compare_v3v3(v_prev, v_next, FLT_EPSILON * 10.0f) == false) {
    float nor[3]; /* for this purpose doesn't need to be normalized */
    cross_v3_v3v3(nor, v_prev, v_next);
    /* concave face check */
    if (UNLIKELY(dot_v3v3(nor, l->f->no) < 0.0f)) {
      negate_v3(nor);
    }
    cross_v3_v3v3(r_tangent, dir, nor);
  }
  else {
    /* prev/next are the same - compare with face normal since we don't have one */
    cross_v3_v3v3(r_tangent, dir, l->f->no);
  }

  normalize_v3(r_tangent);
}

float BM_edge_calc_face_angle_ex(const BMEdge *e, const float fallback)
{
  if (BM_edge_is_manifold(e)) {
    const BMLoop *l1 = e->l;
    const BMLoop *l2 = e->l->radial_next;
    return angle_normalized_v3v3(l1->f->no, l2->f->no);
  }
  return fallback;
}
float BM_edge_calc_face_angle(const BMEdge *e)
{
  return BM_edge_calc_face_angle_ex(e, DEG2RADF(90.0f));
}

float BM_edge_calc_face_angle_with_imat3_ex(const BMEdge *e,
                                            const float imat3[3][3],
                                            const float fallback)
{
  if (BM_edge_is_manifold(e)) {
    const BMLoop *l1 = e->l;
    const BMLoop *l2 = e->l->radial_next;
    float no1[3], no2[3];
    copy_v3_v3(no1, l1->f->no);
    copy_v3_v3(no2, l2->f->no);

    mul_transposed_m3_v3(imat3, no1);
    mul_transposed_m3_v3(imat3, no2);

    normalize_v3(no1);
    normalize_v3(no2);

    return angle_normalized_v3v3(no1, no2);
  }
  return fallback;
}
float BM_edge_calc_face_angle_with_imat3(const BMEdge *e, const float imat3[3][3])
{
  return BM_edge_calc_face_angle_with_imat3_ex(e, imat3, DEG2RADF(90.0f));
}

float BM_edge_calc_face_angle_signed_ex(const BMEdge *e, const float fallback)
{
  if (BM_edge_is_manifold(e)) {
    BMLoop *l1 = e->l;
    BMLoop *l2 = e->l->radial_next;
    const float angle = angle_normalized_v3v3(l1->f->no, l2->f->no);
    return BM_edge_is_convex(e) ? angle : -angle;
  }
  return fallback;
}
float BM_edge_calc_face_angle_signed(const BMEdge *e)
{
  return BM_edge_calc_face_angle_signed_ex(e, DEG2RADF(90.0f));
}

void BM_edge_calc_face_tangent(const BMEdge *e, const BMLoop *e_loop, float r_tangent[3])
{
  float tvec[3];
  BMVert *v1, *v2;
  BM_edge_ordered_verts_ex(e, &v1, &v2, e_loop);

  sub_v3_v3v3(tvec, v1->co, v2->co); /* use for temp storage */
  /* NOTE: we could average the tangents of both loops,
   * for non flat ngons it will give a better direction */
  cross_v3_v3v3(r_tangent, tvec, e_loop->f->no);
  normalize_v3(r_tangent);
}

float BM_vert_calc_edge_angle_ex(const BMVert *v, const float fallback)
{
  BMEdge *e1, *e2;

  /* Saves `BM_vert_edge_count(v)` and edge iterator,
   * get the edges and count them both at once. */

  if ((e1 = v->e) && (e2 = bmesh_disk_edge_next(e1, v)) && (e1 != e2) &&
      /* make sure we come full circle and only have 2 connected edges */
      (e1 == bmesh_disk_edge_next(e2, v)))
  {
    BMVert *v1 = BM_edge_other_vert(e1, v);
    BMVert *v2 = BM_edge_other_vert(e2, v);

    return float(M_PI) - angle_v3v3v3(v1->co, v->co, v2->co);
  }
  return fallback;
}

float BM_vert_calc_edge_angle(const BMVert *v)
{
  return BM_vert_calc_edge_angle_ex(v, DEG2RADF(90.0f));
}

float BM_vert_calc_shell_factor(const BMVert *v)
{
  BMIter iter;
  BMLoop *l;
  float accum_shell = 0.0f;
  float accum_angle = 0.0f;

  BM_ITER_ELEM (l, &iter, (BMVert *)v, BM_LOOPS_OF_VERT) {
    const float face_angle = BM_loop_calc_face_angle(l);
    accum_shell += shell_v3v3_normalized_to_dist(v->no, l->f->no) * face_angle;
    accum_angle += face_angle;
  }

  if (accum_angle != 0.0f) {
    return accum_shell / accum_angle;
  }
  return 1.0f;
}
float BM_vert_calc_shell_factor_ex(const BMVert *v, const float no[3], const char hflag)
{
  BMIter iter;
  const BMLoop *l;
  float accum_shell = 0.0f;
  float accum_angle = 0.0f;
  int tot_sel = 0, tot = 0;

  BM_ITER_ELEM (l, &iter, (BMVert *)v, BM_LOOPS_OF_VERT) {
    if (BM_elem_flag_test(l->f, hflag)) { /* <-- main difference to BM_vert_calc_shell_factor! */
      const float face_angle = BM_loop_calc_face_angle(l);
      accum_shell += shell_v3v3_normalized_to_dist(no, l->f->no) * face_angle;
      accum_angle += face_angle;
      tot_sel++;
    }
    tot++;
  }

  if (accum_angle != 0.0f) {
    return accum_shell / accum_angle;
  }
  /* other main difference from BM_vert_calc_shell_factor! */
  if (tot != 0 && tot_sel == 0) {
    /* none selected, so use all */
    return BM_vert_calc_shell_factor(v);
  }
  return 1.0f;
}

float BM_vert_calc_median_tagged_edge_length(const BMVert *v)
{
  BMIter iter;
  BMEdge *e;
  int tot;
  float length = 0.0f;

  BM_ITER_ELEM_INDEX (e, &iter, (BMVert *)v, BM_EDGES_OF_VERT, tot) {
    const BMVert *v_other = BM_edge_other_vert(e, v);
    if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
      length += BM_edge_calc_length(e);
    }
  }

  if (tot) {
    return length / float(tot);
  }
  return 0.0f;
}

BMLoop *BM_face_find_shortest_loop(BMFace *f)
{
  float shortest_len = FLT_MAX;

  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *shortest_loop = l_first; /* Fallback for non-finite coordinates, see #108658. */

  do {
    const float len_sq = len_squared_v3v3(l_iter->v->co, l_iter->next->v->co);
    if (len_sq <= shortest_len) {
      shortest_loop = l_iter;
      shortest_len = len_sq;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return shortest_loop;
}

BMLoop *BM_face_find_longest_loop(BMFace *f)
{
  float len_max_sq = 0.0f;

  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *longest_loop = l_first; /* Fallback for non-finite coordinates, see #108658. */

  do {
    const float len_sq = len_squared_v3v3(l_iter->v->co, l_iter->next->v->co);
    if (len_sq >= len_max_sq) {
      longest_loop = l_iter;
      len_max_sq = len_sq;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return longest_loop;
}

/**
 * Returns the edge existing between \a v_a and \a v_b, or nullptr if there isn't one.
 *
 * \note multiple edges may exist between any two vertices, and therefore
 * this function only returns the first one found.
 */
#if 0
BMEdge *BM_edge_exists(BMVert *v_a, BMVert *v_b)
{
  BMIter iter;
  BMEdge *e;

  BLI_assert(v_a != v_b);
  BLI_assert(v_a->head.htype == BM_VERT && v_b->head.htype == BM_VERT);

  BM_ITER_ELEM (e, &iter, v_a, BM_EDGES_OF_VERT) {
    if (e->v1 == v_b || e->v2 == v_b) {
      return e;
    }
  }

  return nullptr;
}
#else
BMEdge *BM_edge_exists(BMVert *v_a, BMVert *v_b)
{
  /* speedup by looping over both edges verts
   * where one vert may connect to many edges but not the other. */

  BMEdge *e_a, *e_b;

  BLI_assert(v_a != v_b);
  BLI_assert(v_a->head.htype == BM_VERT && v_b->head.htype == BM_VERT);

  if ((e_a = v_a->e) && (e_b = v_b->e)) {
    BMEdge *e_a_iter = e_a, *e_b_iter = e_b;

    do {
      if (BM_vert_in_edge(e_a_iter, v_b)) {
        return e_a_iter;
      }
      if (BM_vert_in_edge(e_b_iter, v_a)) {
        return e_b_iter;
      }
    } while (((e_a_iter = bmesh_disk_edge_next(e_a_iter, v_a)) != e_a) &&
             ((e_b_iter = bmesh_disk_edge_next(e_b_iter, v_b)) != e_b));
  }

  return nullptr;
}
#endif

BMEdge *BM_edge_find_double(BMEdge *e)
{
  BMVert *v = e->v1;
  BMVert *v_other = e->v2;

  BMEdge *e_iter;

  e_iter = e;
  while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e) {
    if (UNLIKELY(BM_vert_in_edge(e_iter, v_other))) {
      return e_iter;
    }
  }

  return nullptr;
}

BMLoop *BM_edge_find_first_loop_visible(BMEdge *e)
{
  if (e->l != nullptr) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = e->l;
    do {
      if (!BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        return l_iter;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }
  return nullptr;
}

BMFace *BM_face_exists(BMVert *const *varr, int len)
{
  if (varr[0]->e) {
    BMEdge *e_iter, *e_first;
    e_iter = e_first = varr[0]->e;

    /* would normally use BM_LOOPS_OF_VERT, but this runs so often,
     * its faster to iterate on the data directly */
    do {
      if (e_iter->l) {
        BMLoop *l_iter_radial, *l_first_radial;
        l_iter_radial = l_first_radial = e_iter->l;

        do {
          if ((l_iter_radial->v == varr[0]) && (l_iter_radial->f->len == len)) {
            /* the fist 2 verts match, now check the remaining (len - 2) faces do too
             * winding isn't known, so check in both directions */
            int i_walk = 2;

            if (l_iter_radial->next->v == varr[1]) {
              BMLoop *l_walk = l_iter_radial->next->next;
              do {
                if (l_walk->v != varr[i_walk]) {
                  break;
                }
              } while ((void)(l_walk = l_walk->next), ++i_walk != len);
            }
            else if (l_iter_radial->prev->v == varr[1]) {
              BMLoop *l_walk = l_iter_radial->prev->prev;
              do {
                if (l_walk->v != varr[i_walk]) {
                  break;
                }
              } while ((void)(l_walk = l_walk->prev), ++i_walk != len);
            }

            if (i_walk == len) {
              return l_iter_radial->f;
            }
          }
        } while ((l_iter_radial = l_iter_radial->radial_next) != l_first_radial);
      }
    } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, varr[0])) != e_first);
  }

  return nullptr;
}

BMFace *BM_face_find_double(BMFace *f)
{
  BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
  for (BMLoop *l_iter = l_first->radial_next; l_first != l_iter; l_iter = l_iter->radial_next) {
    if (l_iter->f->len == l_first->f->len) {
      if (l_iter->v == l_first->v) {
        BMLoop *l_a = l_first, *l_b = l_iter, *l_b_init = l_iter;
        do {
          if (l_a->e != l_b->e) {
            break;
          }
        } while (((void)(l_a = l_a->next), (l_b = l_b->next)) != l_b_init);
        if (l_b == l_b_init) {
          return l_iter->f;
        }
      }
      else {
        BMLoop *l_a = l_first, *l_b = l_iter, *l_b_init = l_iter;
        do {
          if (l_a->e != l_b->e) {
            break;
          }
        } while (((void)(l_a = l_a->prev), (l_b = l_b->next)) != l_b_init);
        if (l_b == l_b_init) {
          return l_iter->f;
        }
      }
    }
  }
  return nullptr;
}

bool BM_face_exists_multi(BMVert **varr, BMEdge **earr, int len)
{
  BMFace *f;
  BMEdge *e;
  BMVert *v;
  bool ok;
  int tot_tag;

  BMIter fiter;
  BMIter viter;

  int i;

  for (i = 0; i < len; i++) {
    /* save some time by looping over edge faces rather than vert faces
     * will still loop over some faces twice but not as many */
    BM_ITER_ELEM (f, &fiter, earr[i], BM_FACES_OF_EDGE) {
      BM_elem_flag_disable(f, BM_ELEM_INTERNAL_TAG);
      BM_ITER_ELEM (v, &viter, f, BM_VERTS_OF_FACE) {
        BM_elem_flag_disable(v, BM_ELEM_INTERNAL_TAG);
      }
    }

    /* clear all edge tags */
    BM_ITER_ELEM (e, &fiter, varr[i], BM_EDGES_OF_VERT) {
      BM_elem_flag_disable(e, BM_ELEM_INTERNAL_TAG);
    }
  }

  /* now tag all verts and edges in the boundary array as true so
   * we can know if a face-vert is from our array */
  for (i = 0; i < len; i++) {
    BM_elem_flag_enable(varr[i], BM_ELEM_INTERNAL_TAG);
    BM_elem_flag_enable(earr[i], BM_ELEM_INTERNAL_TAG);
  }

  /* so! boundary is tagged, everything else cleared */

  /* 1) tag all faces connected to edges - if all their verts are boundary */
  tot_tag = 0;
  for (i = 0; i < len; i++) {
    BM_ITER_ELEM (f, &fiter, earr[i], BM_FACES_OF_EDGE) {
      if (!BM_elem_flag_test(f, BM_ELEM_INTERNAL_TAG)) {
        ok = true;
        BM_ITER_ELEM (v, &viter, f, BM_VERTS_OF_FACE) {
          if (!BM_elem_flag_test(v, BM_ELEM_INTERNAL_TAG)) {
            ok = false;
            break;
          }
        }

        if (ok) {
          /* we only use boundary verts */
          BM_elem_flag_enable(f, BM_ELEM_INTERNAL_TAG);
          tot_tag++;
        }
      }
      else {
        /* we already found! */
      }
    }
  }

  if (tot_tag == 0) {
    /* no faces use only boundary verts, quit early */
    ok = false;
    goto finally;
  }

  /* 2) loop over non-boundary edges that use boundary verts,
   *    check each have 2 tagged faces connected (faces that only use 'varr' verts) */
  ok = true;
  for (i = 0; i < len; i++) {
    BM_ITER_ELEM (e, &fiter, varr[i], BM_EDGES_OF_VERT) {

      if (/* non-boundary edge */
          BM_elem_flag_test(e, BM_ELEM_INTERNAL_TAG) == false &&
          /* ...using boundary verts */
          BM_elem_flag_test(e->v1, BM_ELEM_INTERNAL_TAG) &&
          BM_elem_flag_test(e->v2, BM_ELEM_INTERNAL_TAG))
      {
        int tot_face_tag = 0;
        BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
          if (BM_elem_flag_test(f, BM_ELEM_INTERNAL_TAG)) {
            tot_face_tag++;
          }
        }

        if (tot_face_tag != 2) {
          ok = false;
          break;
        }
      }
    }

    if (ok == false) {
      break;
    }
  }

finally:
  /* Cleanup */
  for (i = 0; i < len; i++) {
    BM_elem_flag_disable(varr[i], BM_ELEM_INTERNAL_TAG);
    BM_elem_flag_disable(earr[i], BM_ELEM_INTERNAL_TAG);
  }
  return ok;
}

bool BM_face_exists_multi_edge(BMEdge **earr, int len)
{
  BMVert **varr = BLI_array_alloca(varr, len);

  /* first check if verts have edges, if not we can bail out early */
  if (!BM_verts_from_edges(varr, earr, len)) {
    BMESH_ASSERT(0);
    return false;
  }

  return BM_face_exists_multi(varr, earr, len);
}

BMFace *BM_face_exists_overlap(BMVert **varr, const int len)
{
  BMIter viter;
  BMFace *f;
  int i;
  BMFace *f_overlap = nullptr;
  LinkNode *f_lnk = nullptr;

#ifndef NDEBUG
  /* check flag isn't already set */
  for (i = 0; i < len; i++) {
    BM_ITER_ELEM (f, &viter, varr[i], BM_FACES_OF_VERT) {
      BLI_assert(BM_ELEM_API_FLAG_TEST(f, _FLAG_OVERLAP) == 0);
    }
  }
#endif

  for (i = 0; i < len; i++) {
    BM_ITER_ELEM (f, &viter, varr[i], BM_FACES_OF_VERT) {
      if (BM_ELEM_API_FLAG_TEST(f, _FLAG_OVERLAP) == 0) {
        if (len <= BM_verts_in_face_count(varr, len, f)) {
          f_overlap = f;
          break;
        }

        BM_ELEM_API_FLAG_ENABLE(f, _FLAG_OVERLAP);
        BLI_linklist_prepend_alloca(&f_lnk, f);
      }
    }
  }

  for (; f_lnk; f_lnk = f_lnk->next) {
    BM_ELEM_API_FLAG_DISABLE((BMFace *)f_lnk->link, _FLAG_OVERLAP);
  }

  return f_overlap;
}

bool BM_face_exists_overlap_subset(BMVert **varr, const int len)
{
  BMIter viter;
  BMFace *f;
  bool is_init = false;
  bool is_overlap = false;
  LinkNode *f_lnk = nullptr;

#ifndef NDEBUG
  /* check flag isn't already set */
  for (int i = 0; i < len; i++) {
    BLI_assert(BM_ELEM_API_FLAG_TEST(varr[i], _FLAG_OVERLAP) == 0);
    BM_ITER_ELEM (f, &viter, varr[i], BM_FACES_OF_VERT) {
      BLI_assert(BM_ELEM_API_FLAG_TEST(f, _FLAG_OVERLAP) == 0);
    }
  }
#endif

  for (int i = 0; i < len; i++) {
    BM_ITER_ELEM (f, &viter, varr[i], BM_FACES_OF_VERT) {
      if ((f->len <= len) && (BM_ELEM_API_FLAG_TEST(f, _FLAG_OVERLAP) == 0)) {
        /* Check if all verts in this face are flagged. */
        BMLoop *l_iter, *l_first;

        if (is_init == false) {
          is_init = true;
          for (int j = 0; j < len; j++) {
            BM_ELEM_API_FLAG_ENABLE(varr[j], _FLAG_OVERLAP);
          }
        }

        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        is_overlap = true;
        do {
          if (BM_ELEM_API_FLAG_TEST(l_iter->v, _FLAG_OVERLAP) == 0) {
            is_overlap = false;
            break;
          }
        } while ((l_iter = l_iter->next) != l_first);

        if (is_overlap) {
          break;
        }

        BM_ELEM_API_FLAG_ENABLE(f, _FLAG_OVERLAP);
        BLI_linklist_prepend_alloca(&f_lnk, f);
      }
    }
  }

  if (is_init == true) {
    for (int i = 0; i < len; i++) {
      BM_ELEM_API_FLAG_DISABLE(varr[i], _FLAG_OVERLAP);
    }
  }

  for (; f_lnk; f_lnk = f_lnk->next) {
    BM_ELEM_API_FLAG_DISABLE((BMFace *)f_lnk->link, _FLAG_OVERLAP);
  }

  return is_overlap;
}

bool BM_vert_is_all_edge_flag_test(const BMVert *v, const char hflag, const bool respect_hide)
{
  if (v->e) {
    BMEdge *e_other;
    BMIter eiter;

    BM_ITER_ELEM (e_other, &eiter, (BMVert *)v, BM_EDGES_OF_VERT) {
      if (!respect_hide || !BM_elem_flag_test(e_other, BM_ELEM_HIDDEN)) {
        if (!BM_elem_flag_test(e_other, hflag)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool BM_vert_is_all_face_flag_test(const BMVert *v, const char hflag, const bool respect_hide)
{
  if (v->e) {
    BMEdge *f_other;
    BMIter fiter;

    BM_ITER_ELEM (f_other, &fiter, (BMVert *)v, BM_FACES_OF_VERT) {
      if (!respect_hide || !BM_elem_flag_test(f_other, BM_ELEM_HIDDEN)) {
        if (!BM_elem_flag_test(f_other, hflag)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool BM_edge_is_all_face_flag_test(const BMEdge *e, const char hflag, const bool respect_hide)
{
  if (e->l) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      if (!respect_hide || !BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
        if (!BM_elem_flag_test(l_iter->f, hflag)) {
          return false;
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return true;
}

bool BM_edge_is_any_face_flag_test(const BMEdge *e, const char hflag)
{
  if (e->l) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      if (BM_elem_flag_test(l_iter->f, hflag)) {
        return true;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return false;
}

bool BM_edge_is_any_vert_flag_test(const BMEdge *e, const char hflag)
{
  return (BM_elem_flag_test(e->v1, hflag) || BM_elem_flag_test(e->v2, hflag));
}

bool BM_face_is_any_vert_flag_test(const BMFace *f, const char hflag)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (BM_elem_flag_test(l_iter->v, hflag)) {
      return true;
    }
  } while ((l_iter = l_iter->next) != l_first);
  return false;
}

bool BM_face_is_any_edge_flag_test(const BMFace *f, const char hflag)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (BM_elem_flag_test(l_iter->e, hflag)) {
      return true;
    }
  } while ((l_iter = l_iter->next) != l_first);
  return false;
}

bool BM_edge_is_any_face_len_test(const BMEdge *e, const int len)
{
  if (e->l) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      if (l_iter->f->len == len) {
        return true;
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return false;
}

bool BM_face_is_normal_valid(const BMFace *f)
{
  const float eps = 0.0001f;
  float no[3];

  BM_face_calc_normal(f, no);
  return len_squared_v3v3(no, f->no) < (eps * eps);
}

/**
 * Use to accumulate volume calculation for faces with consistent winding.
 *
 * Use double precision since this is prone to float precision error, see #73295.
 */
static double bm_mesh_calc_volume_face(const BMFace *f)
{
  const int tottri = f->len - 2;
  BMLoop **loops = BLI_array_alloca(loops, f->len);
  uint(*index)[3] = BLI_array_alloca(index, tottri);
  double vol = 0.0;

  BM_face_calc_tessellation(f, false, loops, index);

  for (int j = 0; j < tottri; j++) {
    const float *p1 = loops[index[j][0]]->v->co;
    const float *p2 = loops[index[j][1]]->v->co;
    const float *p3 = loops[index[j][2]]->v->co;

    double p1_db[3];
    double p2_db[3];
    double p3_db[3];

    copy_v3db_v3fl(p1_db, p1);
    copy_v3db_v3fl(p2_db, p2);
    copy_v3db_v3fl(p3_db, p3);

    /* co1.dot(co2.cross(co3)) / 6.0 */
    double cross[3];
    cross_v3_v3v3_db(cross, p2_db, p3_db);
    vol += dot_v3v3_db(p1_db, cross);
  }
  return (1.0 / 6.0) * vol;
}
double BM_mesh_calc_volume(BMesh *bm, bool is_signed)
{
  /* warning, calls its own tessellation function, may be slow */
  double vol = 0.0;
  BMFace *f;
  BMIter fiter;

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    vol += bm_mesh_calc_volume_face(f);
  }

  if (is_signed == false) {
    vol = fabs(vol);
  }

  return vol;
}

int BM_mesh_calc_face_groups(BMesh *bm,
                             int *r_groups_array,
                             int (**r_group_index)[2],
                             BMLoopFilterFunc filter_fn,
                             BMLoopPairFilterFunc filter_pair_fn,
                             void *user_data,
                             const char hflag_test,
                             const char htype_step)
{
  /* NOTE: almost duplicate of #BM_mesh_calc_edge_groups, keep in sync. */

#ifndef NDEBUG
  int group_index_len = 1;
#else
  int group_index_len = 32;
#endif

  int (*group_index)[2] = static_cast<int (*)[2]>(
      MEM_mallocN(sizeof(*group_index) * group_index_len, __func__));

  int *group_array = r_groups_array;
  STACK_DECLARE(group_array);

  int group_curr = 0;

  uint tot_faces = 0;
  uint tot_touch = 0;

  BMFace **stack;
  STACK_DECLARE(stack);

  BMIter iter;
  BMFace *f, *f_next;
  int i;

  STACK_INIT(group_array, bm->totface);

  BLI_assert(((htype_step & ~(BM_VERT | BM_EDGE)) == 0) && (htype_step != 0));

  /* init the array */
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    if ((hflag_test == 0) || BM_elem_flag_test(f, hflag_test)) {
      tot_faces++;
      BM_elem_flag_disable(f, BM_ELEM_TAG);
    }
    else {
      /* never walk over tagged */
      BM_elem_flag_enable(f, BM_ELEM_TAG);
    }

    BM_elem_index_set(f, i); /* set_inline */
  }
  bm->elem_index_dirty &= ~BM_FACE;

  /* detect groups */
  stack = MEM_malloc_arrayN<BMFace *>(tot_faces, __func__);

  f_next = static_cast<BMFace *>(BM_iter_new(&iter, bm, BM_FACES_OF_MESH, nullptr));

  while (tot_touch != tot_faces) {
    int *group_item;
    bool ok = false;

    BLI_assert(tot_touch < tot_faces);

    STACK_INIT(stack, tot_faces);

    for (; f_next; f_next = static_cast<BMFace *>(BM_iter_step(&iter))) {
      if (BM_elem_flag_test(f_next, BM_ELEM_TAG) == false) {
        BM_elem_flag_enable(f_next, BM_ELEM_TAG);
        STACK_PUSH(stack, f_next);
        ok = true;
        break;
      }
    }

    BLI_assert(ok == true);
    UNUSED_VARS_NDEBUG(ok);

    /* manage arrays */
    if (group_index_len == group_curr) {
      group_index_len *= 2;
      group_index = static_cast<int (*)[2]>(
          MEM_reallocN(group_index, sizeof(*group_index) * group_index_len));
    }

    group_item = group_index[group_curr];
    group_item[0] = STACK_SIZE(group_array);
    group_item[1] = 0;

    while ((f = STACK_POP(stack))) {
      BMLoop *l_iter, *l_first;

      /* add face */
      STACK_PUSH(group_array, BM_elem_index_get(f));
      tot_touch++;
      group_item[1]++;
      /* done */

      if (htype_step & BM_EDGE) {
        /* search for other faces */
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          BMLoop *l_radial_iter = l_iter->radial_next;
          if ((l_radial_iter != l_iter) &&
              ((filter_fn == nullptr) || filter_fn(l_iter, user_data)))
          {
            do {
              if ((filter_pair_fn == nullptr) || filter_pair_fn(l_iter, l_radial_iter, user_data))
              {
                BMFace *f_other = l_radial_iter->f;
                if (BM_elem_flag_test(f_other, BM_ELEM_TAG) == false) {
                  BM_elem_flag_enable(f_other, BM_ELEM_TAG);
                  STACK_PUSH(stack, f_other);
                }
              }
            } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);
          }
        } while ((l_iter = l_iter->next) != l_first);
      }

      if (htype_step & BM_VERT) {
        BMIter liter;
        /* search for other faces */
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          if ((filter_fn == nullptr) || filter_fn(l_iter, user_data)) {
            BMLoop *l_other;
            BM_ITER_ELEM (l_other, &liter, l_iter, BM_LOOPS_OF_LOOP) {
              if ((filter_pair_fn == nullptr) || filter_pair_fn(l_iter, l_other, user_data)) {
                BMFace *f_other = l_other->f;
                if (BM_elem_flag_test(f_other, BM_ELEM_TAG) == false) {
                  BM_elem_flag_enable(f_other, BM_ELEM_TAG);
                  STACK_PUSH(stack, f_other);
                }
              }
            }
          }
        } while ((l_iter = l_iter->next) != l_first);
      }
    }

    group_curr++;
  }

  MEM_freeN(stack);

  /* reduce alloc to required size */
  if (group_index_len != group_curr) {
    group_index = static_cast<int (*)[2]>(
        MEM_reallocN(group_index, sizeof(*group_index) * group_curr));
  }
  *r_group_index = group_index;

  return group_curr;
}

int BM_mesh_calc_edge_groups(BMesh *bm,
                             int *r_groups_array,
                             int (**r_group_index)[2],
                             BMVertFilterFunc filter_fn,
                             void *user_data,
                             const char hflag_test)
{
  /* NOTE: almost duplicate of #BM_mesh_calc_face_groups, keep in sync. */

#ifndef NDEBUG
  int group_index_len = 1;
#else
  int group_index_len = 32;
#endif

  int (*group_index)[2] = static_cast<int (*)[2]>(
      MEM_mallocN(sizeof(*group_index) * group_index_len, __func__));

  int *group_array = r_groups_array;
  STACK_DECLARE(group_array);

  int group_curr = 0;

  uint tot_edges = 0;
  uint tot_touch = 0;

  BMEdge **stack;
  STACK_DECLARE(stack);

  BMIter iter;
  BMEdge *e, *e_next;
  int i;
  STACK_INIT(group_array, bm->totedge);

  /* init the array */
  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    if ((hflag_test == 0) || BM_elem_flag_test(e, hflag_test)) {
      tot_edges++;
      BM_elem_flag_disable(e, BM_ELEM_TAG);
    }
    else {
      /* never walk over tagged */
      BM_elem_flag_enable(e, BM_ELEM_TAG);
    }

    BM_elem_index_set(e, i); /* set_inline */
  }
  bm->elem_index_dirty &= ~BM_EDGE;

  /* detect groups */
  stack = MEM_malloc_arrayN<BMEdge *>(tot_edges, __func__);

  e_next = static_cast<BMEdge *>(BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, nullptr));

  while (tot_touch != tot_edges) {
    int *group_item;
    bool ok = false;

    BLI_assert(tot_touch < tot_edges);

    STACK_INIT(stack, tot_edges);

    for (; e_next; e_next = static_cast<BMEdge *>(BM_iter_step(&iter))) {
      if (BM_elem_flag_test(e_next, BM_ELEM_TAG) == false) {
        BM_elem_flag_enable(e_next, BM_ELEM_TAG);
        STACK_PUSH(stack, e_next);
        ok = true;
        break;
      }
    }

    BLI_assert(ok == true);
    UNUSED_VARS_NDEBUG(ok);

    /* manage arrays */
    if (group_index_len == group_curr) {
      group_index_len *= 2;
      group_index = static_cast<int (*)[2]>(
          MEM_reallocN(group_index, sizeof(*group_index) * group_index_len));
    }

    group_item = group_index[group_curr];
    group_item[0] = STACK_SIZE(group_array);
    group_item[1] = 0;

    while ((e = STACK_POP(stack))) {
      BMIter viter;
      BMIter eiter;
      BMVert *v;

      /* add edge */
      STACK_PUSH(group_array, BM_elem_index_get(e));
      tot_touch++;
      group_item[1]++;
      /* done */

      /* search for other edges */
      BM_ITER_ELEM (v, &viter, e, BM_VERTS_OF_EDGE) {
        if ((filter_fn == nullptr) || filter_fn(v, user_data)) {
          BMEdge *e_other;
          BM_ITER_ELEM (e_other, &eiter, v, BM_EDGES_OF_VERT) {
            if (BM_elem_flag_test(e_other, BM_ELEM_TAG) == false) {
              BM_elem_flag_enable(e_other, BM_ELEM_TAG);
              STACK_PUSH(stack, e_other);
            }
          }
        }
      }
    }

    group_curr++;
  }

  MEM_freeN(stack);

  /* reduce alloc to required size */
  if (group_index_len != group_curr) {
    group_index = static_cast<int (*)[2]>(
        MEM_reallocN(group_index, sizeof(*group_index) * group_curr));
  }
  *r_group_index = group_index;

  return group_curr;
}

int BM_mesh_calc_edge_groups_as_arrays(
    BMesh *bm, BMVert **verts, BMEdge **edges, BMFace **faces, int (**r_groups)[3])
{
  int (*groups)[3] = MEM_malloc_arrayN<int[3]>(bm->totvert, __func__);
  STACK_DECLARE(groups);
  STACK_INIT(groups, bm->totvert);

  /* Clear all selected vertices */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMVert **stack = MEM_malloc_arrayN<BMVert *>(bm->totvert, __func__);
  STACK_DECLARE(stack);
  STACK_INIT(stack, bm->totvert);

  STACK_DECLARE(verts);
  STACK_INIT(verts, bm->totvert);

  STACK_DECLARE(edges);
  STACK_INIT(edges, bm->totedge);

  STACK_DECLARE(faces);
  STACK_INIT(faces, bm->totface);

  BMIter iter;
  BMVert *v_stack_init;
  BM_ITER_MESH (v_stack_init, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v_stack_init, BM_ELEM_TAG)) {
      continue;
    }

    const uint verts_init = STACK_SIZE(verts);
    const uint edges_init = STACK_SIZE(edges);
    const uint faces_init = STACK_SIZE(faces);

    /* Initialize stack. */
    BM_elem_flag_enable(v_stack_init, BM_ELEM_TAG);
    STACK_PUSH(verts, v_stack_init);

    if (v_stack_init->e != nullptr) {
      BMVert *v_iter = v_stack_init;
      do {
        BMEdge *e_iter, *e_first;
        e_iter = e_first = v_iter->e;
        do {
          if (!BM_elem_flag_test(e_iter, BM_ELEM_TAG)) {
            BM_elem_flag_enable(e_iter, BM_ELEM_TAG);
            STACK_PUSH(edges, e_iter);

            if (e_iter->l != nullptr) {
              BMLoop *l_iter, *l_first;
              l_iter = l_first = e_iter->l;
              do {
                if (!BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
                  BM_elem_flag_enable(l_iter->f, BM_ELEM_TAG);
                  STACK_PUSH(faces, l_iter->f);
                }
              } while ((l_iter = l_iter->radial_next) != l_first);
            }

            BMVert *v_other = BM_edge_other_vert(e_iter, v_iter);
            if (!BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
              BM_elem_flag_enable(v_other, BM_ELEM_TAG);
              STACK_PUSH(verts, v_other);

              STACK_PUSH(stack, v_other);
            }
          }
        } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v_iter)) != e_first);
      } while ((v_iter = STACK_POP(stack)));
    }

    int *g = STACK_PUSH_RET(groups);
    g[0] = STACK_SIZE(verts) - verts_init;
    g[1] = STACK_SIZE(edges) - edges_init;
    g[2] = STACK_SIZE(faces) - faces_init;
  }

  MEM_freeN(stack);

  /* Reduce alloc to required size. */
  groups = static_cast<int (*)[3]>(MEM_reallocN(groups, sizeof(*groups) * STACK_SIZE(groups)));
  *r_groups = groups;
  return STACK_SIZE(groups);
}

float bmesh_subd_falloff_calc(const int falloff, float val)
{
  switch (falloff) {
    case SUBD_FALLOFF_SMOOTH:
      val = 3.0f * val * val - 2.0f * val * val * val;
      break;
    case SUBD_FALLOFF_SPHERE:
      val = sqrtf(2.0f * val - val * val);
      break;
    case SUBD_FALLOFF_ROOT:
      val = sqrtf(val);
      break;
    case SUBD_FALLOFF_SHARP:
      val = val * val;
      break;
    case SUBD_FALLOFF_LIN:
      break;
    case SUBD_FALLOFF_INVSQUARE:
      val = val * (2.0f - val);
      break;
    default:
      BLI_assert(0);
      break;
  }

  return val;
}
