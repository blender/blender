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
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_platform.h"

#include "gpu_py_platform.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Functions
 * \{ */

PyDoc_STRVAR(pygpu_platform_vendor_get_doc,
             ".. function:: vendor_get()\n"
             "\n"
             "   Get GPU vendor.\n"
             "\n"
             "   :return: Vendor name.\n"
             "   :rtype: str\n");
static PyObject *pygpu_platform_vendor_get(PyObject *UNUSED(self))
{
  return PyUnicode_FromString(GPU_platform_vendor());
}

PyDoc_STRVAR(pygpu_platform_renderer_get_doc,
             ".. function:: renderer_get()\n"
             "\n"
             "   Get GPU to be used for rendering.\n"
             "\n"
             "   :return: GPU name.\n"
             "   :rtype: str\n");
static PyObject *pygpu_platform_renderer_get(PyObject *UNUSED(self))
{
  return PyUnicode_FromString(GPU_platform_renderer());
}

PyDoc_STRVAR(pygpu_platform_version_get_doc,
             ".. function:: version_get()\n"
             "\n"
             "   Get GPU driver version.\n"
             "\n"
             "   :return: Driver version.\n"
             "   :rtype: str\n");
static PyObject *pygpu_platform_version_get(PyObject *UNUSED(self))
{
  return PyUnicode_FromString(GPU_platform_version());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static struct PyMethodDef pygpu_platform__tp_methods[] = {
    {"vendor_get",
     (PyCFunction)pygpu_platform_vendor_get,
     METH_NOARGS,
     pygpu_platform_vendor_get_doc},
    {"renderer_get",
     (PyCFunction)pygpu_platform_renderer_get,
     METH_NOARGS,
     pygpu_platform_renderer_get_doc},
    {"version_get",
     (PyCFunction)pygpu_platform_version_get,
     METH_NOARGS,
     pygpu_platform_version_get_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(pygpu_platform__tp_doc, "This module provides access to GPU Platform definitions.");
static PyModuleDef pygpu_platform_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.platform",
    .m_doc = pygpu_platform__tp_doc,
    .m_methods = pygpu_platform__tp_methods,
};

PyObject *bpygpu_platform_init(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_platform_module_def);

  return submodule;
}

/** \} */
