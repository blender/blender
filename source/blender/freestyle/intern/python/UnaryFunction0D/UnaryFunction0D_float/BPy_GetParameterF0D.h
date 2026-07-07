/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DFloat.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject GetParameterF0D_Type;

#define BPy_GetParameterF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&GetParameterF0D_Type))

/*---------------------------Python BPy_GetParameterF0D structure definition----------*/
struct BPy_GetParameterF0D {
  BPy_UnaryFunction0DFloat py_uf0D_float;
};

///////////////////////////////////////////////////////////////////////////////////////////
