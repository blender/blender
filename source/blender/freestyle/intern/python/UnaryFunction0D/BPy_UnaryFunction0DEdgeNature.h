/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../winged_edge/Nature.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DEdgeNature_Type;

#define BPy_UnaryFunction0DEdgeNature_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DEdgeNature_Type))

/*---------------------------Python BPy_UnaryFunction0DEdgeNature structure definition----------*/
struct BPy_UnaryFunction0DEdgeNature {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<Freestyle::Nature::EdgeNature> *uf0D_edgenature;
};

/*---------------------------Python BPy_UnaryFunction0DEdgeNature visible prototypes-----------*/
int UnaryFunction0DEdgeNature_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
