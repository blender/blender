/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DId.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ShapeIdF0D_Type;

#define BPy_ShapeIdF0D_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&ShapeIdF0D_Type))

/*---------------------------Python BPy_ShapeIdF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DId py_uf0D_id;
} BPy_ShapeIdF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
