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

/** \file blender/python/bmesh/bmesh_py_geometry.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh.geometry' module.
 * Utility functions for operating on 'bmesh.types'
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"
#include "bmesh_py_types.h"
#include "bmesh_py_geometry.h" /* own include */

PyDoc_STRVAR(bpy_bm_geometry_intersect_face_point_doc,
".. method:: intersect_face_point(face, point)\n"
"\n"
"   Tests if a point is inside a face (using the faces normal).\n"
"\n"
"   :arg face: The face to test.\n"
"   :type face: :class:`bmesh.types.BMFace`\n"
"   :arg point: The point to test.\n"
"   :type point: float triplet\n"
"   :return: True when the the point is in the face.\n"
"   :rtype: bool\n"
);
static PyObject *bpy_bm_geometry_intersect_face_point(BPy_BMFace *UNUSED(self), PyObject *args)
{
	BPy_BMFace *py_face;
	PyObject *py_point;
	float point[3];
	bool ret;

	if (!PyArg_ParseTuple(args,
	                      "O!O:intersect_face_point",
	                      &BPy_BMFace_Type, &py_face,
	                      &py_point))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_face);
	if (mathutils_array_parse(point, 3, 3, py_point, "intersect_face_point") == -1) {
		return NULL;
	}

	ret = BM_face_point_inside_test(py_face->f, point);

	return PyBool_FromLong(ret);
}


static struct PyMethodDef BPy_BM_geometry_methods[] = {
	{"intersect_face_point", (PyCFunction)bpy_bm_geometry_intersect_face_point, METH_VARARGS, bpy_bm_geometry_intersect_face_point_doc},
	{NULL, NULL, 0, NULL}
};


PyDoc_STRVAR(BPy_BM_utils_doc,
"This module provides access to bmesh geometry evaluation functions."
);
static struct PyModuleDef BPy_BM_geometry_module_def = {
	PyModuleDef_HEAD_INIT,
	"bmesh.geometry",  /* m_name */
	BPy_BM_utils_doc,  /* m_doc */
	0,  /* m_size */
	BPy_BM_geometry_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};


PyObject *BPyInit_bmesh_geometry(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BM_geometry_module_def);

	return submodule;
}
