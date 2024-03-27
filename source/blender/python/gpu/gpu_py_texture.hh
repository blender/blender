/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct GPUTexture;

extern PyTypeObject BPyGPUTexture_Type;
extern const struct PyC_StringEnumItems pygpu_textureformat_items[];

#define BPyGPUTexture_Check(v) (Py_TYPE(v) == &BPyGPUTexture_Type)

struct BPyGPUTexture {
  PyObject_HEAD
  GPUTexture *tex;
};

int bpygpu_ParseTexture(PyObject *o, void *p);
PyObject *bpygpu_texture_init();

PyObject *BPyGPUTexture_CreatePyObject(GPUTexture *tex, bool shared_reference) ATTR_NONNULL(1);
