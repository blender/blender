/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0D_Type;

#define BPy_UnaryFunction0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0D_Type))

/*---------------------------Python BPy_UnaryFunction0D structure definition----------*/
typedef struct {
  PyObject_HEAD
  PyObject *py_uf0D;
} BPy_UnaryFunction0D;

/*---------------------------Python BPy_UnaryFunction0D visible prototypes-----------*/

int UnaryFunction0D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
