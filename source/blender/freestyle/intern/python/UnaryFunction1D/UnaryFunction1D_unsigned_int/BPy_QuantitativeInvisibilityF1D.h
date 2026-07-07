/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DUnsigned.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject QuantitativeInvisibilityF1D_Type;

#define BPy_QuantitativeInvisibilityF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&QuantitativeInvisibilityF1D_Type))

/*---------------------------Python BPy_QuantitativeInvisibilityF1D structure
 * definition----------*/
struct BPy_QuantitativeInvisibilityF1D {
  BPy_UnaryFunction1DUnsigned py_uf1D_unsigned;
};

///////////////////////////////////////////////////////////////////////////////////////////
