/* SPDX-FileCopyrightText: 2023 Blender Authors
 * SPDX-FileCopyrightText: 2001 softSurfer (http://www.softsurfer.com)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_convexhull_2d.h"
#include "BLI_math_vector.h"
#include "BLI_strict_flags.h"
#include "BLI_utildefines.h"

/* Copyright 2001, softSurfer (http://www.softsurfer.com)
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 * http://softsurfer.com/Archive/algorithm_0203/algorithm_0203.htm
 */

/* -------------------------------------------------------------------- */
/** \name Main Convex-Hull Calculation
 * \{ */

/**
 * tests if a point is Left|On|Right of an infinite line.
 *    Input:  three points P0, P1, and P2
 * \returns > 0.0 for P2 left of the line through P0 and P1.
 *          = 0.0 for P2 on the line.
 *          < 0.0 for P2 right of the line.
 */
static float is_left(const float p0[2], const float p1[2], const float p2[2])
{
  return (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (p1[1] - p0[1]);
}

static int BLI_convexhull_2d_sorted(const float (*points)[2], const int n, int r_points[])
{
  BLI_assert(n >= 2); /* Doesn't handle trivial cases. */
  /* the output array r_points[] will be used as the stack */
  int bot = 0;
  int top = -1; /* indices for bottom and top of the stack */
  int i;        /* array scan index */
  int minmin, minmax;
  int maxmin, maxmax;
  float xmax;

  /* Get the indices of points with min x-coord and min|max y-coord */
  float xmin = points[0][0];
  for (i = 1; i < n; i++) {
    if (points[i][0] != xmin) {
      break;
    }
  }

  minmin = 0;
  minmax = i - 1;
  if (minmax == n - 1) { /* degenerate case: all x-coords == xmin */
    r_points[++top] = minmin;
    if (points[minmax][1] != points[minmin][1]) {
      /* a nontrivial segment */
      r_points[++top] = minmax;
    }
    BLI_assert(top + 1 <= n);
    return top + 1;
  }

  /* Get the indices of points with max x-coord and min|max y-coord */

  maxmax = n - 1;
  xmax = points[n - 1][0];
  for (i = n - 2; i >= 0; i--) {
    if (points[i][0] != xmax) {
      break;
    }
  }
  maxmin = i + 1;

  /* Compute the lower hull on the stack r_points */
  r_points[++top] = minmin; /* push minmin point onto stack */
  i = minmax;
  while (++i <= maxmin) {
    /* the lower line joins points[minmin] with points[maxmin] */
    if (is_left(points[minmin], points[maxmin], points[i]) >= 0 && i < maxmin) {
      continue; /* ignore points[i] above or on the lower line */
    }

    while (top > 0) { /* there are at least 2 points on the stack */
      /* test if points[i] is left of the line at the stack top */
      if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
        break; /* points[i] is a new hull vertex */
      }
      top--; /* pop top point off stack */
    }

    r_points[++top] = i; /* push points[i] onto stack */
  }

  /* Next, compute the upper hull on the stack r_points above the bottom hull */
  if (maxmax != maxmin) {     /* if distinct xmax points */
    r_points[++top] = maxmax; /* push maxmax point onto stack */
  }

  bot = top; /* the bottom point of the upper hull stack */
  i = maxmin;
  while (--i >= minmax) {
    /* the upper line joins points[maxmax] with points[minmax] */
    if (is_left(points[maxmax], points[minmax], points[i]) >= 0 && i > minmax) {
      continue; /* ignore points[i] below or on the upper line */
    }

    while (top > bot) { /* at least 2 points on the upper stack */
      /* test if points[i] is left of the line at the stack top */
      if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
        break; /* points[i] is a new hull vertex */
      }
      top--; /* pop top point off stack */
    }

    if (points[i][0] == points[r_points[0]][0] && points[i][1] == points[r_points[0]][1]) {
      BLI_assert(top + 1 <= n);
      return top + 1; /* special case (mgomes) */
    }

    r_points[++top] = i; /* push points[i] onto stack */
  }

  if (minmax != minmin && r_points[0] != minmin) {
    r_points[++top] = minmin; /* push joining endpoint onto stack */
  }

  BLI_assert(top + 1 <= n);
  return top + 1;
}

