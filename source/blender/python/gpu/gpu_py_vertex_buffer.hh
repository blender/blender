/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct GPUVertBuf;

extern PyTypeObject BPyGPUVertBuf_Type;

#define BPyGPUVertBuf_Check(v) (Py_TYPE(v) == &BPyGPUVertBuf_Type)

struct BPyGPUVertBuf {
  PyObject_VAR_HEAD
  /* The buf is owned, we may support thin wrapped batches later. */
  GPUVertBuf *buf;
};

PyObject *BPyGPUVertBuf_CreatePyObject(GPUVertBuf *buf) ATTR_NONNULL(1);
