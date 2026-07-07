/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../system/Id.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DId_Type;

#define BPy_UnaryFunction0DId_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DId_Type))

/*---------------------------Python BPy_UnaryFunction0DId structure definition----------*/
struct BPy_UnaryFunction0DId {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<Freestyle::Id> *uf0D_id;
};

/*---------------------------Python BPy_UnaryFunction0DId visible prototypes-----------*/
int UnaryFunction0DId_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
