/*
 * $Id:
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_interface_atexit.c
 *  \ingroup pythonintern
 */


#include <Python.h>

#include "bpy_util.h"

#include "WM_api.h"

#include "BLI_utildefines.h"

static PyObject *bpy_atexit(PyObject *UNUSED(self), PyObject *UNUSED(args), PyObject *UNUSED(kw))
{
	/* close down enough of blender at least not to crash */
	struct bContext *C= BPy_GetContext();

	WM_exit_ext(C, 0);

	Py_RETURN_NONE;
}

static PyMethodDef meth_bpy_atexit= {"bpy_atexit", (PyCFunction)bpy_atexit, METH_NOARGS, NULL};

void BPY_atexit_init(void)
{
	/* note - no error checking, if any of these fail we'll get a crash
	 * this is intended, but if its problematic it could be changed
	 * - campbell */

	PyObject *atexit_mod= PyImport_ImportModuleLevel((char *)"atexit", NULL, NULL, NULL, 0);
	PyObject *atexit_register= PyObject_GetAttrString(atexit_mod, "register");
	PyObject *args= PyTuple_New(1);
	PyObject *ret;

	PyTuple_SET_ITEM(args, 0, (PyObject *)PyCFunction_New(&meth_bpy_atexit, NULL));

	ret= PyObject_CallObject(atexit_register, args);

	Py_DECREF(atexit_mod);
	Py_DECREF(atexit_register);
	Py_DECREF(args);

	if(ret) {
		Py_DECREF(ret);
	}
	else { /* should never happen */
		PyErr_Print();
	}

}
