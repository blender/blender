/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DUnsigned.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject QuantitativeInvisibilityF1D_Type;

#define BPy_QuantitativeInvisibilityF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&QuantitativeInvisibilityF1D_Type))

/*---------------------------Python BPy_QuantitativeInvisibilityF1D structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction1DUnsigned py_uf1D_unsigned;
} BPy_QuantitativeInvisibilityF1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
