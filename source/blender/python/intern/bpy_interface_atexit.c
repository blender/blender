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

/** \file blender/python/intern/bpy_interface_atexit.c
 *  \ingroup pythonintern
 *
 * This file inserts an exit callback into pythons 'atexit' module.
 * Without this sys.exit() can crash because blender is not properly closing
 * resources.
 */


#include <Python.h>

#include "bpy_util.h"

#include "WM_api.h"

#include "BLI_utildefines.h"

static PyObject *bpy_atexit(PyObject *UNUSED(self), PyObject *UNUSED(args), PyObject *UNUSED(kw))
{
	/* close down enough of blender at least not to crash */
	struct bContext *C = BPy_GetContext();

	WM_exit_ext(C, 0);

	Py_RETURN_NONE;
}

static PyMethodDef meth_bpy_atexit = {"bpy_atexit", (PyCFunction)bpy_atexit, METH_NOARGS, NULL};
static PyObject *func_bpy_atregister = NULL; /* borrowed referebce, atexit holds */

static void atexit_func_call(const char *func_name, PyObject *atexit_func_arg)
{
	/* note - no error checking, if any of these fail we'll get a crash
	 * this is intended, but if its problematic it could be changed
	 * - campbell */

	PyObject *atexit_mod = PyImport_ImportModuleLevel((char *)"atexit", NULL, NULL, NULL, 0);
	PyObject *atexit_func = PyObject_GetAttrString(atexit_mod, func_name);
	PyObject *args = PyTuple_New(1);
	PyObject *ret;

	PyTuple_SET_ITEM(args, 0, atexit_func_arg);
	Py_INCREF(atexit_func_arg); /* only incref so we don't dec'ref along with 'args' */

	ret = PyObject_CallObject(atexit_func, args);

	Py_DECREF(atexit_mod);
	Py_DECREF(atexit_func);
	Py_DECREF(args);

	if (ret) {
		Py_DECREF(ret);
	}
	else { /* should never happen */
		PyErr_Print();
	}
}

void BPY_atexit_register(void)
{
	/* atexit module owns this new function reference */
	BLI_assert(func_bpy_atregister == NULL);

	func_bpy_atregister = (PyObject *)PyCFunction_New(&meth_bpy_atexit, NULL);
	atexit_func_call("register", func_bpy_atregister);
}

void BPY_atexit_unregister(void)
{
	BLI_assert(func_bpy_atregister != NULL);

	atexit_func_call("unregister", func_bpy_atregister);
	func_bpy_atregister = NULL; /* don't really need to set but just in case */
}
