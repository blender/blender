/* SPDX-FileCopyrightText: 2023 Blender Authors
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

extern PyTypeObject TrueUP0D_Type;

#define BPy_TrueUP0D_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&TrueUP0D_Type))

/*---------------------------Python BPy_TrueUP0D structure definition----------*/
typedef struct {
  BPy_UnaryPredicate0D py_up0D;
} BPy_TrueUP0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
