/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

extern PyTypeObject BPyGPUFrameBuffer_Type;

#define BPyGPUFrameBuffer_Check(v) (Py_TYPE(v) == &BPyGPUFrameBuffer_Type)

typedef struct BPyGPUFrameBuffer {
  PyObject_HEAD
  struct GPUFrameBuffer *fb;

#ifndef GPU_NO_USE_PY_REFERENCES
  bool shared_reference;
#endif
} BPyGPUFrameBuffer;

PyObject *BPyGPUFrameBuffer_CreatePyObject(struct GPUFrameBuffer *fb, bool shared_reference)
    ATTR_NONNULL(1);
