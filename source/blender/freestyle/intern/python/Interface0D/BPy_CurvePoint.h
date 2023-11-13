/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Interface0D.h"

#include "../../stroke/Curve.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject CurvePoint_Type;

#define BPy_CurvePoint_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&CurvePoint_Type))

/*---------------------------Python BPy_CurvePoint structure definition----------*/
typedef struct {
  BPy_Interface0D py_if0D;
  Freestyle::CurvePoint *cp;
} BPy_CurvePoint;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
