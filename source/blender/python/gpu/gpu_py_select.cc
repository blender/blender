/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the gpu.select API.
 *
 * \note Currently only used for gizmo selection,
 * will need to add begin/end and a way to access the hits.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.h"

#include "GPU_select.h"

#include "gpu_py.h"
#include "gpu_py_select.h" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Methods
 * \{ */

PyDoc_STRVAR(pygpu_select_load_id_doc,
             ".. function:: load_id(id)\n"
             "\n"
             "   Set the selection ID.\n"
             "\n"
             "   :arg id: Number (32-bit uint).\n"
             "   :type select: int\n");
static PyObject *pygpu_select_load_id(PyObject * /*self*/, PyObject *value)
{
  uint id;
  if ((id = PyC_Long_AsU32(value)) == (uint)-1) {
    return nullptr;
  }
  GPU_select_load_id(id);
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static PyMethodDef pygpu_select__tp_methods[] = {
    /* Manage Stack */
    {"load_id", (PyCFunction)pygpu_select_load_id, METH_O, pygpu_select_load_id_doc},
    {nullptr, nullptr, 0, nullptr},
};

PyDoc_STRVAR(pygpu_select__tp_doc, "This module provides access to selection.");
static PyModuleDef pygpu_select_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.select",
    /*m_doc*/ pygpu_select__tp_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_select__tp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpygpu_select_init(void)
{
  PyObject *submodule;

  submodule = bpygpu_create_module(&pygpu_select_module_def);

  return submodule;
}

/** \} */
