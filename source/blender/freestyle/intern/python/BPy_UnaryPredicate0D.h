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

#include "../stroke/Predicates0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryPredicate0D_Type;

#define BPy_UnaryPredicate0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryPredicate0D_Type))

/*---------------------------Python BPy_UnaryPredicate0D structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::UnaryPredicate0D *up0D;
} BPy_UnaryPredicate0D;

/*---------------------------Python BPy_UnaryPredicate0D visible prototypes-----------*/

int UnaryPredicate0D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
