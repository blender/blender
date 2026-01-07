/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1D.h"

#include "../../winged_edge/Nature.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1DEdgeNature_Type;

#define BPy_UnaryFunction1DEdgeNature_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1DEdgeNature_Type))

/*---------------------------Python BPy_UnaryFunction1DEdgeNature structure definition----------*/
struct BPy_UnaryFunction1DEdgeNature {
  BPy_UnaryFunction1D py_uf1D;
  Freestyle::UnaryFunction1D<Freestyle::Nature::EdgeNature> *uf1D_edgenature;
};

/*---------------------------Python BPy_UnaryFunction1DEdgeNature visible prototypes-----------*/
int UnaryFunction1DEdgeNature_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////
