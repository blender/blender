/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct GPUOffScreen;
struct GPUViewport;

extern PyTypeObject BPyGPUOffScreen_Type;

#define BPyGPUOffScreen_Check(v) (Py_TYPE(v) == &BPyGPUOffScreen_Type)

struct BPyGPUOffScreen {
  PyObject_HEAD
  GPUOffScreen *ofs;
  GPUViewport *viewport;
};

PyObject *BPyGPUOffScreen_CreatePyObject(GPUOffScreen *ofs) ATTR_NONNULL(1);
