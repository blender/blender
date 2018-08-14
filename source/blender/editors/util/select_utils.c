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
 *  \ingroup editors
 */

#include "BLI_utildefines.h"

#include "ED_select_utils.h"

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
