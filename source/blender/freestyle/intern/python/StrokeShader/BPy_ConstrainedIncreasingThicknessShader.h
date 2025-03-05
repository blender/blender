/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ConstrainedIncreasingThicknessShader_Type;

#define BPy_ConstrainedIncreasingThicknessShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ConstrainedIncreasingThicknessShader_Type))

/*---------------------------Python BPy_ConstrainedIncreasingThicknessShader structure
 * definition----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_ConstrainedIncreasingThicknessShader;

///////////////////////////////////////////////////////////////////////////////////////////
