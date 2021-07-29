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

#ifndef __BMESH_CALLBACK_GENERIC_H__
#define __BMESH_CALLBACK_GENERIC_H__

/** \file blender/bmesh/intern/bmesh_callback_generic.h
 *  \ingroup bmesh
 */

bool BM_elem_cb_check_hflag_enabled(BMElem *, void *user_data);
bool BM_elem_cb_check_hflag_disabled(BMElem *, void *user_data);
bool BM_elem_cb_check_hflag_ex(BMElem *, void *user_data);
bool BM_elem_cb_check_elem_not_equal(BMElem *ele, void *user_data);

#define BM_elem_cb_check_hflag_ex_simple(type, hflag_p, hflag_n) \
	(bool (*)(type, void *))BM_elem_cb_check_hflag_ex, \
	SET_UINT_IN_POINTER(((hflag_p) | (hflag_n << 8)))

#define BM_elem_cb_check_hflag_enabled_simple(type, hflag_p) \
	(bool (*)(type, void *))BM_elem_cb_check_hflag_enabled, \
	SET_UINT_IN_POINTER((hflag_p))

#define BM_elem_cb_check_hflag_disabled_simple(type, hflag_n) \
	(bool (*)(type, void *))BM_elem_cb_check_hflag_disabled, \
	SET_UINT_IN_POINTER(hflag_n)

#endif /* __BMESH_CALLBACK_GENERIC_H__ */
