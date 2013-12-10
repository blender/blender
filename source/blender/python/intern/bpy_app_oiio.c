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

/** \file blender/python/intern/bpy_app_oiio.c
 *  \ingroup pythonintern
 */

#include <Python.h>
#include "BLI_utildefines.h"

#include "bpy_app_oiio.h"

#ifdef WITH_OPENIMAGEIO
#  include "openimageio_api.h"
#endif

static PyTypeObject BlenderAppOIIOType;

static PyStructSequence_Field app_oiio_info_fields[] = {
	{(char *)"supported", (char *)("Boolean, True when Blender is built with OpenImageIO support")},
	{(char *)("version"), (char *)("The OpenImageIO version as a tuple of 3 numbers")},
	{(char *)("version_string"), (char *)("The OpenImageIO version formatted as a string")},
	{NULL}
};

static PyStructSequence_Desc app_oiio_info_desc = {
	(char *)"bpy.app.oiio",     /* name */
	(char *)"This module contains information about OpeImageIO blender is linked against",    /* doc */
	app_oiio_info_fields,    /* fields */
	(sizeof(app_oiio_info_fields) / sizeof(PyStructSequence_Field)) - 1
};

static PyObject *make_oiio_info(void)
{
	PyObject *oiio_info;
	int pos = 0;

#ifdef WITH_OPENIMAGEIO
	int curversion;
#endif

	oiio_info = PyStructSequence_New(&BlenderAppOIIOType);
	if (oiio_info == NULL) {
		return NULL;
	}

#ifndef WITH_OPENIMAGEIO
#define SetStrItem(str) \
	PyStructSequence_SET_ITEM(oiio_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) \
	PyStructSequence_SET_ITEM(oiio_info, pos++, obj)

#ifdef WITH_OPENIMAGEIO
	curversion = OIIO_getVersionHex();
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
		Py_CLEAR(oiio_info);
		return NULL;
	}

#undef SetStrItem
#undef SetObjItem

	return oiio_info;
}

PyObject *BPY_app_oiio_struct(void)
{
	PyObject *ret;

	PyStructSequence_InitType(&BlenderAppOIIOType, &app_oiio_info_desc);

	ret = make_oiio_info();

	/* prevent user from creating new instances */
	BlenderAppOIIOType.tp_init = NULL;
	BlenderAppOIIOType.tp_new = NULL;
	BlenderAppOIIOType.tp_hash = (hashfunc)_Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

	return ret;
}
