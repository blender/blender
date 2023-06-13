/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../winged_edge/Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DEdgeNature_Type;

#define BPy_UnaryFunction0DEdgeNature_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DEdgeNature_Type))

/*---------------------------Python BPy_UnaryFunction0DEdgeNature structure definition----------*/
typedef struct {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<Freestyle::Nature::EdgeNature> *uf0D_edgenature;
} BPy_UnaryFunction0DEdgeNature;

/*---------------------------Python BPy_UnaryFunction0DEdgeNature visible prototypes-----------*/
int UnaryFunction0DEdgeNature_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
