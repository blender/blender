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

/** \file blender/python/generic/python_utildefines.h
 *  \ingroup pygen
 *  \brief header-only utilities
 *  \note light addition to Python.h, use py_capi_utils.h for larger features.
 */

#ifndef __PYTHON_UTILDEFINES_H__
#define __PYTHON_UTILDEFINES_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PyTuple_SET_ITEMS(op_arg, ...) \
{ \
	PyTupleObject *op = (PyTupleObject *)op_arg; \
	PyObject **ob_items = op->ob_item; \
	CHECK_TYPE_ANY(op_arg, PyObject *, PyTupleObject *); \
	BLI_assert(_VA_NARGS_COUNT(__VA_ARGS__) == PyTuple_GET_SIZE(op)); \
	ARRAY_SET_ITEMS(ob_items, __VA_ARGS__); \
} (void)0

/* wrap Py_INCREF & return the result,
 * use sparingly to avoid comma operator or temp var assignment */
BLI_INLINE PyObject *Py_INCREF_RET(PyObject *op) { Py_INCREF(op); return op; }

/* append & transfer ownership to the list, avoids inline Py_DECREF all over (which is quite a large macro) */
BLI_INLINE int PyList_APPEND(PyObject *op, PyObject *v)
{
	int ret = PyList_Append(op, v);
	Py_DecRef(v);
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif  /* __PYTHON_UTILDEFINES_H__ */

