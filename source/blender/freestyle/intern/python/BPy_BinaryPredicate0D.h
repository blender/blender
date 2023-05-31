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

#include "../stroke/Predicates0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject BinaryPredicate0D_Type;

#define BPy_BinaryPredicate0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&BinaryPredicate0D_Type))

/*---------------------------Python BPy_BinaryPredicate0D structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::BinaryPredicate0D *bp0D;
} BPy_BinaryPredicate0D;

/*---------------------------Python BPy_BinaryPredicate0D visible prototypes-----------*/

int BinaryPredicate0D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
