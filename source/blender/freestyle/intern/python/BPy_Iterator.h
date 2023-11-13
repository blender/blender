/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../system/Iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Iterator_Type;

#define BPy_Iterator_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&Iterator_Type))

/*---------------------------Python BPy_Iterator structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::Iterator *it;
} BPy_Iterator;

/*---------------------------Python BPy_Iterator visible prototypes-----------*/

int Iterator_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
