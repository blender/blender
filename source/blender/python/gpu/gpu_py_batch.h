/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USE_GPU_PY_REFERENCES

extern PyTypeObject BPyGPUBatch_Type;

#define BPyGPUBatch_Check(v) (Py_TYPE(v) == &BPyGPUBatch_Type)

typedef struct BPyGPUBatch {
  PyObject_VAR_HEAD
  /* The batch is owned, we may support thin wrapped batches later. */
  struct GPUBatch *batch;
#ifdef USE_GPU_PY_REFERENCES
  /* Just to keep a user to prevent freeing buf's we're using */
  PyObject *references;
#endif
} BPyGPUBatch;

PyObject *BPyGPUBatch_CreatePyObject(struct GPUBatch *batch) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
