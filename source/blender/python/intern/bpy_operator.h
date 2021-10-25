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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_operator.h
 *  \ingroup pythonintern
 */

#ifndef __BPY_OPERATOR_H__
#define __BPY_OPERATOR_H__

extern PyTypeObject pyop_base_Type;

#define BPy_OperatorBase_Check(v)	(PyObject_TypeCheck(v, &pyop_base_Type))

typedef struct {
	PyObject_HEAD /* required python macro   */
} BPy_OperatorBase;

PyObject *BPY_operator_module(void);

#endif
