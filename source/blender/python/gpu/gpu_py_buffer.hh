/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 */

#pragma once

extern PyTypeObject BPyGPU_BufferType;

#define BPyGPU_Buffer_Check(v) (Py_TYPE(v) == &BPyGPU_BufferType)

/**
 * Buffer Object
 *
 * For Python access to GPU functions requiring a pointer.
 */
struct BPyGPUBuffer {
  PyObject_VAR_HEAD
  PyObject *parent;

  int format;
  int shape_len;
  Py_ssize_t *shape;

  union {
    char *as_byte;
    int *as_int;
    uint *as_uint;
    float *as_float;

    void *as_void;
  } buf;
};

size_t bpygpu_Buffer_size(BPyGPUBuffer *buffer);
/**
 * Create a buffer object
 *
 * \param shape: An array of `shape_len` integers representing the size of each dimension.
 * \param buffer: When not NULL holds a contiguous buffer
 * with the correct format from which the buffer will be initialized
 */
BPyGPUBuffer *BPyGPU_Buffer_CreatePyObject(int format,
                                           const Py_ssize_t *shape,
                                           int shape_len,
                                           void *buffer);
