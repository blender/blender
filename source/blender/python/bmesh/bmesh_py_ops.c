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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_ops.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh.ops' module.
 * Operators from 'opdefines' are wrapped.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"

#include "bmesh_py_types.h"

#include "bmesh_py_utils.h" /* own include */

static int bpy_bm_op_as_py_error(BMesh *bm)
{
	if (BMO_error_occurred(bm)) {
		const char *errmsg;
		if (BMO_error_get(bm, &errmsg, NULL)) {
			PyErr_Format(PyExc_RuntimeError,
			             "bmesh operator: %.200s",
			             errmsg);
			return -1;
		}
	}
	return 0;
}

PyDoc_STRVAR(bpy_bm_ops_convex_hull_doc,
".. method:: convex_hull(bmesh, filter)\n"
"\n"
"   Face split with optional intermediate points.\n"
"\n"
"   :arg bmesh: The face to cut.\n"
"   :type bmesh: :class:`bmesh.types.BMFace`\n"
"   :arg filter: Set containing vertex flags to apply the operator.\n"
"   :type filter: set\n"
);
static PyObject *bpy_bm_ops_convex_hull(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"bmesh", "filter", NULL};

	BPy_BMesh *py_bm;
	BMesh *bm;

	PyObject *filter;
	int filter_flags;
	BMOperator bmop;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "O!O!:convex_hull", (char **)kwlist,
	                                 &BPy_BMesh_Type, &py_bm,
	                                 &PySet_Type,  &filter))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_bm);
	bm = py_bm->bm;

	if (filter != NULL && PyC_FlagSet_ToBitfield(bpy_bm_hflag_all_flags, filter,
	                                             &filter_flags, "convex_hull") == -1)
	{
		return NULL;
	}

	BMO_op_initf(bm, &bmop,
	             "convex_hull input=%hv",
	             filter_flags);
	BMO_op_exec(bm, &bmop);
	BMO_op_finish(bm, &bmop);

	if (bpy_bm_op_as_py_error(bm) == -1) {
		return NULL;
	}

	/* TODO, return values */
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bm_ops_remove_doubles_doc,
".. method:: remove_doubles(bmesh, filter, dist)\n"
"\n"
"   Face split with optional intermediate points.\n"
"\n"
"   :arg bmesh: The face to cut.\n"
"   :type bmesh: :class:`bmesh.types.BMFace`\n"
"   :arg filter: Set containing vertex flags to apply the operator.\n"
"   :type filter: set\n"
"   :arg dist: Distance limit.\n"
"   :type dist: float\n"
);
static PyObject *bpy_bm_ops_remove_doubles(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"bmesh", "filter", "dist", NULL};

	BPy_BMesh *py_bm;
	BMesh *bm;

	PyObject *filter;
	int filter_flags;
	float dist = 0.0f;

	BMOperator bmop;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "O!O!|f:remove_doubles", (char **)kwlist,
	                                 &BPy_BMesh_Type, &py_bm,
	                                 &PySet_Type,  &filter,
	                                 &dist))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_bm);
	bm = py_bm->bm;

	if (filter != NULL && PyC_FlagSet_ToBitfield(bpy_bm_hflag_all_flags, filter,
	                                             &filter_flags, "remove_doubles") == -1)
	{
		return NULL;
	}

	BMO_op_initf(bm, &bmop,
	             "remove_doubles verts=%hv dist=%f",
	             filter_flags, dist);
	BMO_op_exec(bm, &bmop);
	BMO_op_finish(bm, &bmop);

	if (bpy_bm_op_as_py_error(bm) == -1) {
		return NULL;
	}

	/* TODO, return values */
	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_BM_ops_methods[] = {
    {"convex_hull", (PyCFunction)bpy_bm_ops_convex_hull, METH_VARARGS | METH_KEYWORDS, bpy_bm_ops_convex_hull_doc},
    {"remove_doubles", (PyCFunction)bpy_bm_ops_remove_doubles, METH_VARARGS | METH_KEYWORDS, bpy_bm_ops_remove_doubles_doc},
    {NULL, NULL, 0, NULL}
};


PyDoc_STRVAR(BPy_BM_ops_doc,
             "This module provides access to bmesh operators (EXPEREMENTAL)."
             );
static struct PyModuleDef BPy_BM_ops_module_def = {
    PyModuleDef_HEAD_INIT,
    "bmesh.ops",  /* m_name */
    BPy_BM_ops_doc,  /* m_doc */
    0,  /* m_size */
    BPy_BM_ops_methods,  /* m_methods */
    NULL,  /* m_reload */
    NULL,  /* m_traverse */
    NULL,  /* m_clear */
    NULL,  /* m_free */
};


PyObject *BPyInit_bmesh_ops(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BM_ops_module_def);

	return submodule;
}
