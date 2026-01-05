/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DVec2f.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Normal2DF0D_Type;

#define BPy_Normal2DF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Normal2DF0D_Type))

/*---------------------------Python BPy_Normal2DF0D structure definition----------*/
struct BPy_Normal2DF0D {
  BPy_UnaryFunction0DVec2f py_uf0D_vec2f;
};

///////////////////////////////////////////////////////////////////////////////////////////
