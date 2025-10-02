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

#include "BLI_bounds_types.hh"
#include "BLI_convexhull_2d.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/**
 * The algorithm used to create the convex hull is susceptible to errors when
 * the shape contains *nearly* overlapping vertices on polygons using large values.
 * Unlike most floating point precision errors this happens when the numbers are not *that* big.
 * Errors may occur in the range of 100-500 for polygons that are (near) degenerate.
 * See the test: `convexhull_2d.OctagonNearDuplicates` which fails without this define.
 *
 * To ensure the resulting hull is convex, perform additional convex cross-product checks
 * when adding indices to the hull as well as running a final check on the start & end points.
 * In the common case these checks aren't needed.
 *
 * Note that it's *especially* important for the cross-product of each "ear" to be convex
 * for AABB fitting since the method used relies on each edge "turning" in the same direction.
 * When stepping over the hull: points that turn in a non-convex direction may break
 * angle-stepping, causing the final rotation angle calculation to be incorrect, see: #143390.
 */
#define USE_CONVEX_CROSS_PRODUCT_ENSURE

/**
 * Assert the optimized bounds match a brute force check,
 * disable by default is this is slow for dense hulls, using `O(n^2)` complexity.
 */
// #define USE_BRUTE_FORCE_ASSERT

/**
 * Assert that the angles the iterator is looping over are in order.
 *
 * \note This may fail when #USE_CONVEX_CROSS_PRODUCT_ENSURE isn't defined.
 */
#define USE_ANGLE_ITER_ORDER_ASSERT

using blender::float2;

/* -------------------------------------------------------------------- */
/** \name Internal Math Functions
 * \{ */

static float sincos_rotate_cw_x(const float2 &sincos, const float2 &p)
{
  return (sincos[0] * p[0]) + (sincos[1] * p[1]);
}

