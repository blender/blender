/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryPredicate0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FalseUP0D_Type;

#define BPy_FalseUP0D_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&FalseUP0D_Type))

/*---------------------------Python BPy_FalseUP0D structure definition----------*/
typedef struct {
  BPy_UnaryPredicate0D py_up0D;
} BPy_FalseUP0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
