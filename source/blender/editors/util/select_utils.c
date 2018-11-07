/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file select_utils.c
 *  \ingroup edutil
 */

#include "BLI_utildefines.h"
#include "BLI_kdtree.h"
#include "BLI_math.h"

#include "ED_select_utils.h"

#include "float.h"

/** 1: select, 0: deselect, -1: pass. */
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
	BLI_assert(!"invalid sel_op");
	return -1;
}
/**
 * Use when we've de-selected all items first (for modes that need it).
 *
 * \note In some cases changing selection needs to perform other checks,
 * so it's more straightforward to deselect all, then select.
 */
int ED_select_op_action_deselected(const eSelectOp sel_op, const bool is_select, const bool is_inside)
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
	BLI_assert(!"invalid sel_op");
	return -1;
}

int ED_select_similar_compare_float(const float delta, const float thresh, const int compare)
{
	switch (compare) {
		case SIM_CMP_EQ:
			return (fabsf(delta) < thresh + FLT_EPSILON);
		case SIM_CMP_GT:
			return ((delta + thresh) > -FLT_EPSILON);
		case SIM_CMP_LT:
			return ((delta - thresh) < FLT_EPSILON);
		default:
			BLI_assert(0);
			return 0;
	}
}

bool ED_select_similar_compare_float_tree(const KDTree *tree, const float length, const float thresh, const int compare)
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
			 * For example, in a tree with 1, 2 and 3, which is stored squared as 1, 4, 9, it returns as the nearest
			 * length/node the "4" instead of "1". */
			nearest_edge_length = -1.0f;
			break;
		case SIM_CMP_LT:
			/* Compare against the longest edge. */
			nearest_edge_length = FLT_MAX;
			break;
		default:
			BLI_assert(0);
			return false;
	}

	KDTreeNearest nearest;
	float dummy[3] = {nearest_edge_length, 0.0f, 0.0f};
	if (BLI_kdtree_find_nearest(tree, dummy, &nearest) != -1) {
		float delta = length - nearest.co[0];
		return ED_select_similar_compare_float(delta, thresh, compare);
	}

	return false;
}
