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

#include "gpu_py_types.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPU Types Module
 * \{ */

static PyModuleDef pygpu_types_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.types",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpygpu_types_init()
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_types_module_def);

  if (PyType_Ready(&BPyGPU_BufferType) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUVertFormat_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUVertBuf_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUIndexBuf_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUBatch_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUOffScreen_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUShader_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUTexture_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUFrameBuffer_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUUniformBuf_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUShaderCreateInfo_Type) < 0) {
    return nullptr;
  }
  if (PyType_Ready(&BPyGPUStageInterfaceInfo_Type) < 0) {
    return nullptr;
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
  PyModule_AddType(submodule, &BPyGPUShaderCreateInfo_Type);
  PyModule_AddType(submodule, &BPyGPUStageInterfaceInfo_Type);

  return submodule;
}

/** \} */
