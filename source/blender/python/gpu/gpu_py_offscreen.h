/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject BPyGPUOffScreen_Type;

#define BPyGPUOffScreen_Check(v) (Py_TYPE(v) == &BPyGPUOffScreen_Type)

struct GPUViewport;

typedef struct BPyGPUOffScreen {
  PyObject_HEAD
  struct GPUOffScreen *ofs;
  struct GPUViewport *viewport;
} BPyGPUOffScreen;

PyObject *BPyGPUOffScreen_CreatePyObject(struct GPUOffScreen *ofs) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
