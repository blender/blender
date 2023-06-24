/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DEdgeNature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject CurveNatureF1D_Type;

#define BPy_CurveNatureF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&CurveNatureF1D_Type))

/*---------------------------Python BPy_CurveNatureF1D structure definition----------*/
typedef struct {
  BPy_UnaryFunction1DEdgeNature py_uf1D_edgenature;
} BPy_CurveNatureF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
