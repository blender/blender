/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 */

#include <float.h>

#include "BLI_kdtree.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "ED_select_utils.h"

int ED_select_op_action(const eSelectOp sel_op, const bool is_select, const bool is_inside)
{
  switch (sel_op) {
    case SEL_OP_ADD:
      return (!is_select && (is_inside)) ? 1 : -1;
    case SEL_OP_SUB:
      return (is_select && is_inside) ? 0 : -1;
    case SEL_OP_SET:
      return is_inside ? 1 : 0;
    case SEL_OP_AND:
      return (is_select && is_inside) ? -1 : (is_select ? 0 : -1);
    case SEL_OP_XOR:
      return (is_select && is_inside) ? 0 : ((!is_select && is_inside) ? 1 : -1);
  }
  BLI_assert_msg(0, "invalid sel_op");
  return -1;
}
int ED_select_op_action_deselected(const eSelectOp sel_op,
                                   const bool is_select,
                                   const bool is_inside)
{
  switch (sel_op) {
    case SEL_OP_ADD:
      return (!is_select && is_inside) ? 1 : -1;
    case SEL_OP_SUB:
      return (is_select && is_inside) ? 0 : -1;
    case SEL_OP_SET:
      /* Only difference w/ function above. */
      return is_inside ? 1 : -1;
    case SEL_OP_AND:
      return (is_select && is_inside) ? -1 : (is_select ? 0 : -1);
    case SEL_OP_XOR:
      return (is_select && is_inside) ? 0 : ((!is_select && is_inside) ? 1 : -1);
  }
  BLI_assert_msg(0, "invalid sel_op");
  return -1;
}

eSelectOp ED_select_op_modal(const eSelectOp sel_op, const bool is_first)
{
  if (sel_op == SEL_OP_SET) {
    if (is_first == false) {
      return SEL_OP_ADD;
    }
  }
  return sel_op;
}

int ED_select_similar_compare_float(const float delta, const float thresh, const int compare)
{
  switch (compare) {
    case SIM_CMP_EQ:
      return (fabsf(delta) <= thresh);
    case SIM_CMP_GT:
      return ((delta + thresh) >= 0.0);
    case SIM_CMP_LT:
      return ((delta - thresh) <= 0.0);
    default:
      BLI_assert_unreachable();
      return 0;
  }
}

bool ED_select_similar_compare_float_tree(const KDTree_1d *tree,
                                          const float length,
                                          const float thresh,
                                          const int compare)
{
  /* Length of the edge we want to compare against. */
  float nearest_edge_length;

  switch (compare) {
    case SIM_CMP_EQ:
      /* Compare to the edge closest to the current edge. */
      nearest_edge_length = length;
      break;
    case SIM_CMP_GT:
      /* Compare against the shortest edge. */
      /* -FLT_MAX leads to some precision issues and the wrong edge being selected.
       * For example, in a tree with 1, 2 and 3, which is stored squared as 1, 4, 9,
       * it returns as the nearest length/node the "4" instead of "1". */
      nearest_edge_length = -1.0f;
      break;
    case SIM_CMP_LT:
      /* Compare against the longest edge. */
      nearest_edge_length = FLT_MAX;
      break;
    default:
      BLI_assert_unreachable();
      return false;
  }

  KDTreeNearest_1d nearest;
  if (BLI_kdtree_1d_find_nearest(tree, &nearest_edge_length, &nearest) != -1) {
    float delta = length - nearest.co[0];
    return ED_select_similar_compare_float(delta, thresh, compare);
  }

  return false;
}

eSelectOp ED_select_op_from_booleans(const bool extend, const bool deselect, const bool toggle)
{
  if (extend) {
    return SEL_OP_ADD;
  }
  if (deselect) {
    return SEL_OP_SUB;
  }
  if (toggle) {
    return SEL_OP_XOR;
  }
  return SEL_OP_SET;
}
