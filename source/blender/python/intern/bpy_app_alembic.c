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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_app_alembic.c
 *  \ingroup pythonintern
 */

#include <Python.h>
#include "BLI_utildefines.h"

#include "bpy_app_alembic.h"

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

static PyTypeObject BlenderAppABCType;

static PyStructSequence_Field app_alembic_info_fields[] = {
	{(char *)"supported", (char *)"Boolean, True when Blender is built with Alembic support"},
	{(char *)"version", (char *)"The Alembic version as a tuple of 3 numbers"},
	{(char *)"version_string", (char *)"The Alembic version formatted as a string"},
	{NULL}
};

static PyStructSequence_Desc app_alembic_info_desc = {
	(char *)"bpy.app.alembic",     /* name */
	(char *)"This module contains information about Alembic blender is linked against",  /* doc */
	app_alembic_info_fields,    /* fields */
	ARRAY_SIZE(app_alembic_info_fields) - 1
};

static PyObject *make_alembic_info(void)
{
	PyObject *alembic_info = PyStructSequence_New(&BlenderAppABCType);

	if (alembic_info == NULL) {
		return NULL;
	}

	int pos = 0;

#ifndef WITH_ALEMBIC
#	define SetStrItem(str) \
	PyStructSequence_SET_ITEM(alembic_info, pos++, PyUnicode_FromString(str))
#endif

#define SetObjItem(obj) \
	PyStructSequence_SET_ITEM(alembic_info, pos++, obj)

#ifdef WITH_ALEMBIC
	const int curversion = ABC_get_version();
	const int major = curversion / 10000;
	const int minor = (curversion / 100) - (major * 100);
	const int patch = curversion - ((curversion / 100 ) * 100);

	SetObjItem(PyBool_FromLong(1));
	SetObjItem(Py_BuildValue("(iii)", major, minor, patch));
	SetObjItem(PyUnicode_FromFormat("%2d, %2d, %2d", major, minor, patch));
#else
	SetObjItem(PyBool_FromLong(0));
	SetObjItem(Py_BuildValue("(iii)", 0, 0, 0));
	SetStrItem("Unknown");
#endif

	if (PyErr_Occurred()) {
		Py_CLEAR(alembic_info);
		return NULL;
	}

#undef SetStrItem
#undef SetObjItem

	return alembic_info;
}

PyObject *BPY_app_alembic_struct(void)
{
	PyStructSequence_InitType(&BlenderAppABCType, &app_alembic_info_desc);

	PyObject *ret = make_alembic_info();

	/* prevent user from creating new instances */
	BlenderAppABCType.tp_init = NULL;
	BlenderAppABCType.tp_new = NULL;
	BlenderAppABCType.tp_hash = (hashfunc)_Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

	return ret;
}
