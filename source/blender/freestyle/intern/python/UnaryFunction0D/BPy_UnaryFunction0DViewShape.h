/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryFunction0D.h"

#include "../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DViewShape_Type;

#define BPy_UnaryFunction0DViewShape_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DViewShape_Type))

/*---------------------------Python BPy_UnaryFunction0DViewShape structure definition----------*/
typedef struct {
  BPy_UnaryFunction0D py_uf0D;
  Freestyle::UnaryFunction0D<Freestyle::ViewShape *> *uf0D_viewshape;
} BPy_UnaryFunction0DViewShape;

/*---------------------------Python BPy_UnaryFunction0DViewShape visible prototypes-----------*/
int UnaryFunction0DViewShape_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
