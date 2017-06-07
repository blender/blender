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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_app_opensubdiv.c
 *  \ingroup pythonintern
 */

#include <Python.h>
#include "BLI_utildefines.h"

#include "bpy_app_opensubdiv.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
#endif

static PyTypeObject BlenderAppOpenSubdivType;

static PyStructSequence_Field app_opensubdiv_info_fields[] = {
	{(char *)"supported", (char *)("Boolean, True when Blender is built with OpenSubdiv support")},
	{(char *)("version"), (char *)("The OpenSubdiv version as a tuple of 3 numbers")},
	{(char *)("version_string"), (char *)("The OpenSubdiv version formatted as a string")},
	{NULL}
};

static PyStructSequence_Desc app_opensubdiv_info_desc = {
	(char *)"bpy.app.opensubdiv",     /* name */
	(char *)"This module contains information about OpenSubdiv blender is linked against",    /* doc */
	app_opensubdiv_info_fields,    /* fields */
	ARRAY_SIZE(app_opensubdiv_info_fields) - 1
};

static PyObject *make_opensubdiv_info(void)
{
	PyObject *opensubdiv_info;
	int pos = 0;

	opensubdiv_info = PyStructSequence_New(&BlenderAppOpenSubdivType);
	if (opensubdiv_info == NULL) {
		return NULL;
	}

#ifndef WITH_OPENSUBDIV
#define SetStrItem(str) \
	PyStructSequence_SET_ITEM(opensubdiv_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) \
	PyStructSequence_SET_ITEM(opensubdiv_info, pos++, obj)

#ifdef WITH_OPENSUBDIV
	int curversion = openSubdiv_getVersionHex();
	SetObjItem(PyBool_FromLong(1));
	SetObjItem(Py_BuildValue("(iii)",
	                         curversion / 10000, (curversion / 100) % 100, curversion % 100));
	SetObjItem(PyUnicode_FromFormat("%2d, %2d, %2d",
	                                curversion / 10000, (curversion / 100) % 100, curversion % 100));
#else
	SetObjItem(PyBool_FromLong(0));
	SetObjItem(Py_BuildValue("(iii)", 0, 0, 0));
	SetStrItem("Unknown");
#endif

	if (PyErr_Occurred()) {
		Py_CLEAR(opensubdiv_info);
		return NULL;
	}

#undef SetStrItem
#undef SetObjItem

	return opensubdiv_info;
}

PyObject *BPY_app_opensubdiv_struct(void)
{
	PyObject *ret;

	PyStructSequence_InitType(&BlenderAppOpenSubdivType, &app_opensubdiv_info_desc);

	ret = make_opensubdiv_info();

	/* prevent user from creating new instances */
	BlenderAppOpenSubdivType.tp_init = NULL;
	BlenderAppOpenSubdivType.tp_new = NULL;
	/* without this we can't do set(sys.modules) [#29635] */
	BlenderAppOpenSubdivType.tp_hash = (hashfunc)_Py_HashPointer;

	return ret;
}
