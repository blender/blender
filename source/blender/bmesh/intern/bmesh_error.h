/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "bmesh_operator_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------- bmop error system ----------*/

/**
 * \note More can be added as needed.
 */
typedef enum eBMOpErrorLevel {
  /**
   * Use when the operation could not succeed,
   * typically from input that isn't sufficient for completing the operation.
   */
  BMO_ERROR_CANCEL = 0,
  /**
   * Use this when one or more operations could not succeed,
   * when the resulting mesh can be used (since some operations succeeded or no change was made).
   * This is used by default.
   */
  BMO_ERROR_WARN = 1,
  /**
   * The mesh resulting from this operation should not be used (where possible).
   * It should not be left in a corrupt state either.
   *
   * See #BMBackup type & function calls.
   */
  BMO_ERROR_FATAL = 2,
} eBMOpErrorLevel;

/**
 * Pushes an error onto the bmesh error stack.
 * if msg is null, then the default message for the `errcode` is used.
 */
void BMO_error_raise(BMesh *bm, BMOperator *owner, eBMOpErrorLevel level, const char *msg)
    ATTR_NONNULL(1, 2, 4);

/**
 * Gets the topmost error from the stack.
 * returns error code or 0 if no error.
 */
bool BMO_error_get(BMesh *bm, const char **r_msg, BMOperator **r_op, eBMOpErrorLevel *r_level);
bool BMO_error_get_at_level(BMesh *bm,
                            eBMOpErrorLevel level,
                            const char **r_msg,
                            BMOperator **r_op);
bool BMO_error_occurred_at_level(BMesh *bm, eBMOpErrorLevel level);

/* Same as #BMO_error_get, only pops the error off the stack as well. */
bool BMO_error_pop(BMesh *bm, const char **r_msg, BMOperator **r_op, eBMOpErrorLevel *r_level);
void BMO_error_clear(BMesh *bm);

/* This is meant for handling errors, like self-intersection test failures.
 * it's dangerous to handle errors in general though, so disabled for now. */

/* Catches an error raised by the op pointed to by catchop. */
/* Not yet implemented. */
// int BMO_error_catch_op(BMesh *bm, BMOperator *catchop, char **msg);

#define BM_ELEM_INDEX_VALIDATE(_bm, _msg_a, _msg_b) \
  BM_mesh_elem_index_validate(_bm, __FILE__ ":" STRINGIFY(__LINE__), __func__, _msg_a, _msg_b)

/* BMESH_ASSERT */
#ifdef WITH_ASSERT_ABORT
#  define _BMESH_DUMMY_ABORT abort
#else
#  define _BMESH_DUMMY_ABORT() (void)0
#endif

/**
 * This is meant to be higher level than BLI_assert(),
 * its enabled even when in Release mode.
 */
#define BMESH_ASSERT(a) \
  (void)((!(a)) ? ((fprintf(stderr, \
                            "BMESH_ASSERT failed: %s, %s(), %d at \'%s\'\n", \
                            __FILE__, \
                            __func__, \
                            __LINE__, \
                            STRINGIFY(a)), \
                    _BMESH_DUMMY_ABORT(), \
                    NULL)) : \
                  NULL)

#ifdef __cplusplus
}
#endif
