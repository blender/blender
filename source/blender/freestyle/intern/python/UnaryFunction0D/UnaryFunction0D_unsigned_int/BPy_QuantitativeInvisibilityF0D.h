/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DUnsigned.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject QuantitativeInvisibilityF0D_Type;

#define BPy_QuantitativeInvisibilityF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&QuantitativeInvisibilityF0D_Type))

/*---------------------------Python BPy_QuantitativeInvisibilityF0D structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction0DUnsigned py_uf0D_unsigned;
} BPy_QuantitativeInvisibilityF0D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
