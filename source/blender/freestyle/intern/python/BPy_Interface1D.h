/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

extern "C" {
#include <Python.h>
}

#include "../view_map/Interface1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Interface1D_Type;

#define BPy_Interface1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Interface1D_Type))

/*---------------------------Python BPy_Interface1D structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::Interface1D *if1D;
  bool borrowed; /* true if *if1D is a borrowed object */
} BPy_Interface1D;

/*---------------------------Python BPy_Interface1D visible prototypes-----------*/

int Interface1D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
