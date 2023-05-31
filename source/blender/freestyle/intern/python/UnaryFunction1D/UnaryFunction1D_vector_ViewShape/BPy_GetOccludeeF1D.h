/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

extern PyTypeObject GetOccludeeF1D_Type;

#define BPy_GetOccludeeF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetOccludeeF1D_Type))

/*---------------------------Python BPy_GetOccludeeF1D structure definition----------*/
typedef struct {
  BPy_UnaryFunction1DVectorViewShape py_uf1D_vectorviewshape;
} BPy_GetOccludeeF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
