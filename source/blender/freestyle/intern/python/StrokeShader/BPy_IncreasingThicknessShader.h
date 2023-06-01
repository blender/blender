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

extern PyTypeObject IncreasingThicknessShader_Type;

#define BPy_IncreasingThicknessShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&IncreasingThicknessShader_Type))

/*---------------------------Python BPy_IncreasingThicknessShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_IncreasingThicknessShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
