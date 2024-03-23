/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#ifndef __cplusplus
#  include "../generic/py_capi_utils.h"
#endif

struct GPUShaderCreateInfo;
struct GPUStageInterfaceInfo;

/* Make sure that there is always a reference count for PyObjects of type String as the strings are
 * passed by reference in the #GPUStageInterfaceInfo and #GPUShaderCreateInfo APIs. */
#define USE_GPU_PY_REFERENCES

/* `gpu_py_shader.cc` */

extern PyTypeObject BPyGPUShader_Type;

#define BPyGPUShader_Check(v) (Py_TYPE(v) == &BPyGPUShader_Type)

typedef struct BPyGPUShader {
  PyObject_VAR_HEAD
  struct GPUShader *shader;
  bool is_builtin;
} BPyGPUShader;

PyObject *BPyGPUShader_CreatePyObject(struct GPUShader *shader, bool is_builtin);
PyObject *bpygpu_shader_init(void);

/* gpu_py_shader_create_info.cc */

extern const struct PyC_StringEnumItems pygpu_attrtype_items[];
extern PyTypeObject BPyGPUShaderCreateInfo_Type;
extern PyTypeObject BPyGPUStageInterfaceInfo_Type;

#define BPyGPUShaderCreateInfo_Check(v) (Py_TYPE(v) == &BPyGPUShaderCreateInfo_Type)
#define BPyGPUStageInterfaceInfo_Check(v) (Py_TYPE(v) == &BPyGPUStageInterfaceInfo_Type)

struct BPyGPUStageInterfaceInfo {
  PyObject_VAR_HEAD
  GPUStageInterfaceInfo *interface;
#ifdef USE_GPU_PY_REFERENCES
  /* Just to keep a user to prevent freeing buf's we're using. */
  PyObject *references;
#endif
};

struct BPyGPUShaderCreateInfo {
  PyObject_VAR_HEAD
  GPUShaderCreateInfo *info;
#ifdef USE_GPU_PY_REFERENCES
  /* Just to keep a user to prevent freeing buf's we're using. */
  PyObject *vertex_source;
  PyObject *fragment_source;
  PyObject *compute_source;
  PyObject *typedef_source;
  PyObject *references;
#endif
  size_t constants_total_size;
};

PyObject *BPyGPUStageInterfaceInfo_CreatePyObject(GPUStageInterfaceInfo *interface);
PyObject *BPyGPUShaderCreateInfo_CreatePyObject(GPUShaderCreateInfo *info);
