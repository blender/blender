/* SPDX-FileCopyrightText: 2023 Blender Authors
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

extern PyTypeObject ColorNoiseShader_Type;

#define BPy_ColorNoiseShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ColorNoiseShader_Type))

/*---------------------------Python BPy_ColorNoiseShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_ColorNoiseShader;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
