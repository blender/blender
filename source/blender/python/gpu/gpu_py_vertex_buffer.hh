/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

namespace blender::gpu {
class VertBuf;
}

extern PyTypeObject BPyGPUVertBuf_Type;

#define BPyGPUVertBuf_Check(v) (Py_TYPE(v) == &BPyGPUVertBuf_Type)

struct BPyGPUVertBuf {
  PyObject_VAR_HEAD
  /* The buf is owned, we may support thin wrapped batches later. */
  blender::gpu::VertBuf *buf;
};

PyObject *BPyGPUVertBuf_CreatePyObject(blender::gpu::VertBuf *buf) ATTR_NONNULL(1);