static float sincos_rotate_cw_y(const float2 &sincos, const float2 &p)
{
  return (sincos[1] * p[0]) - (sincos[0] * p[1]);
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
 * http://softsurfer.com/Archive/algorithm_0203/algorithm_0203.htm
 *
 * NOTE(@ideasman42): additional checks added to ensure the resulting hull is convex,
 * see the: #USE_CONVEX_CROSS_PRODUCT_ENSURE define.
 */

/**
 * Tests if a point is [left / on / right] of an infinite line.
 *
 * \param p0: Point (start of the line).
 * \param p1: Point (end of the line).
 * \param p2: Point (point to compare).
 * \return
 * - `value > 0.0` for `p2` left of the line through `p0` and `p1`.
 * - `value = 0.0` for `p2` on the line.
 * - `value < 0.0` for `p2` right of the line.
 *
 * \note When using to check the hull is convex: arguments 1 & 2 are set to the larger span
 * across the hull (lowest-to-highest index) with the mid-point being the last argument.
 * This is a convention that should be followed to prevent mismatching results
 * depending on argument order.
 */
static float is_left(const float2 &p0, const float2 &p1, const float2 &p2)
{
  return (((p1[0] - p0[0]) * (p2[1] - p0[1])) - ((p2[0] - p0[0]) * (p1[1] - p0[1])));
}

/**
 * Final pass on `r_points` & `r_points_range`,
 */
static void convexhull_2d_stack_finalize(const float2 *points,
                                         const int r_points[],
                                         blender::int2 &r_points_range)
{
#ifdef USE_CONVEX_CROSS_PRODUCT_ENSURE
  int bot = r_points_range[0];
  int top = r_points_range[1];
  while (top - bot >= 2) {
    /* While the order of checking the beginning/end doesn't matter,
     * prefer dropping values off the end of `r_points`
     * since it doesn't require re-ordering. */

    /* See #is_left note on argument order. */
    if (UNLIKELY(is_left(
                     /* Second last point. */
                     points[r_points[top - 1]],
                     /* First point. */
                     points[r_points[bot]],
                     /* Last point (candidate "ear" to remove). */
                     points[r_points[top]]) >= 0.0f))
    {
      /* Concave: drop from the end: `r_points[top]`. */
      top--;
      continue;
    }
    /* See #is_left note on argument order. */
    if (UNLIKELY(is_left(
                     /* Last point. */
                     points[r_points[top]],
                     /* Second point. */
                     points[r_points[bot + 1]],
                     /* First point (candidate "ear" to remove). */
                     points[r_points[bot]]) >= 0.0f))
    {
      /* Concave: drop from the front: `r_points[bot]`. */
      bot++;
      continue;
    }
    break;
  }
  r_points_range[0] = bot;
  r_points_range[1] = top;
#else
  UNUSED_VARS(points, r_points, r_points_range);
#endif /* !USE_CONVEX_CROSS_PRODUCT_ENSURE */
}

/**
 * This practically always results in `r_points[++top] = index`.
 * In rare cases extra checks are needed.
 */
static inline void convexhull_2d_stack_push(const float2 *points,
                                            int r_points[],
                                            int &top,
                                            int index)
{
#ifdef USE_CONVEX_CROSS_PRODUCT_ENSURE
  while (
      UNLIKELY((top >= 2) &&
               /* See #is_left note on argument order. */
               (is_left(points[r_points[top - 1]], points[index], points[r_points[top]]) >= 0.0f)))
  {
    top--;
  }
#else
  UNUSED_VARS(points);
#endif /* !USE_CONVEX_CROSS_PRODUCT_ENSURE */

  r_points[++top] = index;
}

/**
 * \return the number of points in `r_points` minus 1.
 */
static int convexhull_2d_sorted_impl(const float2 *points, const int points_num, int r_points[])
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
    convexhull_2d_stack_push(points, r_points, top, minmin);
    if (points[minmax][1] != points[minmin][1]) {
      /* A nontrivial segment. */
      convexhull_2d_stack_push(points, r_points, top, minmax);
    }
    BLI_assert(top + 1 <= points_num);
    return top;
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
  BLI_assert(top < 2);
  convexhull_2d_stack_push(points, r_points, top, minmin);

  i = minmax;
  while (++i <= maxmin) {
    /* The lower line joins `points[minmin]` with `points[maxmin]`. */
    if ((i < maxmin) && (is_left(points[minmin], points[maxmin], points[i]) >= 0.0f)) {
      continue; /* Ignore `points[i]` above or on the lower line. */
    }

    while (top > 0) { /* There are at least 2 points on the stack. */
      /* Test if `points[i]` is left of the line at the stack top. */
      if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
        break; /* `points[i]` is a new hull vertex. */
      }
      top--; /* Pop top point off stack. */
    }
    convexhull_2d_stack_push(points, r_points, top, i);
  }

  /* Next, compute the upper hull on the stack `r_points` above the bottom hull. */
  if (maxmax != maxmin) { /* If distinct `xmax` points. */
    convexhull_2d_stack_push(points, r_points, top, maxmax);
  }

  bot = top; /* The bottom point of the upper hull stack. */
  i = maxmin;
  while (--i >= minmax) {
    /* The upper line joins `points[maxmax]` with `points[minmax]`. */
    if ((i > minmax) && (is_left(points[maxmax], points[minmax], points[i]) >= 0.0f)) {
      continue; /* Ignore points[i] below or on the upper line. */
    }

    while (top > bot) { /* At least 2 points on the upper stack. */
      /* Test if `points[i]` is left of the line at the stack top. */
      if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
        break; /* `points[i]` is a new hull vertex. */
      }
      top--; /* Pop top point off stack. */
    }

    if (points[i][0] == points[r_points[0]][0] && points[i][1] == points[r_points[0]][1]) {
      BLI_assert(top + 1 <= points_num);
      return top; /* Special case (mgomes). */
    }
    convexhull_2d_stack_push(points, r_points, top, i);
  }

  if (minmax != minmin && r_points[0] != minmin) {
    /* Push joining endpoint onto stack. */
    convexhull_2d_stack_push(points, r_points, top, minmin);
  }

  BLI_assert(top + 1 <= points_num);
  return top;
}

/**
 * The range of `r_points` to use (inclusive).
 *
 * \note A range is used since points may be removed from the beginning of `r_points`,
 * this avoids removing elements from the beginning of the array in favor of skipping them
 * to keep the order of points predictable.
 */
static blender::int2 convexhull_2d_sorted(const float2 *points,
                                          const int points_num,
                                          int r_points[])
{
  const int top = convexhull_2d_sorted_impl(points, points_num, r_points);
  blender::int2 points_range = {0, top};
  convexhull_2d_stack_finalize(points, r_points, points_range);
  return points_range;
}

