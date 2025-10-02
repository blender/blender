/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_curve_types.h"

#include "BLI_heap.h"
#include "BLI_math_vector.h"
#include "MEM_guardedalloc.h"

#include "BKE_curve.hh"

extern "C" {
#include "curve_fit_nd.h"
}

#include <cstring>

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

struct Knot {
  Knot *next, *prev;
  uint point_index; /* Index in point array. */
  uint knot_index;  /* Index in knot array. */
  float tan[2][3];
  float handles[2];

  HeapNode *heap_node;
  uint can_remove : 1;
  uint is_removed : 1;

#ifndef NDEBUG
  const float *co;
#endif
};

struct Removal {
  uint knot_index;
  /* handles for prev/next knots */
  float handles[2];
};

static float knot_remove_error_value(const float tan_l[3],
                                     const float tan_r[3],
                                     const float (*points)[3],
                                     const uint points_len,
                                     /* avoid having to re-calculate again */
                                     float r_handle_factors[2])
{
  const uint dims = 3;
  float error_sq = FLT_MAX;
  uint error_sq_index;
  float handle_factors[2][3];

  curve_fit_cubic_to_points_single_fl(&points[0][0],
                                      points_len,
                                      nullptr,
                                      dims,
                                      0.0f,
                                      tan_l,
                                      tan_r,
                                      handle_factors[0],
                                      handle_factors[1],
                                      &error_sq,
                                      &error_sq_index);

  sub_v3_v3(handle_factors[0], points[0]);
  r_handle_factors[0] = dot_v3v3(tan_l, handle_factors[0]);

  sub_v3_v3(handle_factors[1], points[points_len - 1]);
  r_handle_factors[1] = dot_v3v3(tan_r, handle_factors[1]);

  return error_sq;
}

static void knot_remove_error_recalculate(
    Heap *heap, const float (*points)[3], const uint points_len, Knot *k, const float error_sq_max)
{
  BLI_assert(k->can_remove);
  float handles[2];

#ifndef NDEBUG
  BLI_assert(equals_v3v3(points[k->prev->point_index], k->prev->co));
  BLI_assert(equals_v3v3(points[k->next->point_index], k->next->co));
#endif

  const float (*points_offset)[3];
  uint points_offset_len;

  if (k->prev->point_index < k->next->point_index) {
    points_offset = &points[k->prev->point_index];
    points_offset_len = (k->next->point_index - k->prev->point_index) + 1;
  }
  else {
    points_offset = &points[k->prev->point_index];
    points_offset_len = ((k->next->point_index + points_len) - k->prev->point_index) + 1;
  }

  const float cost_sq = knot_remove_error_value(
      k->prev->tan[1], k->next->tan[0], points_offset, points_offset_len, handles);

  if (cost_sq < error_sq_max) {
    Removal *r;
    if (k->heap_node) {
      r = static_cast<Removal *>(BLI_heap_node_ptr(k->heap_node));
    }
    else {
      r = MEM_mallocN<Removal>(__func__);
      r->knot_index = k->knot_index;
    }

    copy_v2_v2(r->handles, handles);

    BLI_heap_insert_or_update(heap, &k->heap_node, cost_sq, r);
  }
  else {
    if (k->heap_node) {
      Removal *r;
      r = static_cast<Removal *>(BLI_heap_node_ptr(k->heap_node));
      BLI_heap_remove(heap, k->heap_node);

      MEM_freeN(r);

      k->heap_node = nullptr;
    }
  }
}

static void curve_decimate(const float (*points)[3],
                           const uint points_len,
                           Knot *knots,
                           const uint knots_len,
                           float error_sq_max,
                           const uint error_target_len)
{
  Heap *heap = BLI_heap_new_ex(knots_len);
  for (uint i = 0; i < knots_len; i++) {
    Knot *k = &knots[i];
    if (k->can_remove) {
      knot_remove_error_recalculate(heap, points, points_len, k, error_sq_max);
    }
  }

  uint knots_len_remaining = knots_len;

  while ((knots_len_remaining > error_target_len) && (BLI_heap_is_empty(heap) == false)) {
    Knot *k;

    {
      Removal *r = static_cast<Removal *>(BLI_heap_pop_min(heap));
      k = &knots[r->knot_index];
      k->heap_node = nullptr;
      k->prev->handles[1] = r->handles[0];
      k->next->handles[0] = r->handles[1];
      MEM_freeN(r);
    }

    Knot *k_prev = k->prev;
    Knot *k_next = k->next;

    /* remove ourselves */
    k_next->prev = k_prev;
    k_prev->next = k_next;

    k->next = nullptr;
    k->prev = nullptr;
    k->is_removed = true;

    if (k_prev->can_remove) {
      knot_remove_error_recalculate(heap, points, points_len, k_prev, error_sq_max);
    }

    if (k_next->can_remove) {
      knot_remove_error_recalculate(heap, points, points_len, k_next, error_sq_max);
    }

    knots_len_remaining -= 1;
  }

  BLI_heap_free(heap, MEM_freeN);
}

