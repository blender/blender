/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryPredicate1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject QuantitativeInvisibilityUP1D_Type;

#define BPy_QuantitativeInvisibilityUP1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&QuantitativeInvisibilityUP1D_Type))

/*---------------------------Python BPy_QuantitativeInvisibilityUP1D structure
 * definition----------*/
typedef struct {
  BPy_UnaryPredicate1D py_up1D;
} BPy_QuantitativeInvisibilityUP1D;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
