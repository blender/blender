/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DDouble.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetSteerableViewMapDensityF1D_Type;

#define BPy_GetSteerableViewMapDensityF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetSteerableViewMapDensityF1D_Type))
/*---------------------------Python BPy_GetSteerableViewMapDensityF1D structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction1DDouble py_uf1D_double;
} BPy_GetSteerableViewMapDensityF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
