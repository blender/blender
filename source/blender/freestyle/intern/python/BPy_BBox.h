/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject BBox_Type;

#define BPy_BBox_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&BBox_Type))

/*---------------------------Python BPy_BBox structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::BBox<Freestyle::Geometry::Vec3r> *bb;
} BPy_BBox;

/*---------------------------Python BPy_BBox visible prototypes-----------*/

int BBox_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
