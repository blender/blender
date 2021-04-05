/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
typedef struct BPyGPUBuffer {
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
} BPyGPUBuffer;

size_t bpygpu_Buffer_size(BPyGPUBuffer *buffer);
BPyGPUBuffer *BPyGPU_Buffer_CreatePyObject(const int format,
                                           const Py_ssize_t *shape,
                                           const int shape_len,
                                           void *buffer);
