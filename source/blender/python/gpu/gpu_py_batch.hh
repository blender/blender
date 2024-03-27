/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

namespace blender::gpu {
class Batch;
}

#define USE_GPU_PY_REFERENCES

extern PyTypeObject BPyGPUBatch_Type;

#define BPyGPUBatch_Check(v) (Py_TYPE(v) == &BPyGPUBatch_Type)

struct BPyGPUBatch {
  PyObject_VAR_HEAD
  /* The batch is owned, we may support thin wrapped batches later. */
  blender::gpu::Batch *batch;
#ifdef USE_GPU_PY_REFERENCES
  /* Just to keep a user to prevent freeing buffers we're using. */
  PyObject *references;
#endif
};

PyObject *BPyGPUBatch_CreatePyObject(blender::gpu::Batch *batch) ATTR_NONNULL(1);
