/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ColorNoiseShader_Type;

#define BPy_ColorNoiseShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ColorNoiseShader_Type))

/*---------------------------Python BPy_ColorNoiseShader structure definition----------*/
struct BPy_ColorNoiseShader {
  BPy_StrokeShader py_ss;
};

///////////////////////////////////////////////////////////////////////////////////////////