struct PointRef {
  const float *pt; /* 2d vector */
};

static int pointref_cmp_yx(const void *a_, const void *b_)
{
  const struct PointRef *a = a_;
  const struct PointRef *b = b_;

  if (a->pt[1] > b->pt[1]) {
    return 1;
  }
  if (a->pt[1] < b->pt[1]) {
    return -1;
  }

  if (a->pt[0] > b->pt[0]) {
    return 1;
  }
  if (a->pt[0] < b->pt[0]) {
    return -1;
  }
  return 0;
}

int BLI_convexhull_2d(const float (*points)[2], const int n, int r_points[])
{
  BLI_assert(n >= 0);
  if (n < 2) {
    if (n == 1) {
      r_points[0] = 0;
    }
    return n;
  }
  struct PointRef *points_ref = MEM_mallocN(sizeof(*points_ref) * (size_t)n, __func__);
  float(*points_sort)[2] = MEM_mallocN(sizeof(*points_sort) * (size_t)n, __func__);

  for (int i = 0; i < n; i++) {
    points_ref[i].pt = points[i];
  }

  /* Sort the points by X, then by Y. */
  qsort(points_ref, (size_t)n, sizeof(struct PointRef), pointref_cmp_yx);

  for (int i = 0; i < n; i++) {
    memcpy(points_sort[i], points_ref[i].pt, sizeof(float[2]));
  }

  int points_hull_num = BLI_convexhull_2d_sorted(points_sort, n, r_points);

  /* Map back to the unsorted index values. */
  for (int i = 0; i < points_hull_num; i++) {
    r_points[i] = (int)((const float(*)[2])points_ref[r_points[i]].pt - points);
  }

  MEM_freeN(points_ref);
  MEM_freeN(points_sort);

  BLI_assert(points_hull_num <= n);
  return points_hull_num;
}

/** \} */

/* Helper functions */

/* -------------------------------------------------------------------- */
/** \name Utility Convex-Hull Functions
 * \{ */

static float BLI_convexhull_aabb_fit_hull_2d(const float (*points_hull)[2], int n)
{
  float area_best = FLT_MAX;
  float dvec_best[2]; /* best angle, delay atan2 */

  int i_prev = n - 1;
  for (int i = 0; i < n; i++) {
    const float *ev_a = points_hull[i];
    const float *ev_b = points_hull[i_prev];
    float dvec[2]; /* 2d rotation matrix */

    sub_v2_v2v2(dvec, ev_a, ev_b);
    if (normalize_v2(dvec) != 0.0f) {
      /* rotation matrix */
      float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
      float area;

      for (int j = 0; j < n; j++) {
        float tvec[2];
        mul_v2_v2_cw(tvec, dvec, points_hull[j]);

        min[0] = min_ff(min[0], tvec[0]);
        min[1] = min_ff(min[1], tvec[1]);

        max[0] = max_ff(max[0], tvec[0]);
        max[1] = max_ff(max[1], tvec[1]);

        area = (max[0] - min[0]) * (max[1] - min[1]);
        if (area > area_best) {
          break;
        }
      }

      if (area < area_best) {
        area_best = area;
        copy_v2_v2(dvec_best, dvec);
      }
    }

    i_prev = i;
  }

  return (area_best != FLT_MAX) ? (float)atan2(dvec_best[0], dvec_best[1]) : 0.0f;
}

float BLI_convexhull_aabb_fit_points_2d(const float (*points)[2], int n)
{
  BLI_assert(n >= 0);
  float angle = 0.0f;

  int *index_map = MEM_mallocN(sizeof(*index_map) * (size_t)n, __func__);

  int points_hull_num = BLI_convexhull_2d(points, n, index_map);

  if (points_hull_num > 1) {
    float(*points_hull)[2] = MEM_mallocN(sizeof(*points_hull) * (size_t)points_hull_num, __func__);
    for (int j = 0; j < points_hull_num; j++) {
      copy_v2_v2(points_hull[j], points[index_map[j]]);
    }

    angle = BLI_convexhull_aabb_fit_hull_2d(points_hull, points_hull_num);
    MEM_freeN(points_hull);
  }

  MEM_freeN(index_map);

  return angle;
}

/** \} */
