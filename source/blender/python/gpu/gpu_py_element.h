/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject BPyGPUIndexBuf_Type;

#define BPyGPUIndexBuf_Check(v) (Py_TYPE(v) == &BPyGPUIndexBuf_Type)

typedef struct BPyGPUIndexBuf {
  PyObject_VAR_HEAD
  struct GPUIndexBuf *elem;
} BPyGPUIndexBuf;

PyObject *BPyGPUIndexBuf_CreatePyObject(struct GPUIndexBuf *elem);

#ifdef __cplusplus
}
#endif
