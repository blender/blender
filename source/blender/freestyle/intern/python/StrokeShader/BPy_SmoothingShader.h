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

extern PyTypeObject SmoothingShader_Type;

#define BPy_SmoothingShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&SmoothingShader_Type))

/*---------------------------Python BPy_SmoothingShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_SmoothingShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
