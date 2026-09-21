/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include <Python.h>

#include "BLI_compiler_attrs.hh"

namespace blender {

namespace gpu {
class StorageBuf;
}  // namespace gpu

extern PyTypeObject BPyGPUStorageBuf_Type;

#define BPyGPUStorageBuf_Check(v) (Py_TYPE(v) == &BPyGPUStorageBuf_Type)

struct BPyGPUStorageBuf {
  PyObject_HEAD
  gpu::StorageBuf *ssbo;
  /** Size in bytes, needed since #gpu::StorageBuf doesn't expose it publicly. */
  size_t size;
};

[[nodiscard]] PyObject *BPyGPUStorageBuf_CreatePyObject(gpu::StorageBuf *ssbo, size_t size)
    ATTR_NONNULL(1);

}  // namespace blender
