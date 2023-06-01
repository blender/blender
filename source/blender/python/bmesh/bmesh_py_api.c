/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the 'bmesh' module.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_types_customdata.h"
#include "bmesh_py_types_meshdata.h"
#include "bmesh_py_types_select.h"

#include "bmesh_py_geometry.h"
#include "bmesh_py_ops.h"
#include "bmesh_py_utils.h"

#include "BKE_editmesh.h"

#include "DNA_mesh_types.h"

#include "../generic/py_capi_utils.h"

#include "bmesh_py_api.h" /* own include */

PyDoc_STRVAR(bpy_bm_new_doc,
             ".. method:: new(use_operators=True)\n"
             "\n"
             "   :arg use_operators: Support calling operators in :mod:`bmesh.ops` (uses some "
             "extra memory per vert/edge/face).\n"
             "   :type use_operators: bool\n"
             "   :return: Return a new, empty BMesh.\n"
             "   :rtype: :class:`bmesh.types.BMesh`\n");

static PyObject *bpy_bm_new(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"use_operators", NULL};
  BMesh *bm;

  bool use_operators = true;

  if (!PyArg_ParseTupleAndKeywords(
          args, kw, "|$O&:new", (char **)kwlist, PyC_ParseBool, &use_operators))
  {
    return NULL;
  }

  bm = BM_mesh_create(&bm_mesh_allocsize_default,
                      &((struct BMeshCreateParams){
                          .use_toolflags = use_operators,
                      }));

  return BPy_BMesh_CreatePyObject(bm, BPY_BMFLAG_NOP);
}

PyDoc_STRVAR(bpy_bm_from_edit_mesh_doc,
             ".. method:: from_edit_mesh(mesh)\n"
             "\n"
             "   Return a BMesh from this mesh, currently the mesh must already be in editmode.\n"
             "\n"
             "   :arg mesh: The editmode mesh.\n"
             "   :type mesh: :class:`bpy.types.Mesh`\n"
             "   :return: the BMesh associated with this mesh.\n"
             "   :rtype: :class:`bmesh.types.BMesh`\n");
static PyObject *bpy_bm_from_edit_mesh(PyObject *UNUSED(self), PyObject *value)
{
  BMesh *bm;
  Mesh *me = PyC_RNA_AsPointer(value, "Mesh");

  if (me == NULL) {
    return NULL;
  }

  if (me->edit_mesh == NULL) {
    PyErr_SetString(PyExc_ValueError, "The mesh must be in editmode");
    return NULL;
  }

  bm = me->edit_mesh->bm;

  return BPy_BMesh_CreatePyObject(bm, BPY_BMFLAG_IS_WRAPPED);
}

PyDoc_STRVAR(bpy_bm_update_edit_mesh_doc,
             ".. method:: update_edit_mesh(mesh, loop_triangles=True, destructive=True)\n"
             "\n"
             "   Update the mesh after changes to the BMesh in editmode,\n"
             "   optionally recalculating n-gon tessellation.\n"
             "\n"
             "   :arg mesh: The editmode mesh.\n"
             "   :type mesh: :class:`bpy.types.Mesh`\n"
             "   :arg loop_triangles: Option to recalculate n-gon tessellation.\n"
             "   :type loop_triangles: boolean\n"
             "   :arg destructive: Use when geometry has been added or removed.\n"
             "   :type destructive: boolean\n");
static PyObject *bpy_bm_update_edit_mesh(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"mesh", "loop_triangles", "destructive", NULL};
  PyObject *py_me;
  Mesh *me;
  bool do_loop_triangles = true;
  bool is_destructive = true;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "O|$O&O&:update_edit_mesh",
                                   (char **)kwlist,
                                   &py_me,
                                   PyC_ParseBool,
                                   &do_loop_triangles,
                                   PyC_ParseBool,
                                   &is_destructive))
  {
    return NULL;
  }

  me = PyC_RNA_AsPointer(py_me, "Mesh");

  if (me == NULL) {
    return NULL;
  }

  if (me->edit_mesh == NULL) {
    PyErr_SetString(PyExc_ValueError, "The mesh must be in editmode");
    return NULL;
  }

  {
    extern void EDBM_update_extern(
        struct Mesh * me, const bool do_tessface, const bool is_destructive);

    EDBM_update_extern(me, do_loop_triangles, is_destructive);
  }

  Py_RETURN_NONE;
}

static struct PyMethodDef BPy_BM_methods[] = {
    {"new", (PyCFunction)bpy_bm_new, METH_VARARGS | METH_KEYWORDS, bpy_bm_new_doc},
    {"from_edit_mesh", (PyCFunction)bpy_bm_from_edit_mesh, METH_O, bpy_bm_from_edit_mesh_doc},
    {"update_edit_mesh",
     (PyCFunction)bpy_bm_update_edit_mesh,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bm_update_edit_mesh_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(BPy_BM_doc,
             "This module provides access to blenders bmesh data structures.\n"
             "\n"
             ".. include:: include__bmesh.rst\n");
static struct PyModuleDef BPy_BM_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh",
    /*m_doc*/ BPy_BM_doc,
    /*m_size*/ 0,
    /*m_methods*/ BPy_BM_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *BPyInit_bmesh(void)
{
  PyObject *mod;
  PyObject *submodule;
  PyObject *sys_modules = PyImport_GetModuleDict();

  BPy_BM_init_types();
  BPy_BM_init_types_select();
  BPy_BM_init_types_customdata();
  BPy_BM_init_types_meshdata();

  mod = PyModule_Create(&BPy_BM_module_def);

  /* bmesh.types */
  PyModule_AddObject(mod, "types", (submodule = BPyInit_bmesh_types()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  /* bmesh.ops (not a real module, exposes module like access). */
  PyModule_AddObject(mod, "ops", (submodule = BPyInit_bmesh_ops()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "utils", (submodule = BPyInit_bmesh_utils()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "geometry", (submodule = BPyInit_bmesh_geometry()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  return mod;
}
