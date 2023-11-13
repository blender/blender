/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DVectorViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetShapeF1D_Type;

#define BPy_GetShapeF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetShapeF1D_Type))

/*---------------------------Python BPy_GetShapeF1D structure definition----------*/
typedef struct {
  BPy_UnaryFunction1DVectorViewShape py_uf1D_vectorviewshape;
} BPy_GetShapeF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
