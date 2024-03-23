/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct GPUBatch;

#define USE_GPU_PY_REFERENCES

extern PyTypeObject BPyGPUBatch_Type;

#define BPyGPUBatch_Check(v) (Py_TYPE(v) == &BPyGPUBatch_Type)

struct BPyGPUBatch {
  PyObject_VAR_HEAD
  /* The batch is owned, we may support thin wrapped batches later. */
  GPUBatch *batch;
#ifdef USE_GPU_PY_REFERENCES
  /* Just to keep a user to prevent freeing buf's we're using */
  PyObject *references;
#endif
};

PyObject *BPyGPUBatch_CreatePyObject(GPUBatch *batch) ATTR_NONNULL(1);
