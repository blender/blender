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

extern PyTypeObject IncreasingColorShader_Type;

#define BPy_IncreasingColorShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&IncreasingColorShader_Type))

/*---------------------------Python BPy_IncreasingColorShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_IncreasingColorShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
