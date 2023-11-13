/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../view_map/ViewMap.h"
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DVectorViewShape_Type;

#define BPy_UnaryFunction0DVectorViewShape_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DVectorViewShape_Type))

/*---------------------------Python BPy_UnaryFunction0DVectorViewShape structure
 * definition----------*/
typedef struct {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<std::vector<Freestyle::ViewShape *>> *uf0D_vectorviewshape;
} BPy_UnaryFunction0DVectorViewShape;

/*---------------------------Python BPy_UnaryFunction0DVectorViewShape visible
 * prototypes-----------*/
int UnaryFunction0DVectorViewShape_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
