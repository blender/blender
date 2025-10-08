/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the 'bmesh' module.
 */

#include <Python.h>

#include "bmesh.hh"

#include "bmesh_py_types.hh"
#include "bmesh_py_types_customdata.hh"
#include "bmesh_py_types_meshdata.hh"
#include "bmesh_py_types_select.hh"

#include "bmesh_py_geometry.hh"
#include "bmesh_py_ops.hh"
#include "bmesh_py_utils.hh"

#include "BKE_editmesh.hh"
#include "BKE_mesh_types.hh"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "../generic/py_capi_utils.hh"

#include "bmesh_py_api.hh" /* own include */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_new_doc,
    ".. method:: new(*, use_operators=True)\n"
    "\n"
    "   :arg use_operators: Support calling operators in :mod:`bmesh.ops` (uses some "
    "extra memory per vert/edge/face).\n"
    "   :type use_operators: bool\n"
    "   :return: Return a new, empty BMesh.\n"
    "   :rtype: :class:`bmesh.types.BMesh`\n");
static PyObject *bpy_bm_new(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"use_operators", nullptr};
  BMesh *bm;

  bool use_operators = true;

  if (!PyArg_ParseTupleAndKeywords(
          args, kw, "|$O&:new", (char **)kwlist, PyC_ParseBool, &use_operators))
  {
    return nullptr;
  }

  BMeshCreateParams params{};
  params.use_toolflags = use_operators;
  bm = BM_mesh_create(&bm_mesh_allocsize_default, &params);
  bm->selectmode = SCE_SELECT_VERTEX;

  return BPy_BMesh_CreatePyObject(bm, BPY_BMFLAG_NOP);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_from_edit_mesh_doc,
    ".. method:: from_edit_mesh(mesh)\n"
    "\n"
    "   Return a BMesh from this mesh, currently the mesh must already be in editmode.\n"
    "\n"
    "   :arg mesh: The editmode mesh.\n"
    "   :type mesh: :class:`bpy.types.Mesh`\n"
    "   :return: the BMesh associated with this mesh.\n"
    "   :rtype: :class:`bmesh.types.BMesh`\n");
static PyObject *bpy_bm_from_edit_mesh(PyObject * /*self*/, PyObject *value)
{
  BMesh *bm;
  Mesh *mesh = static_cast<Mesh *>(PyC_RNA_AsPointer(value, "Mesh"));

  if (mesh == nullptr) {
    return nullptr;
  }

  if (mesh->runtime->edit_mesh == nullptr) {
    PyErr_SetString(PyExc_ValueError, "The mesh must be in editmode");
    return nullptr;
  }

  bm = mesh->runtime->edit_mesh->bm;

  return BPy_BMesh_CreatePyObject(bm, BPY_BMFLAG_IS_WRAPPED);
}

void EDBM_update_extern(Mesh *mesh, const bool do_tessface, const bool is_destructive);

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_update_edit_mesh_doc,
    ".. method:: update_edit_mesh(mesh, *, loop_triangles=True, destructive=True)\n"
    "\n"
    "   Update the mesh after changes to the BMesh in editmode,\n"
    "   optionally recalculating n-gon tessellation.\n"
    "\n"
    "   :arg mesh: The editmode mesh.\n"
    "   :type mesh: :class:`bpy.types.Mesh`\n"
    "   :arg loop_triangles: Option to recalculate n-gon tessellation.\n"
    "   :type loop_triangles: bool\n"
    "   :arg destructive: Use when geometry has been added or removed.\n"
    "   :type destructive: bool\n");
static PyObject *bpy_bm_update_edit_mesh(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"mesh", "loop_triangles", "destructive", nullptr};
  PyObject *py_me;
  Mesh *mesh;
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
    return nullptr;
  }

  mesh = static_cast<Mesh *>(PyC_RNA_AsPointer(py_me, "Mesh"));

  if (mesh == nullptr) {
    return nullptr;
  }

  if (mesh->runtime->edit_mesh == nullptr) {
    PyErr_SetString(PyExc_ValueError, "The mesh must be in editmode");
    return nullptr;
  }

  {
    EDBM_update_extern(mesh, do_loop_triangles, is_destructive);
  }

  Py_RETURN_NONE;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef BPy_BM_methods[] = {
    {"new", (PyCFunction)bpy_bm_new, METH_VARARGS | METH_KEYWORDS, bpy_bm_new_doc},
    {"from_edit_mesh", (PyCFunction)bpy_bm_from_edit_mesh, METH_O, bpy_bm_from_edit_mesh_doc},
    {"update_edit_mesh",
     (PyCFunction)bpy_bm_update_edit_mesh,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bm_update_edit_mesh_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    BPy_BM_doc,
    "This module provides access to blenders bmesh data structures.\n"
    "\n"
    ".. include:: include__bmesh.rst\n");
static PyModuleDef BPy_BM_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh",
    /*m_doc*/ BPy_BM_doc,
    /*m_size*/ 0,
    /*m_methods*/ BPy_BM_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_bmesh()
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
