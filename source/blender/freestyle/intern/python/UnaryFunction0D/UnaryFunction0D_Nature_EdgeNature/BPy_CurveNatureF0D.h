/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DEdgeNature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject CurveNatureF0D_Type;

#define BPy_CurveNatureF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&CurveNatureF0D_Type))

/*---------------------------Python BPy_CurveNatureF0D structure definition----------*/
typedef struct {
  BPy_UnaryFunction0DEdgeNature py_uf0D_edgenature;
} BPy_CurveNatureF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
