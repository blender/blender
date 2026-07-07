/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DFloat_Type;

#define BPy_UnaryFunction0DFloat_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DFloat_Type))

/*---------------------------Python BPy_UnaryFunction0DFloat structure definition----------*/
struct BPy_UnaryFunction0DFloat {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<float> *uf0D_float;
};

/*---------------------------Python BPy_UnaryFunction0DFloat visible prototypes-----------*/
int UnaryFunction0DFloat_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
