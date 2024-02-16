/* SPDX-FileCopyrightText: 2023 Blender Authors
 * SPDX-FileCopyrightText: 2001 softSurfer (http://www.softsurfer.com)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_bounds.hh"
#include "BLI_convexhull_2d.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* Keep last. */

/**
 * Assert the optimized bounds match a brute force check,
 * disable by default is this is slow for dense hulls, using `O(n^2)` complexity.
 */
// #define USE_BRUTE_FORCE_ASSERT

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Internal Math Functions
 * \{ */

static float mul_v2_v2_cw_x(const float2 &mat, const float2 &vec)
{
  return (mat[0] * vec[0]) + (mat[1] * vec[1]);
}

static float mul_v2_v2_cw_y(const float2 &mat, const float2 &vec)
{
  return (mat[1] * vec[0]) - (mat[0] * vec[1]);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Convex-Hull Calculation
 * \{ */

/* Copyright 2001, softSurfer (http://www.softsurfer.com)
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 * http://softsurfer.com/Archive/algorithm_0203/algorithm_0203.htm */

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

static int convexhull_2d_sorted(const float (*points)[2], const int points_num, int r_points[])
{
  BLI_assert(points_num >= 2); /* Doesn't handle trivial cases. */
  /* The output array `r_points[]` will be used as the stack. */
  int bot = 0;
  /* Indices for bottom and top of the stack. */
  int top = -1;
  /* Array scan index. */
  int i;

  const int minmin = 0;
  const int maxmax = points_num - 1;
  int minmax;
  int maxmin;

  float xmax;

  /* Get the indices of points with min X-coord and min|max Y-coord. */
  float xmin = points[0][0];
  for (i = 1; i <= maxmax; i++) {
    if (points[i][0] != xmin) {
      break;
    }
  }

  minmax = i - 1;
  if (minmax == maxmax) { /* Degenerate case: all x-coords == X-min. */
    r_points[++top] = minmin;
    if (points[minmax][1] != points[minmin][1]) {
      /* A nontrivial segment. */
      r_points[++top] = minmax;
    }
    BLI_assert(top + 1 <= points_num);
    return top + 1;
  }

  /* Get the indices of points with max X-coord and min|max Y-coord. */

  xmax = points[maxmax][0];
  for (i = maxmax - 1; i >= 0; i--) {
    if (points[i][0] != xmax) {
      break;
    }
  }
  maxmin = i + 1;

  /* Compute the lower hull on the stack `r_points`. */
  r_points[++top] = minmin; /* Push `minmin` point onto stack. */
  i = minmax;
  while (++i <= maxmin) {
    /* The lower line joins `points[minmin]` with `points[maxmin]`. */
    if (is_left(points[minmin], points[maxmin], points[i]) >= 0 && i < maxmin) {
      continue; /* Ignore `points[i]` above or on the lower line. */
    }

    while (top > 0) { /* There are at least 2 points on the stack. */
      /* Test if `points[i]` is left of the line at the stack top. */
      if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
        break; /* `points[i]` is a new hull vertex. */
      }
      top--; /* Pop top point off stack. */
    }

    r_points[++top] = i; /* Push `points[i]` onto stack. */
  }

  /* Next, compute the upper hull on the stack `r_points` above the bottom hull. */
  if (maxmax != maxmin) {     /* If distinct `xmax` points. */
    r_points[++top] = maxmax; /* Push `maxmax` point onto stack. */
  }

  bot = top; /* the bottom point of the upper hull stack */
  i = maxmin;
  while (--i >= minmax) {
    /* The upper line joins `points[maxmax]` with `points[minmax]`. */
    if (is_left(points[maxmax], points[minmax], points[i]) >= 0 && i > minmax) {
      continue; /* Ignore points[i] below or on the upper line. */
    }

    while (top > bot) { /* At least 2 points on the upper stack. */
      /* Test if `points[i]` is left of the line at the stack top. */
      if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
        break; /* points[i] is a new hull vertex. */
      }
      top--; /* Pop top point off stack. */
    }

    if (points[i][0] == points[r_points[0]][0] && points[i][1] == points[r_points[0]][1]) {
      BLI_assert(top + 1 <= points_num);
      return top + 1; /* Special case (mgomes). */
    }

    r_points[++top] = i; /* Push points[i] onto stack. */
  }

  if (minmax != minmin && r_points[0] != minmin) {
    r_points[++top] = minmin; /* Push joining endpoint onto stack. */
  }

  BLI_assert(top + 1 <= points_num);
  return top + 1;
}

int BLI_convexhull_2d(const float (*points)[2], const int points_num, int r_points[])
{
  BLI_assert(points_num >= 0);
  if (points_num < 2) {
    if (points_num == 1) {
      r_points[0] = 0;
    }
    return points_num;
  }
  int *points_map = static_cast<int *>(MEM_mallocN(sizeof(int) * size_t(points_num), __func__));
  float(*points_sort)[2] = static_cast<float(*)[2]>(
      MEM_mallocN(sizeof(*points_sort) * size_t(points_num), __func__));

  for (int i = 0; i < points_num; i++) {
    points_map[i] = i;
  }

  /* Sort the points by X, then by Y. */
  std::sort(points_map, points_map + points_num, [points](const int &a_index, const int &b_index) {
    const float *a = points[a_index];
    const float *b = points[b_index];
    if (a[1] > b[1]) {
      return false;
    }
    if (a[1] < b[1]) {
      return true;
    }

    if (a[0] > b[0]) {
      return false;
    }
    if (a[0] < b[0]) {
      return true;
    }
    return false;
  });

  for (int i = 0; i < points_num; i++) {
    copy_v2_v2(points_sort[i], points[points_map[i]]);
  }

  int points_hull_num = convexhull_2d_sorted(points_sort, points_num, r_points);

  /* Map back to the unsorted index values. */
  for (int i = 0; i < points_hull_num; i++) {
    r_points[i] = points_map[r_points[i]];
  }

  MEM_freeN(points_map);
  MEM_freeN(points_sort);

  BLI_assert(points_hull_num <= points_num);
  return points_hull_num;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Comupte AABB Fitting Angle (For Assertion)
 * \{ */

static float convexhull_aabb_fit_hull_2d_brute_force(const float (*points_hull)[2],
                                                     int points_hull_num)
{
  float area_best = FLT_MAX;
  float2 dvec_best = {0.0f, 1.0f}; /* Track the best angle as a unit vector, delaying `atan2`. */

  for (int i = 0, i_prev = points_hull_num - 1; i < points_hull_num; i_prev = i++) {
    /* 2D rotation matrix. */
    float dvec_length = 0.0f;
    const float2 dvec = math::normalize_and_get_length(
        float2(points_hull[i]) - float2(points_hull[i_prev]), dvec_length);
    if (UNLIKELY(dvec_length == 0.0f)) {
      continue;
    }

    blender::Bounds<float> bounds[2] = {{FLT_MAX, -FLT_MAX}, {FLT_MAX, -FLT_MAX}};
    float area_test;

    for (int j = 0; j < points_hull_num; j++) {
      const float2 tvec = {
          mul_v2_v2_cw_x(dvec, points_hull[j]),
          mul_v2_v2_cw_y(dvec, points_hull[j]),
      };

      bounds[0].min = math::min(bounds[0].min, tvec[0]);
      bounds[0].max = math::max(bounds[0].max, tvec[0]);
      bounds[1].min = math::min(bounds[1].min, tvec[1]);
      bounds[1].max = math::max(bounds[1].max, tvec[1]);

      area_test = (bounds[0].max - bounds[0].min) * (bounds[1].max - bounds[1].min);
      if (area_test > area_best) {
        break;
      }
    }

    if (area_test < area_best) {
      area_best = area_test;
      dvec_best = dvec;
    }
  }

  return (area_best != FLT_MAX) ? float(atan2(dvec_best[0], dvec_best[1])) : 0.0f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Comupte AABB Fitting Angle (Optimized)
 * \{ */

/**
 * When using the rotating calipers, step one half of the caliper to a new index.
 *
 * Note that this relies on `points_hull` being ordered CCW which #BLI_convexhull_2d ensures.
 */
template<int Axis, int AxisSign>
static float convexhull_2d_compute_extent_on_axis(const float (*points_hull)[2],
                                                  const int points_hull_num,
                                                  const float2 &dvec,
                                                  int *index_p)
{
  /* NOTE(@ideasman42): This could be optimized to use a search strategy
   * that computes the upper bounds and narrows down the result instead of
   * simply checking every point until the new maximum is reached.
   * From looking into I couldn't find cases where doing this has significant benefits,
   * especially when compared with the complexity of using more involved logic for
   * the common case, where only a few steps are needed.
   * Typically the number of points to scan is small (around [0..8]).
   * And while a high-detail hull with single outliner points will cause stepping over
   * many more points, in practice there are rarely more than a few of these in a convex-hull.
   * Nevertheless, a high-poly hull that has subtle curves containing many points as well as
   * some sharp-corners wont perform as well with this method. */

  const int index_init = *index_p;
  int index_best = index_init;
  float value_init = (Axis == 0) ? mul_v2_v2_cw_x(dvec, points_hull[index_best]) :
                                   mul_v2_v2_cw_y(dvec, points_hull[index_best]);
  float value_best = value_init;
  /* Simply scan up the array. */
  for (int count = 1; count < points_hull_num; count++) {
    const int index_test = (index_init + count) % points_hull_num;
    const float value_test = (Axis == 0) ? mul_v2_v2_cw_x(dvec, points_hull[index_test]) :
                                           mul_v2_v2_cw_y(dvec, points_hull[index_test]);
    if ((AxisSign == -1) ? (value_test > value_best) : (value_test < value_best)) {
      break;
    }
    value_best = value_test;
    index_best = index_test;
  }

  *index_p = index_best;
  return value_best;
}

static float convexhull_aabb_fit_hull_2d(const float (*points_hull)[2], int points_hull_num)
{
  float area_best = FLT_MAX;
  float2 dvec_best; /* Track the best angle as a unit vector, delaying `atan2`. */
  bool is_first = true;

  /* Initialize to zero because the first pass uses the first index to set the bounds. */
  blender::Bounds<int> bounds_index[2] = {{0, 0}, {0, 0}};

  for (int i = 0, i_prev = points_hull_num - 1; i < points_hull_num; i_prev = i++) {
    /* 2D rotation matrix. */
    float dvec_length = 0.0f;
    const float2 dvec = math::normalize_and_get_length(
        float2(points_hull[i]) - float2(points_hull[i_prev]), dvec_length);
    if (UNLIKELY(dvec_length == 0.0f)) {
      continue;
    }

    if (UNLIKELY(is_first)) {
      is_first = false;

      blender::Bounds<float> bounds[2];

      bounds[0].min = bounds[0].max = mul_v2_v2_cw_x(dvec, points_hull[0]);
      bounds[1].min = bounds[1].max = mul_v2_v2_cw_y(dvec, points_hull[0]);

      bounds_index[0].min = bounds_index[0].max = 0;
      bounds_index[1].min = bounds_index[1].max = 0;

      for (int j = 1; j < points_hull_num; j++) {
        const float2 tvec = {
            mul_v2_v2_cw_x(dvec, points_hull[j]),
            mul_v2_v2_cw_y(dvec, points_hull[j]),
        };
        for (int axis = 0; axis < 2; axis++) {
          if (tvec[axis] < bounds[axis].min) {
            bounds[axis].min = tvec[axis];
            bounds_index[axis].min = j;
          }
          if (tvec[axis] > bounds[axis].max) {
            bounds[axis].max = tvec[axis];
            bounds_index[axis].max = j;
          }
        }
      }

      area_best = (bounds[0].max - bounds[0].min) * (bounds[1].max - bounds[1].min);
      dvec_best = dvec;
      continue;
    }

    /* Step the calipers to the new rotation `dvec`, returning the bounds at the same time. */
    blender::Bounds<float> bounds_test[2] = {
        {convexhull_2d_compute_extent_on_axis<0, -1>(
             points_hull, points_hull_num, dvec, &bounds_index[0].min),
         convexhull_2d_compute_extent_on_axis<0, 1>(
             points_hull, points_hull_num, dvec, &bounds_index[0].max)},
        {convexhull_2d_compute_extent_on_axis<1, -1>(
             points_hull, points_hull_num, dvec, &bounds_index[1].min),
         convexhull_2d_compute_extent_on_axis<1, 1>(
             points_hull, points_hull_num, dvec, &bounds_index[1].max)},

    };

    const float area_test = (bounds_test[0].max - bounds_test[0].min) *
                            (bounds_test[1].max - bounds_test[1].min);

    if (area_test < area_best) {
      area_best = area_test;
      dvec_best = dvec;
    }
  }

  const float angle = (area_best != FLT_MAX) ? float(atan2(dvec_best[0], dvec_best[1])) : 0.0f;

#ifdef USE_BRUTE_FORCE_ASSERT
  /* Ensure the optimized result matches the brute-force version. */
  BLI_assert(angle == convexhull_aabb_fit_hull_2d_brute_force(points_hull, points_hull_num));
#else
  (void)convexhull_aabb_fit_hull_2d_brute_force;
#endif

  return angle;
}

float BLI_convexhull_aabb_fit_points_2d(const float (*points)[2], int points_num)
{
  BLI_assert(points_num >= 0);
  float angle = 0.0f;

  int *index_map = static_cast<int *>(
      MEM_mallocN(sizeof(*index_map) * size_t(points_num), __func__));

  int points_hull_num = BLI_convexhull_2d(points, points_num, index_map);

  if (points_hull_num > 1) {
    float(*points_hull)[2] = static_cast<float(*)[2]>(
        MEM_mallocN(sizeof(*points_hull) * size_t(points_hull_num), __func__));
    for (int j = 0; j < points_hull_num; j++) {
      copy_v2_v2(points_hull[j], points[index_map[j]]);
    }

    angle = convexhull_aabb_fit_hull_2d(points_hull, points_hull_num);
    MEM_freeN(points_hull);
  }

  MEM_freeN(index_map);

  return angle;
}

/** \} */
