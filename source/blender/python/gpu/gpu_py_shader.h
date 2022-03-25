/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

extern PyTypeObject BPyGPUShader_Type;

#define BPyGPUShader_Check(v) (Py_TYPE(v) == &BPyGPUShader_Type)

typedef struct BPyGPUShader {
  PyObject_VAR_HEAD
  struct GPUShader *shader;
  bool is_builtin;
} BPyGPUShader;

PyObject *BPyGPUShader_CreatePyObject(struct GPUShader *shader, bool is_builtin);
PyObject *bpygpu_shader_init(void);
