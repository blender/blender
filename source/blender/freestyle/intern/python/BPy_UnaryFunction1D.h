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

#include "../view_map/Functions1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1D_Type;

#define BPy_UnaryFunction1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1D_Type))

/*---------------------------Python BPy_UnaryFunction1D structure definition----------*/
typedef struct {
  PyObject_HEAD
  PyObject *py_uf1D;
} BPy_UnaryFunction1D;

/*---------------------------Python BPy_UnaryFunction1D visible prototypes-----------*/

int UnaryFunction1D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
