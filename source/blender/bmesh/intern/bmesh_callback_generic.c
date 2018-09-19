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

/** \file blender/bmesh/intern/bmesh_callback_generic.c
 *  \ingroup bmesh
 *
 * BM element callback functions.
 */

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_callback_generic.h"

bool BM_elem_cb_check_hflag_ex(BMElem *ele, void *user_data)
{
	const uint hflag_pair = POINTER_AS_INT(user_data);
	const char hflag_p = (hflag_pair & 0xff);
	const char hflag_n = (hflag_pair >> 8);

	return ((BM_elem_flag_test(ele, hflag_p) != 0) &&
	        (BM_elem_flag_test(ele, hflag_n) == 0));
}

bool BM_elem_cb_check_hflag_enabled(BMElem *ele, void *user_data)
{
	const char hflag = POINTER_AS_INT(user_data);

	return (BM_elem_flag_test(ele, hflag) != 0);
}

bool BM_elem_cb_check_hflag_disabled(BMElem *ele, void *user_data)
{
	const char hflag = POINTER_AS_INT(user_data);

	return (BM_elem_flag_test(ele, hflag) == 0);
}

bool BM_elem_cb_check_elem_not_equal(BMElem *ele, void *user_data)
{
	return (ele != user_data);
}
