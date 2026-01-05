/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1DVec2f.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Normal2DF1D_Type;

#define BPy_Normal2DF1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Normal2DF1D_Type))

/*---------------------------Python BPy_Normal2DF1D structure definition----------*/
struct BPy_Normal2DF1D {
  BPy_UnaryFunction1DVec2f py_uf1D_vec2f;
};

///////////////////////////////////////////////////////////////////////////////////////////
