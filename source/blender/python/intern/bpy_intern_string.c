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

/** \file blender/python/intern/bpy_intern_string.c
 *  \ingroup pythonintern
 *
 * Store python versions of strings frequently used for python lookups
 * to avoid converting, creating the hash and freeing every time as
 * PyDict_GetItemString and PyObject_GetAttrString do.
 */

#include <Python.h>

#include "bpy_intern_string.h"

PyObject *bpy_intern_str_register;
PyObject *bpy_intern_str_unregister;
PyObject *bpy_intern_str_bl_rna;
PyObject *bpy_intern_str_order;
PyObject *bpy_intern_str_attr;
PyObject *bpy_intern_str___slots__;

void bpy_intern_string_init(void)
{
	bpy_intern_str_register = PyUnicode_FromString("register");
	bpy_intern_str_unregister = PyUnicode_FromString("unregister");
	bpy_intern_str_bl_rna = PyUnicode_FromString("bl_rna");
	bpy_intern_str_order = PyUnicode_FromString("order");
	bpy_intern_str_attr = PyUnicode_FromString("attr");
	bpy_intern_str___slots__ = PyUnicode_FromString("__slots__");
}

void bpy_intern_string_exit(void)
{
	Py_DECREF(bpy_intern_str_register);
	Py_DECREF(bpy_intern_str_unregister);
	Py_DECREF(bpy_intern_str_bl_rna);
	Py_DECREF(bpy_intern_str_order);
	Py_DECREF(bpy_intern_str_attr);
	Py_DECREF(bpy_intern_str___slots__);
}
