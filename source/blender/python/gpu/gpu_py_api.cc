/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * Experimental Python API, not considered public yet (called '_gpu'),
 * we may re-expose as public later.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "../generic/py_capi_utils.hh"

#include "gpu_py_capabilities.hh"
#include "gpu_py_compute.hh"
#include "gpu_py_matrix.hh"
#include "gpu_py_platform.hh"
#include "gpu_py_select.hh"
#include "gpu_py_state.hh"
#include "gpu_py_types.hh"

#include "BKE_global.hh"
#include "GPU_context.hh"
#include "GPU_init_exit.hh"
#include "WM_api.hh"
#include "gpu_py_api.hh" /* Own include. */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name GPU Module
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_init_doc,
    ".. function:: init()\n"
    "\n"
    "   Initializes the GPU module for background use.\n"
    "   If the initialization fails, a SystemError will be raised.\n");

static PyObject *pygpu_init(PyObject * /*self*/)
{
  if (!G.background || GPU_is_init()) {
    /* GPU is already initialized.*/
    Py_RETURN_NONE;
  }

  if (!GPU_backend_supported()) {
    PyErr_SetString(PyExc_SystemError, "Failed to initialize GPU. GPU backend not supported");
    return nullptr;
  }

  /* Cannot use GPU_init() as it requires a GPU context to have been created.
   * See WM_init_gpu implementation. */
  WM_init_gpu();

  if (!GPU_is_init()) {
    PyErr_SetString(PyExc_SystemError, "Failed to initialize GPU. Unexpected Error");
    return nullptr;
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

static PyMethodDef pygpu_tp_methods[] = {
    {"init", reinterpret_cast<PyCFunction>(pygpu_init), METH_NOARGS, pygpu_init_doc},
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
    pygpu_doc,
    "This module provides Python wrappers for the GPU implementation in Blender.\n"
    "Some higher level functions can be found in the :mod:`gpu_extras` module.\n");
static PyModuleDef pygpu_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu",
    /*m_doc*/ pygpu_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_tp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_gpu()
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *submodule;
  PyObject *mod;

  mod = PyModule_Create(&pygpu_module_def);

  PyModule_AddObject(mod, "types", (submodule = bpygpu_types_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "capabilities", (submodule = bpygpu_capabilities_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "matrix", (submodule = bpygpu_matrix_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "platform", (submodule = bpygpu_platform_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "select", (submodule = bpygpu_select_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "shader", (submodule = bpygpu_shader_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "state", (submodule = bpygpu_state_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "texture", (submodule = bpygpu_texture_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  PyModule_AddObject(mod, "compute", (submodule = bpygpu_compute_init()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  return mod;
}

/** \} */

}  // namespace blender
