/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DDouble.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetDirectionalViewMapDensityF1D_Type;

#define BPy_GetDirectionalViewMapDensityF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetDirectionalViewMapDensityF1D_Type))

/*---------------------------Python BPy_GetDirectionalViewMapDensityF1D structure
 * definition----------*/
struct BPy_GetDirectionalViewMapDensityF1D {
  BPy_UnaryFunction1DDouble py_uf1D_double;
};

///////////////////////////////////////////////////////////////////////////////////////////
