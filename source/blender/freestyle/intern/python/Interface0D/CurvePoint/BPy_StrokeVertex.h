/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_CurvePoint.h"

#include "../../../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject StrokeVertex_Type;

#define BPy_StrokeVertex_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&StrokeVertex_Type))

/*---------------------------Python BPy_StrokeVertex structure definition----------*/
typedef struct {
  BPy_CurvePoint py_cp;
  Freestyle::StrokeVertex *sv;
} BPy_StrokeVertex;

/*---------------------------Python BPy_StrokeVertex visible prototypes-----------*/

void StrokeVertex_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
