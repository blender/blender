/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1DVoid_Type;

#define BPy_UnaryFunction1DVoid_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1DVoid_Type))

/*---------------------------Python BPy_UnaryFunction1DVoid structure definition----------*/
typedef struct {
  BPy_UnaryFunction1D py_uf1D;
  Freestyle::UnaryFunction1D_void *uf1D_void;
} BPy_UnaryFunction1DVoid;

/*---------------------------Python BPy_UnaryFunction1DVoid visible prototypes-----------*/
int UnaryFunction1DVoid_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
