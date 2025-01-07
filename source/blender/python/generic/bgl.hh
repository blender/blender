/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#pragma once

PyObject *BPyInit_bgl();

/* This API is deprecated, currently these are only used in `bgl.cc`
 * and there should be no reason to make use of them in the future.
 * Use a define to indicate they are part of the public API which is being phased out. */
#ifdef USE_BGL_DEPRECATED_API

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

#endif /* USE_BGL_DEPRECATED_API */
