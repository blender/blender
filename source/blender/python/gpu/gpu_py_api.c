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
 * Experimental Python API, not considered public yet (called '_gpu'),
 * we may re-expose as public later.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "../generic/python_utildefines.h"

#include "GPU_init_exit.h"

#include "gpu_py_matrix.h"
#include "gpu_py_select.h"
#include "gpu_py_state.h"
#include "gpu_py_types.h"

#include "gpu_py_api.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Utils to invalidate functions
 * \{ */

bool bpygpu_is_init_or_error(void)
{
  if (!GPU_is_init()) {
    PyErr_SetString(PyExc_SystemError,
                    "GPU functions for drawing are not available in background mode");

    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Module
 * \{ */

PyDoc_STRVAR(pygpu_doc,
             "This module provides Python wrappers for the GPU implementation in Blender.\n"
             "Some higher level functions can be found in the `gpu_extras` module.");
static struct PyModuleDef pygpu_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu",
    .m_doc = pygpu_doc,
};

PyObject *BPyInit_gpu(void)
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *submodule;
  PyObject *mod;

  mod = PyModule_Create(&pygpu_module_def);

  PyModule_AddObject(mod, "types", (submodule = bpygpu_types_init()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "matrix", (submodule = bpygpu_matrix_init()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "select", (submodule = bpygpu_select_init()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "shader", (submodule = bpygpu_shader_init()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "state", (submodule = bpygpu_state_init()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  return mod;
}

/** \} */
