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
 * This file defines the 'bmesh' module.
 */

#include <Python.h>

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_utils.h"
#include "bmesh_py_select.h"

#include "BLI_utildefines.h"

#include "BKE_tessmesh.h"
#include "BKE_depsgraph.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "../generic/py_capi_utils.h"

#include "bmesh_py_api.h" /* own include */



PyDoc_STRVAR(bpy_bm_new_doc,
".. method:: new()\n"
"\n"
"   :return: Retyrn a new, empty mesh.\n"
"   :rtype: :class:`BMesh`\n"
);

static PyObject *bpy_bm_new(PyObject *UNUSED(self))
{
	BPy_BMesh *py_bmesh;
	BMesh *bm;

	bm = BM_mesh_create(NULL, &bm_mesh_allocsize_default);

	py_bmesh = (BPy_BMesh *)BPy_BMesh_CreatePyObject(bm);
	py_bmesh->py_owns = TRUE;
	return (PyObject *)py_bmesh;
}

PyDoc_STRVAR(bpy_bm_from_mesh_doc,
".. method:: from_mesh(mesh)\n"
"\n"
"   Return a BMesh from this mesh, currently the mesh must already be in editmode.\n"
"\n"
"   :return: the BMesh assosiated with this mesh.\n"
"   :rtype: :class:`bmesh.types.BMesh`\n"
);

static PyObject *bpy_bm_from_mesh(PyObject *UNUSED(self), PyObject *value)
{
	BPy_BMesh *py_bmesh;
	BMesh *bm;
	Mesh *me = PyC_RNA_AsPointer(value, "Mesh");
	int py_owns;

	if (me == NULL) {
		return NULL;
	}

	/* temp! */
	if (!me->edit_btmesh) {
		bm = BM_mesh_create(NULL, &bm_mesh_allocsize_default);
		BM_mesh_to_bmesh(bm, me, 0, 0); /* BMESH_TODO add args */
		py_owns = TRUE;
	}
	else {
		bm = me->edit_btmesh->bm;
		py_owns = FALSE;
	}

	py_bmesh = (BPy_BMesh *)BPy_BMesh_CreatePyObject(bm);
	py_bmesh->py_owns = py_owns;
	return (PyObject *)py_bmesh;
}

PyDoc_STRVAR(bpy_bm_to_mesh_doc,
".. method:: to_mesh(mesh, bmesh)\n"
"\n"
"   Return a BMesh from this mesh, currently the mesh must already be in editmode.\n"
"\n"
"   :return: the BMesh assosiated with this mesh.\n"
"   :rtype: :class:`bmesh.types.BMesh`\n"
);

static PyObject *bpy_bm_to_mesh(PyObject *UNUSED(self), PyObject *args)
{
	PyObject  *py_mesh;
	BPy_BMesh *py_bmesh;
	Mesh  *me;
	BMesh *bm;

	if (!PyArg_ParseTuple(args, "OO!:to_mesh", &py_mesh, &BPy_BMesh_Type, &py_bmesh) ||
	    !(me = PyC_RNA_AsPointer(py_mesh, "Mesh")))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_bmesh);

	if (me->edit_btmesh) {
		PyErr_Format(PyExc_ValueError,
		             "to_mesh(): Mesh '%s' is in editmode", me->id.name + 2);
		return NULL;
	}

	bm = py_bmesh->bm;

	BM_mesh_from_bmesh(bm, me, FALSE);

	/* we could have the user do this but if they forget blender can easy crash
	 * since the references arrays for the objects derived meshes are now invalid */
	DAG_id_tag_update(&me->id, OB_RECALC_DATA);

	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_BM_methods[] = {
    /* THESE NAMES MAY CHANGE! */
    {"new",       (PyCFunction)bpy_bm_new,       METH_NOARGS,  bpy_bm_new_doc},
    {"from_mesh", (PyCFunction)bpy_bm_from_mesh, METH_O,       bpy_bm_from_mesh_doc},
    {"to_mesh",   (PyCFunction)bpy_bm_to_mesh,   METH_VARARGS, bpy_bm_to_mesh_doc},
    {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_BM_doc,
"This module provides access to blenders bmesh data structures.\n"
"\n"
"\n"
"Submodules:\n"
"\n"
"* :mod:`bmesh.utils`\n"
"* :mod:`bmesh.types`\n"
);
static struct PyModuleDef BPy_BM_module_def = {
	PyModuleDef_HEAD_INIT,
	"bmesh",  /* m_name */
	BPy_BM_doc,  /* m_doc */
	0,  /* m_size */
	BPy_BM_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit_bmesh(void)
{
	PyObject *mod;
	PyObject *submodule;
	PyObject *sys_modules = PySys_GetObject("modules"); /* not pretty */

	BPy_BM_init_types();
	BPy_BM_init_select_types();


	mod = PyModule_Create(&BPy_BM_module_def);

	/* bmesh.types */
	PyModule_AddObject(mod, "types", (submodule=BPyInit_bmesh_types()));
	PyDict_SetItemString(sys_modules, "bmesh.types", submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "utils", (submodule=BPyInit_bmesh_utils()));
	PyDict_SetItemString(sys_modules, "bmesh.utils", submodule);
	Py_INCREF(submodule);

	return mod;
}
