/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "GPU_init_exit.h"
#include "GPU_primitive.h"
#include "GPU_texture.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPU Enums
 * \{ */

PyC_StringEnumItems bpygpu_primtype_items[] = {
    {GPU_PRIM_POINTS, "POINTS"},
    {GPU_PRIM_LINES, "LINES"},
    {GPU_PRIM_TRIS, "TRIS"},
    {GPU_PRIM_LINE_STRIP, "LINE_STRIP"},
    {GPU_PRIM_LINE_LOOP, "LINE_LOOP"},
    {GPU_PRIM_TRI_STRIP, "TRI_STRIP"},
    {GPU_PRIM_TRI_FAN, "TRI_FAN"},
    {GPU_PRIM_LINES_ADJ, "LINES_ADJ"},
    {GPU_PRIM_TRIS_ADJ, "TRIS_ADJ"},
    {GPU_PRIM_LINE_STRIP_ADJ, "LINE_STRIP_ADJ"},
    {0, nullptr},
};

PyC_StringEnumItems bpygpu_dataformat_items[] = {
    {GPU_DATA_FLOAT, "FLOAT"},
    {GPU_DATA_INT, "INT"},
    {GPU_DATA_UINT, "UINT"},
    {GPU_DATA_UBYTE, "UBYTE"},
    {GPU_DATA_UINT_24_8, "UINT_24_8"},
    {GPU_DATA_10_11_11_REV, "10_11_11_REV"},
    {0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static const char g_error[] = "GPU API is not available in background mode";

static PyObject *py_error__ml_meth(PyObject * /*self*/, PyObject * /*args*/)
{
  PyErr_SetString(PyExc_SystemError, g_error);
  return nullptr;
}

static PyObject *py_error__getter(PyObject * /*self*/, void * /*type*/)
{
  PyErr_SetString(PyExc_SystemError, g_error);
  return nullptr;
}

static int py_error__setter(PyObject * /*self*/, PyObject * /*value*/, void * /*type*/)
{
  PyErr_SetString(PyExc_SystemError, g_error);
  return -1;
}

static PyObject *py_error__tp_new(PyTypeObject * /*type*/,
                                  PyObject * /*args*/,
                                  PyObject * /*kwds*/)
{
  PyErr_SetString(PyExc_SystemError, g_error);
  return nullptr;
}

PyObject *bpygpu_create_module(PyModuleDef *module_type)
{
  if (!GPU_is_init() && module_type->m_methods) {
    /* Replace all methods with an error method.
     * That way when the method is called, an error will appear instead. */
    for (PyMethodDef *meth = module_type->m_methods; meth->ml_name; meth++) {
      meth->ml_meth = py_error__ml_meth;
    }
  }

  PyObject *module = PyModule_Create(module_type);

  return module;
}

int bpygpu_finalize_type(PyTypeObject *py_type)
{
  if (!GPU_is_init()) {
    if (py_type->tp_methods) {
      /* Replace all methods with an error method. */
      for (PyMethodDef *meth = py_type->tp_methods; meth->ml_name; meth++) {
        meth->ml_meth = py_error__ml_meth;
      }
    }
    if (py_type->tp_getset) {
      /* Replace all getters and setter with a functions that always returns error. */
      for (PyGetSetDef *getset = py_type->tp_getset; getset->name; getset++) {
        getset->get = py_error__getter;
        getset->set = py_error__setter;
      }
    }
    if (py_type->tp_new) {
      /* If initialized, return error. */
      py_type->tp_new = py_error__tp_new;
    }
  }

  return PyType_Ready(py_type);
}

/** \} */
