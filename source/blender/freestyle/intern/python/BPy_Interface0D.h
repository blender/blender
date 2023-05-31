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

#include "../view_map/Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Interface0D_Type;

#define BPy_Interface0D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Interface0D_Type))

/*---------------------------Python BPy_Interface0D structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::Interface0D *if0D;
  bool borrowed; /* true if *if0D is a borrowed object */
} BPy_Interface0D;

/*---------------------------Python BPy_Interface0D visible prototypes-----------*/

int Interface0D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
