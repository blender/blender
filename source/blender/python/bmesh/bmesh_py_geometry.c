/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the 'bmesh.geometry' module.
 * Utility functions for operating on 'bmesh.types'
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"
#include "bmesh_py_geometry.h" /* own include */
#include "bmesh_py_types.h"

PyDoc_STRVAR(bpy_bm_geometry_intersect_face_point_doc,
             ".. method:: intersect_face_point(face, point)\n"
             "\n"
             "   Tests if the projection of a point is inside a face (using the face's normal).\n"
             "\n"
             "   :arg face: The face to test.\n"
             "   :type face: :class:`bmesh.types.BMFace`\n"
             "   :arg point: The point to test.\n"
             "   :type point: float triplet\n"
             "   :return: True when the projection of the point is in the face.\n"
             "   :rtype: bool\n");
static PyObject *bpy_bm_geometry_intersect_face_point(BPy_BMFace *UNUSED(self), PyObject *args)
{
  BPy_BMFace *py_face;
  PyObject *py_point;
  float point[3];
  bool ret;

  if (!PyArg_ParseTuple(args, "O!O:intersect_face_point", &BPy_BMFace_Type, &py_face, &py_point)) {
    return NULL;
  }

  BPY_BM_CHECK_OBJ(py_face);
  if (mathutils_array_parse(point, 3, 3, py_point, "intersect_face_point") == -1) {
    return NULL;
  }

  ret = BM_face_point_inside_test(py_face->f, point);

  return PyBool_FromLong(ret);
}

static PyMethodDef BPy_BM_geometry_methods[] = {
    {"intersect_face_point",
     (PyCFunction)bpy_bm_geometry_intersect_face_point,
     METH_VARARGS,
     bpy_bm_geometry_intersect_face_point_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(BPy_BM_utils_doc,
             "This module provides access to bmesh geometry evaluation functions.");
static PyModuleDef BPy_BM_geometry_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh.geometry",
    /*m_doc*/ BPy_BM_utils_doc,
    /*m_size*/ 0,
    /*m_methods*/ BPy_BM_geometry_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *BPyInit_bmesh_geometry(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPy_BM_geometry_module_def);

  return submodule;
}
