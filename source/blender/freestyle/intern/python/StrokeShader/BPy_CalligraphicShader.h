/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject CalligraphicShader_Type;

#define BPy_CalligraphicShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&CalligraphicShader_Type)

/*---------------------------Python BPy_CalligraphicShader structure definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_CalligraphicShader;

///////////////////////////////////////////////////////////////////////////////////////////
