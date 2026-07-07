/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject SpatialNoiseShader_Type;

#define BPy_SpatialNoiseShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&SpatialNoiseShader_Type))

/*---------------------------Python BPy_SpatialNoiseShader structure definition----------*/
struct BPy_SpatialNoiseShader {
  BPy_StrokeShader py_ss;
};

///////////////////////////////////////////////////////////////////////////////////////////
