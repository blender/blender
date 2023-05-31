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

extern PyTypeObject GuidingLinesShader_Type;

#define BPy_GuidingLinesShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GuidingLinesShader_Type))

/*---------------------------Python BPy_GuidingLinesShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_GuidingLinesShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
