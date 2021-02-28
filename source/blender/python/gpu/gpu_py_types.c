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
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_types.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPU Types Module
 * \{ */

static struct PyModuleDef pygpu_types_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.types",
};

PyObject *bpygpu_types_init(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_types_module_def);

  if (PyType_Ready(&BPyGPU_BufferType) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUVertFormat_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUVertBuf_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUIndexBuf_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUBatch_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUOffScreen_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUShader_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUTexture_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUFrameBuffer_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BPyGPUUniformBuf_Type) < 0) {
    return NULL;
  }

  PyModule_AddType(submodule, &BPyGPU_BufferType);
  PyModule_AddType(submodule, &BPyGPUVertFormat_Type);
  PyModule_AddType(submodule, &BPyGPUVertBuf_Type);
  PyModule_AddType(submodule, &BPyGPUIndexBuf_Type);
  PyModule_AddType(submodule, &BPyGPUBatch_Type);
  PyModule_AddType(submodule, &BPyGPUOffScreen_Type);
  PyModule_AddType(submodule, &BPyGPUShader_Type);
  PyModule_AddType(submodule, &BPyGPUTexture_Type);
  PyModule_AddType(submodule, &BPyGPUFrameBuffer_Type);
  PyModule_AddType(submodule, &BPyGPUUniformBuf_Type);

  return submodule;
}

/** \} */
