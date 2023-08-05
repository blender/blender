/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct KDTree_1d;
struct PointerRNA;
struct wmOperatorType;

enum {
  SEL_TOGGLE = 0,
  SEL_SELECT = 1,
  SEL_DESELECT = 2,
  SEL_INVERT = 3,
};

enum WalkSelectDirection {
  UI_SELECT_WALK_UP,
  UI_SELECT_WALK_DOWN,
  UI_SELECT_WALK_LEFT,
  UI_SELECT_WALK_RIGHT,
};

/** See #WM_operator_properties_select_operation */
enum eSelectOp {
  SEL_OP_ADD = 1,
  SEL_OP_SUB,
  SEL_OP_SET,
  SEL_OP_AND,
  SEL_OP_XOR,
};

/* Select Similar */
enum eSimilarCmp {
  SIM_CMP_EQ = 0,
  SIM_CMP_GT,
  SIM_CMP_LT,
};

#define SEL_OP_USE_OUTSIDE(sel_op) (ELEM(sel_op, SEL_OP_AND))
#define SEL_OP_USE_PRE_DESELECT(sel_op) (ELEM(sel_op, SEL_OP_SET))
#define SEL_OP_CAN_DESELECT(sel_op) (!ELEM(sel_op, SEL_OP_ADD))

/**
 * Use when we've de-selected all first for 'SEL_OP_SET'.
 * 1: select, 0: deselect, -1: pass.
 */
int ED_select_op_action(eSelectOp sel_op, bool is_select, bool is_inside);
/**
 * Use when we've de-selected all items first (for modes that need it).
 *
 * \note In some cases changing selection needs to perform other checks,
 * so it's more straightforward to deselect all, then select.
 */
int ED_select_op_action_deselected(eSelectOp sel_op, bool is_select, bool is_inside);

bool ED_select_similar_compare_float(float delta, float thresh, eSimilarCmp compare);
bool ED_select_similar_compare_float_tree(const KDTree_1d *tree,
                                          float length,
                                          float thresh,
                                          eSimilarCmp compare);

/**
 * Utility to use for selection operations that run multiple times (circle select).
 */
eSelectOp ED_select_op_modal(eSelectOp sel_op, bool is_first);

/** Argument passed to picking functions. */
struct SelectPick_Params {
  /**
   * - #SEL_OP_ADD named "extend" from operators.
   * - #SEL_OP_SUB named "deselect" from operators.
   * - #SEL_OP_XOR named "toggle" from operators.
   * - #SEL_OP_AND (never used for picking).
   * - #SEL_OP_SET use when "extend", "deselect" and "toggle" are all disabled.
   */
  eSelectOp sel_op;
  /** Deselect all, even when there is nothing found at the cursor location. */
  bool deselect_all;
  /**
   * When selecting an element that is already selected, do nothing (passthrough).
   * don't even make it active.
   * Use to implement tweaking to move the selection without first de-selecting.
   */
  bool select_passthrough;
};

/**
 * Utility to get #eSelectPickMode from booleans for convenience.
 */
eSelectOp ED_select_op_from_operator(PointerRNA *ptr) ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT;

/**
 * Initialize `params` from `op`,
 * these properties are defined by #WM_operator_properties_mouse_select.
 */
void ED_select_pick_params_from_operator(PointerRNA *ptr, SelectPick_Params *params)
    ATTR_NONNULL(1, 2);

/**
 * Get-name callback for #wmOperatorType.get_name, this is mainly useful so the selection
 * action is shown in the status-bar.
 */
const char *ED_select_pick_get_name(wmOperatorType *ot, PointerRNA *ptr);
const char *ED_select_circle_get_name(wmOperatorType *ot, PointerRNA *ptr);
