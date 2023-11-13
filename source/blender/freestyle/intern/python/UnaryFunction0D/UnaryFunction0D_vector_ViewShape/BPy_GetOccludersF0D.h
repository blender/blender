/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DVectorViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetOccludersF0D_Type;

#define BPy_GetOccludersF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetOccludersF0D_Type))

/*---------------------------Python BPy_GetOccludersF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DVectorViewShape py_uf0D_vectorviewshape;
} BPy_GetOccludersF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
