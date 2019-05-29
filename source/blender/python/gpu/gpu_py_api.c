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
#include "GPU_primitive.h"

#include "gpu_py_matrix.h"
#include "gpu_py_select.h"
#include "gpu_py_types.h"

#include "gpu_py_api.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Utils to invalidate functions
 * \{ */

bool bpygpu_is_initialized_or_error(void)
{
  if (!GPU_is_initialized()) {
    PyErr_SetString(PyExc_SystemError,
                    "GPU functions for drawing are not available in background mode");

    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Primitive Type Utils
 * \{ */

int bpygpu_ParsePrimType(PyObject *o, void *p)
{
  Py_ssize_t mode_id_len;
  const char *mode_id = _PyUnicode_AsStringAndSize(o, &mode_id_len);
  if (mode_id == NULL) {
    PyErr_Format(PyExc_ValueError, "expected a string, got %s", Py_TYPE(o)->tp_name);
    return 0;
  }
#define MATCH_ID(id) \
  if (mode_id_len == strlen(STRINGIFY(id))) { \
    if (STREQ(mode_id, STRINGIFY(id))) { \
      mode = GPU_PRIM_##id; \
      goto success; \
    } \
  } \
  ((void)0)

  GPUPrimType mode;
  MATCH_ID(POINTS);
  MATCH_ID(LINES);
  MATCH_ID(TRIS);
  MATCH_ID(LINE_STRIP);
  MATCH_ID(LINE_LOOP);
  MATCH_ID(TRI_STRIP);
  MATCH_ID(TRI_FAN);
  MATCH_ID(LINES_ADJ);
  MATCH_ID(TRIS_ADJ);
  MATCH_ID(LINE_STRIP_ADJ);

#undef MATCH_ID
  PyErr_Format(PyExc_ValueError, "unknown type literal: '%s'", mode_id);
  return 0;

success:
  (*(GPUPrimType *)p) = mode;
  return 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Module
 * \{ */

PyDoc_STRVAR(GPU_doc,
             "This module provides Python wrappers for the GPU implementation in Blender. "
             "Some higher level functions can be found in the `gpu_extras` module. "
             "\n\n"
             "Submodules:\n"
             "\n"
             ".. toctree::\n"
             "   :maxdepth: 1\n"
             "\n"
             "   gpu.types.rst\n"
             "   gpu.shader.rst\n"
             "   gpu.matrix.rst\n"
             "   gpu.select.rst\n"
             "\n");
static struct PyModuleDef GPU_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu",
    .m_doc = GPU_doc,
};

PyObject *BPyInit_gpu(void)
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *submodule;
  PyObject *mod;

  mod = PyModule_Create(&GPU_module_def);

  PyModule_AddObject(mod, "types", (submodule = BPyInit_gpu_types()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "matrix", (submodule = BPyInit_gpu_matrix()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "select", (submodule = BPyInit_gpu_select()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "shader", (submodule = BPyInit_gpu_shader()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  return mod;
}

/** \} */
