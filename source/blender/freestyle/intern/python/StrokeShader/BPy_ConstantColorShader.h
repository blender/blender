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

extern PyTypeObject ConstantColorShader_Type;

#define BPy_ConstantColorShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ConstantColorShader_Type))

/*---------------------------Python BPy_ConstantColorShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_ConstantColorShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
