/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include <Python.h>

#include "BLI_compiler_attrs.h"

namespace blender {

namespace gpu {
class VertBuf;
}

extern PyTypeObject BPyGPUVertBuf_Type;

#define BPyGPUVertBuf_Check(v) (Py_TYPE(v) == &BPyGPUVertBuf_Type)

struct BPyGPUVertBuf {
  PyObject_VAR_HEAD
  /* The buf is owned, we may support thin wrapped batches later. */
  gpu::VertBuf *buf;
};

[[nodiscard]] PyObject *BPyGPUVertBuf_CreatePyObject(gpu::VertBuf *buf) ATTR_NONNULL(1);

}  // namespace blender
