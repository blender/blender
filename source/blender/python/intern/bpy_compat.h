/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* This file is only to contain definitions to functions that enable
 * the python api to compile with different python versions.
 * no utility functions please
 */

#ifndef BPY_COMPAT_H__
#define BPY_COMPAT_H__

/* if you are NOT using python 3.0 - define these */
#if PY_VERSION_HEX < 0x03000000
#define _PyUnicode_AsString PyString_AsString

#undef PyUnicode_Check
#define PyUnicode_Check PyString_Check

#define PyLong_FromSize_t PyInt_FromLong
#define PyLong_AsSsize_t PyInt_AsLong

#undef PyLong_Check
#define PyLong_Check PyInt_Check


#ifdef PyUnicode_FromString
#undef PyUnicode_FromString
#endif
#define PyUnicode_FromString PyString_FromString

#ifdef PyUnicode_FromFormat
#undef PyUnicode_FromFormat
#endif
#define PyUnicode_FromFormat PyString_FromFormat

#endif

/* older then python 2.6 - define these */
// #if (PY_VERSION_HEX < 0x02060000)
// #endif

/* older then python 2.5 - define these */
#if (PY_VERSION_HEX < 0x02050000)
#define Py_ssize_t ssize_t
#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE	return Py_BuildValue("O", Py_None)
#endif
#ifndef Py_RETURN_FALSE
#define Py_RETURN_FALSE  return PyBool_FromLong(0) 
#endif
#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE  return PyBool_FromLong(1)
#endif
#endif


/* defined in bpy_util.c */
#if PY_VERSION_HEX < 0x03000000
PyObject *Py_CmpToRich(int op, int cmp);
#endif

#ifndef Py_CmpToRich
PyObject *Py_CmpToRich(int op, int cmp); /* bpy_util.c */
#endif

#endif /* BPY_COMPAT_H__ */
