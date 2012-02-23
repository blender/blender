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

/** \file blender/python/bmesh/bmesh_py_api.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh.utils' module.
 * Utility functions for operating on 'bmesh.types'
 */



#include <Python.h>

#include "bmesh.h"

#include "bmesh_py_types.h"

#include "BLI_utildefines.h"

#include "bmesh_py_utils.h" /* own include */

PyDoc_STRVAR(bpy_bm_utils_edge_split_doc,
".. method:: edge_split(vert, edge, fac)\n"
"\n"
"   Split an edge, return the newly created data.\n"
"\n"
"   :arg edge: The edge to split.\n"
"   :type edge: :class:`bmesh.tupes.BMEdge`\n"
"   :arg vert: One of the verts on the edge, defines the split direction.\n"
"   :type vert: :class:`bmesh.tupes.BMVert`\n"
"   :arg fac: The point on the edge where the new vert will be created.\n"
"   :type fac: float\n"
"   :return: The newly created (edge, vert) pair.\n"
"   :rtype: tuple\n"
);

static PyObject *bpy_bm_utils_edge_split(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMEdge *py_edge;
	BPy_BMVert *py_vert;
	float fac;

	BMesh *bm;
	BMVert *v_new = NULL;
	BMEdge *e_new = NULL;

	if (!PyArg_ParseTuple(args, "O!O!f:edge_split",
	                      &BPy_BMEdge_Type, &py_edge,
	                      &BPy_BMVert_Type, &py_vert,
	                      &fac))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_edge);
	BPY_BM_CHECK_OBJ(py_vert);

	if (!(py_edge->e->v1 == py_vert->v ||
	      py_edge->e->v2 == py_vert->v))
	{
		PyErr_SetString(PyExc_ValueError,
		                "edge_split(edge, vert): the vertex is not found in the edge");
		return NULL;
	}

	bm = py_edge->bm;

	v_new = BM_edge_split(bm, py_edge->e, py_vert->v, &e_new, fac);

	if (v_new && e_new) {
		PyObject *ret = PyTuple_New(2);
		PyTuple_SET_ITEM(ret, 0, BPy_BMEdge_CreatePyObject(bm, e_new));
		PyTuple_SET_ITEM(ret, 1, BPy_BMVert_CreatePyObject(bm, v_new));
		return ret;
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "edge_split(edge, vert): couldn't split the edge, internal error");
		return NULL;
	}
}

static struct PyMethodDef BPy_BM_utils_methods[] = {
	{"edge_split", (PyCFunction)bpy_bm_utils_edge_split, METH_VARARGS, bpy_bm_utils_edge_split_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_BM_doc,
"This module provides access to blenders bmesh data structures."
);
static struct PyModuleDef BPy_BM_types_module_def = {
	PyModuleDef_HEAD_INIT,
	"bmesh.utils",  /* m_name */
	BPy_BM_doc,  /* m_doc */
	0,  /* m_size */
	BPy_BM_utils_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit_bmesh_utils(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BM_types_module_def);

	return submodule;
}
