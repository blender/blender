/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

PyObject *BPyInit_bgl(void);

/**
 * Create a buffer object
 *
 * \param dimensions: An array of ndimensions integers representing the size of each dimension.
 * \param initbuffer: When not NULL holds a contiguous buffer
 * with the correct format from which the buffer will be initialized
 */
struct _Buffer *BGL_MakeBuffer(int type, int ndimensions, int *dimensions, void *initbuffer);

int BGL_typeSize(int type);

/**
 * Buffer Object
 *
 * For Python access to OpenGL functions requiring a pointer.
 */
typedef struct _Buffer {
  PyObject_VAR_HEAD
  PyObject *parent;

  int type; /* GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT */
  int ndimensions;
  int *dimensions;

  union {
    char *asbyte;
    short *asshort;
    int *asint;
    float *asfloat;
    double *asdouble;

    void *asvoid;
  } buf;
} Buffer;

/** The type object. */
extern PyTypeObject BGL_bufferType;

#ifdef __cplusplus
}
#endif