int BLI_convexhull_2d(blender::Span<float2> points, int r_points[])
{
  const int points_num = int(points.size());
  BLI_assert(points_num >= 0);
  if (points_num < 2) {
    if (points_num == 1) {
      r_points[0] = 0;
    }
    return points_num;
  }
  int *points_map = MEM_malloc_arrayN<int>(size_t(points_num), __func__);
  float2 *points_sort = MEM_malloc_arrayN<float2>(size_t(points_num), __func__);

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

  const blender::int2 points_hull_range = convexhull_2d_sorted(points_sort, points_num, r_points);

  /* Map back to the unsorted index values. */
  for (int i = points_hull_range[0]; i <= points_hull_range[1]; i++) {
    r_points[i] = points_map[r_points[i]];
  }

  MEM_freeN(points_map);
  MEM_freeN(points_sort);

  const int points_hull_num = (points_hull_range[1] - points_hull_range[0]) + 1;
  BLI_assert(points_hull_num <= points_num);
  return points_hull_num;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute AABB Fitting Angle (For Assertion)
 * \{ */

#if defined(USE_BRUTE_FORCE_ASSERT) && !defined(NDEBUG)
static float2 convexhull_aabb_fit_hull_2d_brute_force(const float (*points_hull)[2],
                                                      int points_hull_num)
{
  float area_best = std::numeric_limits<float>::max();
  float2 sincos_best = {0.0f, 1.0f}; /* Track the best angle as a unit vector, delaying `atan2`. */

  for (int i = 0; i < points_hull_num; i++) {
    const int i_next = (i + 1) % points_hull_num;
    /* 2D rotation matrix. */
    float dvec_length = 0.0f;
    const float2 sincos = blender::math::normalize_and_get_length(
        float2(points_hull[i_next]) - float2(points_hull[i]), dvec_length);
    if (UNLIKELY(dvec_length == 0.0f)) {
      continue;
    }

    blender::Bounds<float> bounds[2] = {
        {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()},
        {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()},
    };
    float area_test;

    for (int j = 0; j < points_hull_num; j++) {
      const float2 tvec = {
          sincos_rotate_cw_x(sincos, points_hull[j]),
          sincos_rotate_cw_y(sincos, points_hull[j]),
      };

      bounds[0].min = blender::math::min(bounds[0].min, tvec[0]);
      bounds[0].max = blender::math::max(bounds[0].max, tvec[0]);
      bounds[1].min = blender::math::min(bounds[1].min, tvec[1]);
      bounds[1].max = blender::math::max(bounds[1].max, tvec[1]);

      area_test = (bounds[0].max - bounds[0].min) * (bounds[1].max - bounds[1].min);
      if (area_test > area_best) {
        break;
      }
    }

    if (area_test < area_best) {
      area_best = area_test;
      sincos_best = sincos;
    }
  }

  return sincos_best;
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hull Angle Iteration
 *
 * Step over all angles defined by a convex hull in order from 0-90 degrees,
 * when angles are converted into their canonical form (see #sincos_canonical).
 * \{ */

/**
 * Return a canonical version of `sincos` for the purpose of bounding box fitting,
 * this maps any `sincos` to 0-1 range for both `sin` & `cos`.
 */
static float2 sincos_canonical(const float2 &sincos)
{

  /* Normalize doesn't ensure a `sin` / `cos` of 1.0/-1.0 ensures the other value is zero.
   * Without the check for both 0.0 and 1.0, iteration may not be ordered. */
  float2 result;
  if (sincos[0] < 0.0f) {
    if (sincos[1] < 0.0f) {
      result[0] = -sincos[0];
      result[1] = -sincos[1];
    }
    else if ((sincos[0] == -1.0f) && (sincos[1] == 0.0f)) {
      result[0] = -sincos[0];
      result[1] = sincos[1];
    }
    else {
      result[0] = sincos[1];
      result[1] = -sincos[0];
    }
  }
  else {
    if (sincos[1] < 0.0f) {
      result[0] = -sincos[1];
      result[1] = sincos[0];
    }
    else if ((sincos[0] == 0.0f) && (sincos[1] == 1.0f)) {
      result[0] = sincos[1];
      result[1] = sincos[0];
    }
    else {
      result = sincos;
    }
  }

  /* The range is [1.0, 0.0], it will approach but never return [0.0, 1.0],
   * as the canonical version of this value gets flipped to [1.0, 0.0]. */
  BLI_assert(result[0] > 0.0f);
  BLI_assert(result[1] >= 0.0f);
  return result;
}

/**
 * An angle calculated from an edge in a convex hull.
 */
struct AngleCanonical {
  /** The edges normalized vector. */
  float2 sincos;
  /** The result of `sincos_canonical(sincos)` */
  float2 sincos_canonical;
  /** The index value for the edge `sincos` was calculated from, used as a tie breaker. */
  int index;
};

static int hull_angle_canonical_cmp(const AngleCanonical &a, const AngleCanonical &b)
{
  if (a.sincos_canonical[0] < b.sincos_canonical[0]) {
    return -1;
  }
  if (a.sincos_canonical[0] > b.sincos_canonical[0]) {
    return 1;
  }
  /* Flipped intentionally. */
  if (a.sincos_canonical[1] > b.sincos_canonical[1]) {
    return -1;
  }
  if (a.sincos_canonical[1] < b.sincos_canonical[1]) {
    return 1;
  }

  /* Flipped intentionally. */
  if (a.index > b.index) {
    return -1;
  }
  if (a.index < b.index) {
    return 1;
  }
  return 0;
}

/**
 * This represents an angle at index `index`.
 */
struct HullAngleStep {
  /** Single linked list. */
  HullAngleStep *next;

  /** The current angle value. */
  AngleCanonical angle;

  /** The next index value to step into. */
  int index;
  /** Do not seek past this index. */
  int index_max;
};

/**
 * Iterate over all angles of a convex hull (defined by `points_hull`) in-order.
 */
struct HullAngleIter {
  /** Linked list of up to 4 items (kept in order), * to support walking over angles in order. */
  HullAngleStep *axis_ordered = nullptr;
  /** [X/Y][min/max]. */
  HullAngleStep axis[2][2];
  /** The convex hull being iterated over. */
  const float (*points_hull)[2];
  int points_hull_num;
};

static void hull_angle_insert_ordered(HullAngleIter &hiter, HullAngleStep *insert)
{
  HullAngleStep **prev_p = &hiter.axis_ordered;
  HullAngleStep *iter = hiter.axis_ordered;
  while (iter && hull_angle_canonical_cmp(iter->angle, insert->angle) > 0) {
    prev_p = &iter->next;
    iter = iter->next;
  }
  *prev_p = insert;
  insert->next = iter;
}

static bool convexhull_2d_angle_iter_step_on_axis(const HullAngleIter &hiter, HullAngleStep &hstep)
{
  BLI_assert(hstep.index != -1);
  while (hstep.index != hstep.index_max) {
    const int i_curr = hstep.index;
    const int i_next = (hstep.index + 1) % hiter.points_hull_num;
    const float2 dir = float2(hiter.points_hull[i_next]) - float2(hiter.points_hull[i_curr]);
    float dir_length = 0.0f;
    const float2 sincos_test = blender::math::normalize_and_get_length(dir, dir_length);
    hstep.index = i_next;
    if (LIKELY(dir_length != 0.0f)) {
      hstep.angle.sincos = sincos_test;
      hstep.angle.sincos_canonical = sincos_canonical(sincos_test);
      hstep.angle.index = i_curr;
      return true;
    }
  }

  /* Reached the end, signal this axis shouldn't be stepped over. */
  hstep.index = -1;
  return false;
}

static HullAngleIter convexhull_2d_angle_iter_init(const float (*points_hull)[2],
                                                   const int points_hull_num)
{
  const int points_hull_num_minus_1 = points_hull_num - 1;
  HullAngleIter hiter = {};
  /* Aligned with `hiter.axis`. */
  float range[2][2];
  /* Initialize min-max range from the first point. */
  for (int axis = 0; axis < 2; axis++) {
    range[axis][0] = points_hull[0][axis];
    range[axis][1] = points_hull[0][axis];
  }
  /* Expand from all other points.
   *
   * NOTE: Don't attempt to pick either side when there are multiple equal points.
   * Walking backwards while checking `sincos_canonical` handles that. */
  for (int i = 1; i < points_hull_num; i++) {
    const float *p = points_hull[i];
    for (int axis = 0; axis < 2; axis++) {
      if (range[axis][0] < p[axis]) {
        range[axis][0] = p[axis];
        hiter.axis[axis][0].index = i;
      }
      if (range[axis][1] > p[axis]) {
        range[axis][1] = p[axis];
        hiter.axis[axis][1].index = i;
      }
    }
  }

  /* Step backwards, compute the actual `sincos_canonical` because it's possible
   * an edge which is not *exactly* axis aligned normalizes to a value which is.
   * Instead of attempting to guess when this might happen,
   * simply calculate the value and walk backwards for a long as the canonical angle
   * has a `sin` of 1.0 (which must always come first). */
  for (int axis = 0; axis < 2; axis++) {
    for (int i = 0; i < 2; i++) {
      const int i_orig = hiter.axis[axis][i].index;
      int i_curr = i_orig, i_prev;
      /* Prevent an eternal loop (incredibly unlikely).
       * In virtually all cases this will step back once
       * (in the case of an axis-aligned edge) or not at all. */
      while ((i_prev = (i_curr + points_hull_num_minus_1) % points_hull_num) != i_orig) {
        float dir_length = 0.0f;
        const float2 sincos_test = blender::math::normalize_and_get_length(
            float2(points_hull[i_curr]) - float2(points_hull[i_prev]), dir_length);
        if (LIKELY(dir_length != 0.0f)) {
          /* Account for 90 degree corners that may also have an axis-aligned canonical angle. */
          if (blender::math::abs(sincos_test[axis]) > 0.5f) {
            break;
          }
          const float2 sincos_test_canonical = sincos_canonical(sincos_test);
          if (LIKELY(sincos_test_canonical[0] != 1.0f)) {
            break;
          }
        }
        i_curr = i_prev;
        hiter.axis[axis][i].index = i_curr;
      }
    }
  }

  /* Setup counter-clockwise limits. */
  hiter.axis[0][0].index_max = hiter.axis[1][0].index; /* West to south. */
  hiter.axis[1][0].index_max = hiter.axis[0][1].index; /* South to east. */
  hiter.axis[0][1].index_max = hiter.axis[1][1].index; /* East to north. */
  hiter.axis[1][1].index_max = hiter.axis[0][0].index; /* North to west. */

  hiter.points_hull = points_hull;
  hiter.points_hull_num = points_hull_num;

  for (int axis = 0; axis < 2; axis++) {
    for (int i = 0; i < 2; i++) {
      hiter.axis[axis][i].angle.index = hiter.axis[axis][i].index;
      if (convexhull_2d_angle_iter_step_on_axis(hiter, hiter.axis[axis][i])) {
        hull_angle_insert_ordered(hiter, &hiter.axis[axis][i]);
      }
    }
  }

  return hiter;
}

static void convexhull_2d_angle_iter_step(HullAngleIter &hiter)
{
  HullAngleStep *hstep = hiter.axis_ordered;
#ifdef USE_ANGLE_ITER_ORDER_ASSERT
  const AngleCanonical angle_prev = hstep->angle;
#endif

  hiter.axis_ordered = hiter.axis_ordered->next;
  if (convexhull_2d_angle_iter_step_on_axis(hiter, *hstep)) {
    hull_angle_insert_ordered(hiter, hstep);
  }

#ifdef USE_ANGLE_ITER_ORDER_ASSERT
  if (hiter.axis_ordered) {
    hstep = hiter.axis_ordered;
    BLI_assert(hull_angle_canonical_cmp(angle_prev, hiter.axis_ordered->angle) > 0);
    UNUSED_VARS_NDEBUG(angle_prev);
  }
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute AABB Fitting Angle (Optimized)
 * \{ */

/**
 * When using the rotating calipers, step one half of the caliper to a new index.
 *
 * Note that this relies on `points_hull` being ordered CCW which #BLI_convexhull_2d ensures.
 */
template<int Axis, int AxisSign>
static float convexhull_2d_compute_extent_on_axis(const float (*points_hull)[2],
                                                  const int points_hull_num,
                                                  const float2 &sincos,
                                                  int *index_p)
{
  /* NOTE(@ideasman42): Use a forward search instead of attempting a search strategy
   * computing upper & lower bounds (similar to a binary search). The rotating calipers
   * are ensured to test ordered rotations between 0-90 degrees, meaning any cases where
   * this function needs to step over many points will be limited to a small number of cases.
   * Since scanning forward isn't expensive it shouldn't pose a problem. */
  BLI_assert(*index_p >= 0);
  const int index_init = *index_p;
  int index_best = index_init;
  float value_init = (Axis == 0) ? sincos_rotate_cw_x(sincos, points_hull[index_best]) :
                                   sincos_rotate_cw_y(sincos, points_hull[index_best]);
  float value_best = value_init;
  /* Simply scan up the array. */
  for (int count = 1; count < points_hull_num; count++) {
    const int index_test = (index_init + count) % points_hull_num;
    const float value_test = (Axis == 0) ? sincos_rotate_cw_x(sincos, points_hull[index_test]) :
                                           sincos_rotate_cw_y(sincos, points_hull[index_test]);
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
  float area_best = std::numeric_limits<float>::max();
  float2 sincos_best = {0.0f, 1.0f}; /* Track the best angle as a unit vector, delaying `atan2`. */
  int index_best = std::numeric_limits<int>::max();

  /* Initialize to zero because the first pass uses the first index to set the bounds. */
  blender::Bounds<int> bounds_index[2] = {{0, 0}, {0, 0}};

  HullAngleIter hull_iter = convexhull_2d_angle_iter_init(points_hull, points_hull_num);

  /* Use the axis aligned bounds as starting points. */
  bounds_index[0].min = hull_iter.axis[0][1].angle.index;
  bounds_index[0].max = hull_iter.axis[0][0].angle.index;
  bounds_index[1].min = hull_iter.axis[1][0].angle.index;
  bounds_index[1].max = hull_iter.axis[1][1].angle.index;
  while (const HullAngleStep *hstep = hull_iter.axis_ordered) {
    /* Step the calipers to the new rotation `sincos`, returning the bounds at the same time. */
    blender::Bounds<float> bounds_test[2] = {
        {convexhull_2d_compute_extent_on_axis<0, -1>(
             points_hull, points_hull_num, hstep->angle.sincos_canonical, &bounds_index[0].min),
         convexhull_2d_compute_extent_on_axis<0, 1>(
             points_hull, points_hull_num, hstep->angle.sincos_canonical, &bounds_index[0].max)},
        {convexhull_2d_compute_extent_on_axis<1, -1>(
             points_hull, points_hull_num, hstep->angle.sincos_canonical, &bounds_index[1].min),
         convexhull_2d_compute_extent_on_axis<1, 1>(
             points_hull, points_hull_num, hstep->angle.sincos_canonical, &bounds_index[1].max)},
    };

    const float area_test = (bounds_test[0].max - bounds_test[0].min) *
                            (bounds_test[1].max - bounds_test[1].min);

    if (area_test < area_best ||
        /* Use the index as a tie breaker, this simply matches the behavior of checking
         * all edges in-order and only overwriting past results when they're an improvement. */
        ((area_test == area_best) && (hstep->angle.index < index_best)))
    {
      area_best = area_test;
      sincos_best = hstep->angle.sincos;
      index_best = hstep->angle.index;
    }

    convexhull_2d_angle_iter_step(hull_iter);
  }

  const float angle = (area_best != std::numeric_limits<float>::max()) ?
                          atan2(sincos_best[0], sincos_best[1]) :
                          0.0f;

#if defined(USE_BRUTE_FORCE_ASSERT) && !defined(NDEBUG)
  {
    /* Ensure the optimized result matches the brute-force version. */
    const float2 sincos_test = convexhull_aabb_fit_hull_2d_brute_force(points_hull,
                                                                       points_hull_num);
    if (sincos_best != sincos_test) {
      BLI_assert(sincos_best == sincos_test);
    }
  }
#endif

  return angle;
}

float BLI_convexhull_aabb_fit_points_2d(blender::Span<float2> points)
{
  const int points_num = int(points.size());
  BLI_assert(points_num >= 0);
  float angle = 0.0f;

  int *index_map = MEM_malloc_arrayN<int>(size_t(points_num), __func__);

  int points_hull_num = BLI_convexhull_2d(points, index_map);

  if (points_hull_num > 1) {
    float (*points_hull)[2] = MEM_malloc_arrayN<float[2]>(size_t(points_hull_num), __func__);
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
