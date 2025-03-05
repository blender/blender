/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_StrokeShader.h"

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject BlenderTextureShader_Type;

#define BPy_BlenderTextureShader_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&BlenderTextureShader_Type))

/*---------------------------Python BPy_BlenderTextureShader structure definition-----------*/
typedef struct {
  BPy_StrokeShader py_ss;
} BPy_BlenderTextureShader;

///////////////////////////////////////////////////////////////////////////////////////////
