/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct GPUFrameBuffer;

extern PyTypeObject BPyGPUFrameBuffer_Type;

#define BPyGPUFrameBuffer_Check(v) (Py_TYPE(v) == &BPyGPUFrameBuffer_Type)

struct BPyGPUFrameBuffer {
  PyObject_HEAD
  GPUFrameBuffer *fb;

#ifndef GPU_NO_USE_PY_REFERENCES
  bool shared_reference;
#endif
};

PyObject *BPyGPUFrameBuffer_CreatePyObject(GPUFrameBuffer *fb, bool shared_reference)
    ATTR_NONNULL(1);
