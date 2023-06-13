/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject BezierCurveShader_Type;

#define BPy_BezierCurveShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&BezierCurveShader_Type))

/*---------------------------Python BPy_BezierCurveShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_BezierCurveShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
