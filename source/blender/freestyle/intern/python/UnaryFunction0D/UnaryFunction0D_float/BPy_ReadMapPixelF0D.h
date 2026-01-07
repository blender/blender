/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0DFloat.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ReadMapPixelF0D_Type;

#define BPy_ReadMapPixelF0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ReadMapPixelF0D_Type))

/*---------------------------Python BPy_ReadMapPixelF0D structure definition----------*/
struct BPy_ReadMapPixelF0D {
  BPy_UnaryFunction0DFloat py_uf0D_float;
};

///////////////////////////////////////////////////////////////////////////////////////////
