/*
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
 */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the gpu.select API.
 *
 * \note Currently only used for gizmo selection,
 * will need to add begin/end and a way to access the hits.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.h"

#include "GPU_select.h"

#include "gpu_py_select.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Methods
 * \{ */

PyDoc_STRVAR(bpygpu_select_load_id_doc,
             "load_id(id)\n"
             "\n"
             "   Set the selection ID.\n"
             "\n"
             "   :param id: Number (32-bit uint).\n"
             "   :type select: int\n");
static PyObject *bpygpu_select_load_id(PyObject *UNUSED(self), PyObject *value)
{
  uint id;
  if ((id = PyC_Long_AsU32(value)) == (uint)-1) {
    return NULL;
  }
  GPU_select_load_id(id);
  Py_RETURN_NONE;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static struct PyMethodDef bpygpu_select_methods[] = {
    /* Manage Stack */
    {"load_id", (PyCFunction)bpygpu_select_load_id, METH_O, bpygpu_select_load_id_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(bpygpu_select_doc, "This module provides access to selection.");
static PyModuleDef BPyGPU_select_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.select",
    .m_doc = bpygpu_select_doc,
    .m_methods = bpygpu_select_methods,
};

PyObject *BPyInit_gpu_select(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPyGPU_select_module_def);

  return submodule;
}

/** \} */
