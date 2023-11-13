/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DDouble_Type;

#define BPy_UnaryFunction0DDouble_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DDouble_Type))

/*---------------------------Python BPy_UnaryFunction0DDouble structure definition----------*/
typedef struct {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<double> *uf0D_double;
} BPy_UnaryFunction0DDouble;

/*---------------------------Python BPy_UnaryFunction0DDouble visible prototypes-----------*/
int UnaryFunction0DDouble_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
