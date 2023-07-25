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

extern PyTypeObject BPyGPUVertBuf_Type;

#define BPyGPUVertBuf_Check(v) (Py_TYPE(v) == &BPyGPUVertBuf_Type)

typedef struct BPyGPUVertBuf {
  PyObject_VAR_HEAD
  /* The buf is owned, we may support thin wrapped batches later. */
  struct GPUVertBuf *buf;
} BPyGPUVertBuf;

PyObject *BPyGPUVertBuf_CreatePyObject(struct GPUVertBuf *buf) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
