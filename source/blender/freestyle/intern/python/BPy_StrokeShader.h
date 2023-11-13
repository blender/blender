/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../system/FreestyleConfig.h"

#include "../stroke/StrokeShader.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject StrokeShader_Type;

#define BPy_StrokeShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&StrokeShader_Type))

/*---------------------------Python BPy_StrokeShader structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::StrokeShader *ss;
} BPy_StrokeShader;

/*---------------------------Python BPy_StrokeShader visible prototypes-----------*/

int StrokeShader_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
