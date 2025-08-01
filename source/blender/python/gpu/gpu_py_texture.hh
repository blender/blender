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
class Texture;
}

extern PyTypeObject BPyGPUTexture_Type;

/**
 * GPU_DEPTH24_STENCIL8 and GPU_DEPTH_COMPONENT24 are deprecated in Blender 5.0. These formats are
 * automatically converted to their 32F variant.
 */
constexpr int GPU_DEPTH24_STENCIL8_DEPRECATED = -1;
constexpr int GPU_DEPTH_COMPONENT24_DEPRECATED = -2;
extern const struct PyC_StringEnumItems pygpu_textureformat_items[];

#define BPyGPUTexture_Check(v) (Py_TYPE(v) == &BPyGPUTexture_Type)

struct BPyGPUTexture {
  PyObject_HEAD
  blender::gpu::Texture *tex;
};

[[nodiscard]] int bpygpu_ParseTexture(PyObject *o, void *p);
[[nodiscard]] PyObject *bpygpu_texture_init();

[[nodiscard]] PyObject *BPyGPUTexture_CreatePyObject(blender::gpu::Texture *tex,
                                                     bool shared_reference) ATTR_NONNULL(1);
