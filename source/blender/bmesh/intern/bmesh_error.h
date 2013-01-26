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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_ERROR_H__
#define __BMESH_ERROR_H__

/** \file blender/bmesh/intern/bmesh_error.h
 *  \ingroup bmesh
 */

/*----------- bmop error system ----------*/

/* pushes an error onto the bmesh error stack.
 * if msg is null, then the default message for the errorcode is used.*/
void BMO_error_raise(BMesh *bm, BMOperator *owner, int errcode, const char *msg);

/* gets the topmost error from the stack.
 * returns error code or 0 if no error.*/
int  BMO_error_get(BMesh *bm, const char **msg, BMOperator **op);
bool BMO_error_occurred(BMesh *bm);

/* same as geterror, only pops the error off the stack as well */
int  BMO_error_pop(BMesh *bm, const char **msg, BMOperator **op);
void BMO_error_clear(BMesh *bm);

/* this is meant for handling errors, like self-intersection test failures.
 * it's dangerous to handle errors in general though, so disabled for now. */

/* catches an error raised by the op pointed to by catchop.
 * errorcode is either the errorcode, or BMERR_ALL for any
 * error.*/

/* not yet implemented.
 * int BMO_error_catch_op(BMesh *bm, BMOperator *catchop, int errorcode, char **msg);
 */

#define BM_ELEM_INDEX_VALIDATE(_bm, _msg_a, _msg_b) \
	BM_mesh_elem_index_validate(_bm, __FILE__ ":" STRINGIFY(__LINE__), __func__, _msg_a, _msg_b)

/*------ error code defines -------*/

/*error messages*/
#define BMERR_SELF_INTERSECTING			1
#define BMERR_DISSOLVEDISK_FAILED		2
#define BMERR_CONNECTVERT_FAILED		3
#define BMERR_WALKER_FAILED				4
#define BMERR_DISSOLVEFACES_FAILED		5
#define BMERR_DISSOLVEVERTS_FAILED		6
#define BMERR_TESSELLATION				7
#define BMERR_NONMANIFOLD				8
#define BMERR_INVALID_SELECTION			9
#define BMERR_MESH_ERROR				10
#define BMERR_CONVEX_HULL_FAILED		11

/* BMESH_ASSERT */
#ifdef WITH_ASSERT_ABORT
#  define _BMESH_DUMMY_ABORT abort
#else
#  define _BMESH_DUMMY_ABORT() (void)0
#endif

/* this is meant to be higher level then BLI_assert(),
 * its enabled even when in Release mode*/
#define BMESH_ASSERT(a)                                                       \
	(void)((!(a)) ?  (                                                        \
		(                                                                     \
		fprintf(stderr,                                                       \
		        "BMESH_ASSERT failed: %s, %s(), %d at \'%s\'\n",              \
		        __FILE__, __func__, __LINE__, STRINGIFY(a)),                  \
		_BMESH_DUMMY_ABORT(),                                                 \
		NULL)) : NULL)

#endif /* __BMESH_ERROR_H__ */
