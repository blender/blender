/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface1D.h"

#include "../../stroke/Curve.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FrsCurve_Type;

#define BPy_FrsCurve_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&FrsCurve_Type))

/*---------------------------Python BPy_FrsCurve structure definition----------*/
typedef struct {
  BPy_Interface1D py_if1D;
  Freestyle::Curve *c;
} BPy_FrsCurve;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
