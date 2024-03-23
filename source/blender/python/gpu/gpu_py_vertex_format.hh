/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#include "GPU_vertex_format.hh"

extern PyTypeObject BPyGPUVertFormat_Type;

#define BPyGPUVertFormat_Check(v) (Py_TYPE(v) == &BPyGPUVertFormat_Type)

struct BPyGPUVertFormat {
  PyObject_VAR_HEAD
  GPUVertFormat fmt;
};

PyObject *BPyGPUVertFormat_CreatePyObject(GPUVertFormat *fmt);
