/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Utility defines for timing/benchmarks.
 */

#pragma once

#include "BLI_time.h"        /* for BLI_check_seconds_timer */
#include "BLI_utildefines.h" /* for AT */

#define TIMEIT_START(var) \
  { \
    double _timeit_##var = BLI_check_seconds_timer(); \
    printf("time start (" #var "):  " AT "\n"); \
    fflush(stdout); \
    { \
      (void)0

/**
 * \return the time since TIMEIT_START was called.
 */
#define TIMEIT_VALUE(var) (float)(BLI_check_seconds_timer() - _timeit_##var)

#define TIMEIT_VALUE_PRINT(var) \
  { \
    printf("time update   (" #var \
           "): %.6f" \
           "  " AT "\n", \
           TIMEIT_VALUE(var)); \
    fflush(stdout); \
  } \
  (void)0

#define TIMEIT_END(var) \
  } \
  printf("time end   (" #var \
         "): %.6f" \
         "  " AT "\n", \
         TIMEIT_VALUE(var)); \
  fflush(stdout); \
  } \
  (void)0

/**
 * _AVERAGED variants do same thing as their basic counterpart,
 * but additionally add elapsed time to an averaged static value,
 * useful to get sensible timing of code running fast and often.
 */
#define TIMEIT_START_AVERAGED(var) \
  { \
    static float _sum_##var = 0.0f; \
    static float _num_##var = 0.0f; \
    double _timeit_##var = BLI_check_seconds_timer(); \
    printf("time start    (" #var "):  " AT "\n"); \
    fflush(stdout); \
    { \
      (void)0

#define TIMEIT_AVERAGED_VALUE(var) (_num##var ? (_sum_##var / _num_##var) : 0.0f)

#define TIMEIT_END_AVERAGED(var) \
  } \
  const float _delta_##var = TIMEIT_VALUE(var); \
  _sum_##var += _delta_##var; \
  _num_##var++; \
  printf("time end      (" #var \
         "): %.6f" \
         "  " AT "\n", \
         _delta_##var); \
  printf("time averaged (" #var "): %.6f (total: %.6f, in %d runs)\n", \
         (_sum_##var / _num_##var), \
         _sum_##var, \
         (int)_num_##var); \
  fflush(stdout); \
  } \
  (void)0

/**
 * Given some function/expression:
 *   TIMEIT_BENCH(some_function(), some_unique_description);
 */
#define TIMEIT_BENCH(expr, id) \
  { \
    TIMEIT_START(id); \
    (expr); \
    TIMEIT_END(id); \
  } \
  (void)0

#define TIMEIT_BLOCK_INIT(id) double _timeit_var_##id = 0

#define TIMEIT_BLOCK_START(id) \
  { \
    double _timeit_block_start_##id = BLI_check_seconds_timer(); \
    { \
      (void)0

#define TIMEIT_BLOCK_END(id) \
  } \
  _timeit_var_##id += (BLI_check_seconds_timer() - _timeit_block_start_##id); \
  } \
  (void)0

#define TIMEIT_BLOCK_VALUE(id) (float)(_timeit_var_##id)

#define TIMEIT_BLOCK_STATS(id) \
  { \
    printf("%s time (in seconds): %f\n", #id, _timeit_var_##id); \
    fflush(stdout); \
  } \
  (void)0
