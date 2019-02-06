/*
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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup pythonintern
 */

#include <Python.h>
#include "BLI_utildefines.h"

#include "bpy_app_openvdb.h"

#include "../generic/py_capi_utils.h"

#ifdef WITH_OPENVDB
#  include "openvdb_capi.h"
#endif

static PyTypeObject BlenderAppOVDBType;

static PyStructSequence_Field app_openvdb_info_fields[] = {
	{(char *)"supported", (char *)("Boolean, True when Blender is built with OpenVDB support")},
	{(char *)("version"), (char *)("The OpenVDB version as a tuple of 3 numbers")},
	{(char *)("version_string"), (char *)("The OpenVDB version formatted as a string")},
	{NULL},
};

static PyStructSequence_Desc app_openvdb_info_desc = {
	(char *)"bpy.app.openvdb",     /* name */
	(char *)"This module contains information about OpenVDB blender is linked against",  /* doc */
	app_openvdb_info_fields,    /* fields */
	ARRAY_SIZE(app_openvdb_info_fields) - 1,
};

static PyObject *make_openvdb_info(void)
{
	PyObject *openvdb_info;
	int pos = 0;

#ifdef WITH_OPENVDB
	int curversion;
#endif

	openvdb_info = PyStructSequence_New(&BlenderAppOVDBType);
	if (openvdb_info == NULL) {
		return NULL;
	}

#ifndef WITH_OPENVDB
#define SetStrItem(str) \
	PyStructSequence_SET_ITEM(openvdb_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) \
	PyStructSequence_SET_ITEM(openvdb_info, pos++, obj)

#ifdef WITH_OPENVDB
	curversion = OpenVDB_getVersionHex();
	SetObjItem(PyBool_FromLong(1));
	SetObjItem(PyC_Tuple_Pack_I32(curversion >> 24, (curversion >> 16) % 256, (curversion >> 8) % 256));
	SetObjItem(PyUnicode_FromFormat("%2d, %2d, %2d",
	                                curversion >> 24, (curversion >> 16) % 256, (curversion >> 8) % 256));
#else
	SetObjItem(PyBool_FromLong(0));
	SetObjItem(PyC_Tuple_Pack_I32(0, 0, 0));
	SetStrItem("Unknown");
#endif

	if (PyErr_Occurred()) {
		Py_CLEAR(openvdb_info);
		return NULL;
	}

#undef SetStrItem
#undef SetObjItem

	return openvdb_info;
}

PyObject *BPY_app_openvdb_struct(void)
{
	PyObject *ret;

	PyStructSequence_InitType(&BlenderAppOVDBType, &app_openvdb_info_desc);

	ret = make_openvdb_info();

	/* prevent user from creating new instances */
	BlenderAppOVDBType.tp_init = NULL;
	BlenderAppOVDBType.tp_new = NULL;
	BlenderAppOVDBType.tp_hash = (hashfunc)_Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

	return ret;
}
