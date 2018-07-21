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

#include "BLI_utildefines.h"

static PyObject *bpy_intern_str_arr[16];

PyObject *bpy_intern_str___annotations__;
PyObject *bpy_intern_str___doc__;
PyObject *bpy_intern_str___main__;
PyObject *bpy_intern_str___module__;
PyObject *bpy_intern_str___name__;
PyObject *bpy_intern_str___slots__;
PyObject *bpy_intern_str_attr;
PyObject *bpy_intern_str_bl_property;
PyObject *bpy_intern_str_bl_rna;
PyObject *bpy_intern_str_bl_target_properties;
PyObject *bpy_intern_str_bpy_types;
PyObject *bpy_intern_str_frame;
PyObject *bpy_intern_str_properties;
PyObject *bpy_intern_str_register;
PyObject *bpy_intern_str_self;
PyObject *bpy_intern_str_unregister;

void bpy_intern_string_init(void)
{
	unsigned int i = 0;

#define BPY_INTERN_STR(var, str) \
	{ var = bpy_intern_str_arr[i++] = PyUnicode_FromString(str); } (void)0

	BPY_INTERN_STR(bpy_intern_str___annotations__, "__annotations__");
	BPY_INTERN_STR(bpy_intern_str___doc__, "__doc__");
	BPY_INTERN_STR(bpy_intern_str___main__, "__main__");
	BPY_INTERN_STR(bpy_intern_str___module__, "__module__");
	BPY_INTERN_STR(bpy_intern_str___name__, "__name__");
	BPY_INTERN_STR(bpy_intern_str___slots__, "__slots__");
	BPY_INTERN_STR(bpy_intern_str_attr, "attr");
	BPY_INTERN_STR(bpy_intern_str_bl_property, "bl_property");
	BPY_INTERN_STR(bpy_intern_str_bl_rna, "bl_rna");
	BPY_INTERN_STR(bpy_intern_str_bl_target_properties, "bl_target_properties");
	BPY_INTERN_STR(bpy_intern_str_bpy_types, "bpy.types");
	BPY_INTERN_STR(bpy_intern_str_frame, "frame");
	BPY_INTERN_STR(bpy_intern_str_properties, "properties");
	BPY_INTERN_STR(bpy_intern_str_register, "register");
	BPY_INTERN_STR(bpy_intern_str_self, "self");
	BPY_INTERN_STR(bpy_intern_str_unregister, "unregister");

#undef BPY_INTERN_STR

	BLI_assert(i == ARRAY_SIZE(bpy_intern_str_arr));
}

void bpy_intern_string_exit(void)
{
	unsigned int i = ARRAY_SIZE(bpy_intern_str_arr);
	while (i--) {
		Py_DECREF(bpy_intern_str_arr[i]);
	}
}
