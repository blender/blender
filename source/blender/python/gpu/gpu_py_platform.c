/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_context.h"
#include "GPU_platform.h"

#include "gpu_py.h"
#include "gpu_py_platform.h" /* Own include. */

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

PyDoc_STRVAR(
    pygpu_platform_device_type_get_doc,
    ".. function:: device_type_get()\n"
    "\n"
    "   Get GPU device type.\n"
    "\n"
    "   :return: Device type ('APPLE', 'NVIDIA', 'AMD', 'INTEL', 'SOFTWARE', 'UNKNOWN').\n"
    "   :rtype: str\n");
static PyObject *pygpu_platform_device_type_get(PyObject *UNUSED(self))
{
  if (GPU_type_matches(GPU_DEVICE_APPLE, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    return PyUnicode_FromString("APPLE");
  }
  if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    return PyUnicode_FromString("NVIDIA");
  }
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    return PyUnicode_FromString("AMD");
  }
  if (GPU_type_matches(GPU_DEVICE_INTEL | GPU_DEVICE_INTEL_UHD, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    return PyUnicode_FromString("INTEL");
  }
  if (GPU_type_matches(GPU_DEVICE_SOFTWARE, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    return PyUnicode_FromString("SOFTWARE");
  }
  return PyUnicode_FromString("UNKNOWN");
}

PyDoc_STRVAR(pygpu_platform_backend_type_get_doc,
             ".. function:: backend_type_get()\n"
             "\n"
             "   Get actuve GPU backend.\n"
             "\n"
             "   :return: Backend type ('OPENGL', 'VULKAN', 'METAL', 'NONE', 'UNKNOWN').\n"
             "   :rtype: str\n");
static PyObject *pygpu_platform_backend_type_get(PyObject *UNUSED(self))
{
  switch (GPU_backend_get_type()) {
    case GPU_BACKEND_VULKAN:
      return PyUnicode_FromString("VULKAN");
    case GPU_BACKEND_METAL:
      return PyUnicode_FromString("METAL");
    case GPU_BACKEND_NONE:
      return PyUnicode_FromString("NONE");
    case GPU_BACKEND_OPENGL:
      return PyUnicode_FromString("OPENGL");
    case GPU_BACKEND_ANY:
      break;
  }
  return PyUnicode_FromString("UNKNOWN");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static PyMethodDef pygpu_platform__tp_methods[] = {
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
    {"device_type_get",
     (PyCFunction)pygpu_platform_device_type_get,
     METH_NOARGS,
     pygpu_platform_device_type_get_doc},
    {"backend_type_get",
     (PyCFunction)pygpu_platform_backend_type_get,
     METH_NOARGS,
     pygpu_platform_backend_type_get_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(pygpu_platform__tp_doc, "This module provides access to GPU Platform definitions.");
static PyModuleDef pygpu_platform_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.platform",
    /*m_doc*/ pygpu_platform__tp_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_platform__tp_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *bpygpu_platform_init(void)
{
  PyObject *submodule;

  submodule = bpygpu_create_module(&pygpu_platform_module_def);

  return submodule;
}

/** \} */
