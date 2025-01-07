/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#pragma once

/**
 * Buffer Object
 *
 * For Python access to OpenGL functions requiring a pointer.
 */
struct Buffer {
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
};

/** The type object. */
extern PyTypeObject BGL_bufferType;

PyObject *BPyInit_bgl();

/**
 * Create a buffer object
 *
 * \param dimensions: An array of ndimensions integers representing the size of each dimension.
 * \param initbuffer: When not NULL holds a contiguous buffer
 * with the correct format from which the buffer will be initialized
 */
struct Buffer *BGL_MakeBuffer(int type,
                              int ndimensions,
                              const int *dimensions,
                              const void *initbuffer);

int BGL_typeSize(int type);
