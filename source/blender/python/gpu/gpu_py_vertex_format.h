/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "GPU_vertex_format.h"

extern PyTypeObject BPyGPUVertFormat_Type;

#define BPyGPUVertFormat_Check(v) (Py_TYPE(v) == &BPyGPUVertFormat_Type)

typedef struct BPyGPUVertFormat {
  PyObject_VAR_HEAD
  struct GPUVertFormat fmt;
} BPyGPUVertFormat;

PyObject *BPyGPUVertFormat_CreatePyObject(struct GPUVertFormat *fmt);
