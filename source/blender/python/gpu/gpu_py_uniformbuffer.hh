/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

extern PyTypeObject BPyGPUUniformBuf_Type;

#define BPyGPUUniformBuf_Check(v) (Py_TYPE(v) == &BPyGPUUniformBuf_Type)

typedef struct BPyGPUUniformBuf {
  PyObject_HEAD
  struct GPUUniformBuf *ubo;
} BPyGPUUniformBuf;

PyObject *BPyGPUUniformBuf_CreatePyObject(struct GPUUniformBuf *ubo) ATTR_NONNULL(1);
