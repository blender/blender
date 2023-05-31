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

#include "../stroke/Predicates1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject BinaryPredicate1D_Type;

#define BPy_BinaryPredicate1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&BinaryPredicate1D_Type))

/*---------------------------Python BPy_BinaryPredicate1D structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::BinaryPredicate1D *bp1D;
} BPy_BinaryPredicate1D;

/*---------------------------Python BPy_BinaryPredicate1D visible prototypes-----------*/

int BinaryPredicate1D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
