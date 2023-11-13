/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetShapeF0D_Type;

#define BPy_GetShapeF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetShapeF0D_Type))

/*---------------------------Python BPy_GetShapeF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DViewShape py_uf0D_viewshape;
} BPy_GetShapeF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
