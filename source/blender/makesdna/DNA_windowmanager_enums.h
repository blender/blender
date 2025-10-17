/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

/**
 * Operator type return flags: exec(), invoke() modal(), return values.
 */
enum wmOperatorStatus {
  OPERATOR_RUNNING_MODAL = (1 << 0),
  OPERATOR_CANCELLED = (1 << 1),
  OPERATOR_FINISHED = (1 << 2),
  /** Add this flag if the event should pass through. */
  OPERATOR_PASS_THROUGH = (1 << 3),
  /** In case operator got executed outside WM code (like via file-select). */
  OPERATOR_HANDLED = (1 << 4),
  /**
   * Used for operators that act indirectly (eg. popup menu).
   * \note this isn't great design (using operators to trigger UI) avoid where possible.
   */
  OPERATOR_INTERFACE = (1 << 5),
};
#define OPERATOR_FLAGS_ALL \
  (OPERATOR_RUNNING_MODAL | OPERATOR_CANCELLED | OPERATOR_FINISHED | OPERATOR_PASS_THROUGH | \
   OPERATOR_HANDLED | OPERATOR_INTERFACE | 0)

/* sanity checks for debug mode only */
#define OPERATOR_RETVAL_CHECK(ret) \
  { \
    CHECK_TYPE(ret, wmOperatorStatus); \
    BLI_assert(ret != 0 && (ret & OPERATOR_FLAGS_ALL) == ret); \
  } \
  ((void)0)

ENUM_OPERATORS(wmOperatorStatus);

/** #wmOperator.flag */
enum {
  /**
   * Low level flag so exec() operators can tell if they were invoked, use with care.
   * Typically this shouldn't make any difference, but it rare cases its needed (see smooth-view).
   */
  OP_IS_INVOKE = (1 << 0),
  /** So we can detect if an operators exec() call is activated by adjusting the last action. */
  OP_IS_REPEAT = (1 << 1),
  /**
   * So we can detect if an operators exec() call is activated from #SCREEN_OT_repeat_last.
   *
   * This difference can be important because previous settings may be used,
   * even with #PROP_SKIP_SAVE the repeat last operator will use the previous settings.
   * Unlike #OP_IS_REPEAT the selection (and context generally) may be different each time.
   * See #60777 for an example of when this is needed.
   */
  OP_IS_REPEAT_LAST = (1 << 2),

  /** When the cursor is grabbed */
  OP_IS_MODAL_GRAB_CURSOR = (1 << 3),

  /**
   * Allow modal operators to have the region under the cursor for their context
   * (the region-type is maintained to prevent errors).
   */
  OP_IS_MODAL_CURSOR_REGION = (1 << 4),
};
