/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction1D.h"

#include "../../view_map/ViewMap.h"
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1DVectorViewShape_Type;

#define BPy_UnaryFunction1DVectorViewShape_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1DVectorViewShape_Type))

/*---------------------------Python BPy_UnaryFunction1DVectorViewShape structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction1D py_uf1D;
  Freestyle::UnaryFunction1D<std::vector<Freestyle::ViewShape *>> *uf1D_vectorviewshape;
} BPy_UnaryFunction1DVectorViewShape;

/*---------------------------Python BPy_UnaryFunction1DVectorViewShape visible
 * prototypes-----------*/
int UnaryFunction1DVectorViewShape_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
