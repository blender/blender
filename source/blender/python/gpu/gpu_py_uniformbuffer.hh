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
class UniformBuf;
}  // namespace gpu

extern PyTypeObject BPyGPUUniformBuf_Type;

#define BPyGPUUniformBuf_Check(v) (Py_TYPE(v) == &BPyGPUUniformBuf_Type)

struct BPyGPUUniformBuf {
  PyObject_HEAD
  gpu::UniformBuf *ubo;
};

[[nodiscard]] PyObject *BPyGPUUniformBuf_CreatePyObject(gpu::UniformBuf *ubo) ATTR_NONNULL(1);

}  // namespace blender
