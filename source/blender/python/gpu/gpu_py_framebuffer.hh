/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include <Python.h>

#include "BLI_compiler_attrs.h"

namespace blender::gpu {
class FrameBuffer;
}  // namespace blender::gpu

extern PyTypeObject BPyGPUFrameBuffer_Type;

#define BPyGPUFrameBuffer_Check(v) (Py_TYPE(v) == &BPyGPUFrameBuffer_Type)

struct BPyGPUFrameBuffer {
  PyObject_HEAD
  blender::gpu::FrameBuffer *fb;

#ifndef GPU_NO_USE_PY_REFERENCES
  bool shared_reference;
#endif
};

[[nodiscard]] PyObject *BPyGPUFrameBuffer_CreatePyObject(blender::gpu::FrameBuffer *fb,
                                                         bool shared_reference) ATTR_NONNULL(1);