uint BKE_curve_decimate_bezt_array(BezTriple *bezt_array,
                                   const uint bezt_array_len,
                                   const uint resolu,
                                   const bool is_cyclic,
                                   const char flag_test,
                                   const char flag_set,
                                   const float error_sq_max,
                                   const uint error_target_len)
{
  const uint bezt_array_last = bezt_array_len - 1;
  const uint points_len = BKE_curve_calc_coords_axis_len(bezt_array_len, resolu, is_cyclic, true);

  float (*points)[3] = MEM_malloc_arrayN<float[3]>(points_len * (is_cyclic ? 2 : 1), __func__);

  BKE_curve_calc_coords_axis(
      bezt_array, bezt_array_len, resolu, is_cyclic, false, 0, sizeof(float[3]), &points[0][0]);
  BKE_curve_calc_coords_axis(
      bezt_array, bezt_array_len, resolu, is_cyclic, false, 1, sizeof(float[3]), &points[0][1]);
  BKE_curve_calc_coords_axis(
      bezt_array, bezt_array_len, resolu, is_cyclic, false, 2, sizeof(float[3]), &points[0][2]);

  const uint knots_len = bezt_array_len;
  Knot *knots = MEM_malloc_arrayN<Knot>(bezt_array_len, __func__);

  if (is_cyclic) {
    memcpy(points[points_len], points[0], sizeof(float[3]) * points_len);
  }

  for (uint i = 0; i < bezt_array_len; i++) {
    knots[i].heap_node = nullptr;
    knots[i].can_remove = (bezt_array[i].f2 & flag_test) != 0;
    knots[i].is_removed = false;
    knots[i].next = &knots[i + 1];
    knots[i].prev = &knots[i - 1];
    knots[i].point_index = i * resolu;
    knots[i].knot_index = i;

    sub_v3_v3v3(knots[i].tan[0], bezt_array[i].vec[0], bezt_array[i].vec[1]);
    knots[i].handles[0] = normalize_v3(knots[i].tan[0]);

    sub_v3_v3v3(knots[i].tan[1], bezt_array[i].vec[1], bezt_array[i].vec[2]);
    knots[i].handles[1] = -normalize_v3(knots[i].tan[1]);

#ifndef NDEBUG
    knots[i].co = bezt_array[i].vec[1];
    BLI_assert(equals_v3v3(knots[i].co, points[knots[i].point_index]));
#endif
  }

  if (is_cyclic) {
    knots[0].prev = &knots[bezt_array_last];
    knots[bezt_array_last].next = &knots[0];
  }
  else {
    knots[0].prev = nullptr;
    knots[bezt_array_last].next = nullptr;

    /* always keep end-points */
    knots[0].can_remove = false;
    knots[bezt_array_last].can_remove = false;
  }

  curve_decimate(points, points_len, knots, knots_len, error_sq_max, error_target_len);

  MEM_freeN(points);

  uint knots_len_decimated = knots_len;

/* Update handle type on modifications. */
#define HANDLE_UPDATE(a, b) \
  { \
    if (a == HD_VECT) { \
      a = HD_FREE; \
    } \
    else if (ELEM(a, HD_AUTO, HD_AUTO_ANIM)) { \
      a = HD_ALIGN; \
    } \
    /* opposite handle */ \
    if (ELEM(b, HD_AUTO, HD_AUTO_ANIM)) { \
      b = HD_ALIGN; \
    } \
  } \
  ((void)0)

  for (uint i = 0; i < bezt_array_len; i++) {
    if (knots[i].is_removed) {
      /* caller must remove */
      bezt_array[i].f2 |= flag_set;
      knots_len_decimated--;
    }
    else {
      bezt_array[i].f2 &= char(~flag_set);
      if (is_cyclic || i != 0) {
        uint i_prev = (i != 0) ? i - 1 : bezt_array_last;
        if (knots[i_prev].is_removed) {
          madd_v3_v3v3fl(
              bezt_array[i].vec[0], bezt_array[i].vec[1], knots[i].tan[0], knots[i].handles[0]);
          HANDLE_UPDATE(bezt_array[i].h1, bezt_array[i].h2);
        }
      }
      if (is_cyclic || i != bezt_array_last) {
        uint i_next = (i != bezt_array_last) ? i + 1 : 0;
        if (knots[i_next].is_removed) {
          madd_v3_v3v3fl(
              bezt_array[i].vec[2], bezt_array[i].vec[1], knots[i].tan[1], knots[i].handles[1]);
          HANDLE_UPDATE(bezt_array[i].h2, bezt_array[i].h1);
        }
      }
    }
  }

#undef HANDLE_UPDATE

  MEM_freeN(knots);

  return knots_len_decimated;
}
#define SELECT 1

void BKE_curve_decimate_nurb(Nurb *nu,
                             const uint resolu,
                             const float error_sq_max,
                             const uint error_target_len)
{
  const char flag_test = BEZT_FLAG_TEMP_TAG;

  const uint pntsu_dst = BKE_curve_decimate_bezt_array(nu->bezt,
                                                       uint(nu->pntsu),
                                                       resolu,
                                                       (nu->flagu & CU_NURB_CYCLIC) != 0,
                                                       SELECT,
                                                       flag_test,
                                                       error_sq_max,
                                                       error_target_len);

  if (pntsu_dst == uint(nu->pntsu)) {
    return;
  }

  BezTriple *bezt_src = nu->bezt;
  BezTriple *bezt_dst = MEM_malloc_arrayN<BezTriple>(pntsu_dst, __func__);

  int i_src = 0, i_dst = 0;

  while (i_src < nu->pntsu) {
    if ((bezt_src[i_src].f2 & flag_test) == 0) {
      bezt_dst[i_dst] = bezt_src[i_src];
      i_dst++;
    }
    i_src++;
  }

  MEM_freeN(bezt_src);

  nu->bezt = bezt_dst;
  nu->pntsu = i_dst;
}
