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

extern PyTypeObject ThicknessNoiseShader_Type;

#define BPy_ThicknessNoiseShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ThicknessNoiseShader_Type))

/*---------------------------Python BPy_ThicknessNoiseShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_ThicknessNoiseShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
