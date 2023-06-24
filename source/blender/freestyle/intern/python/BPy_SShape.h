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

#include "../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject SShape_Type;

#define BPy_SShape_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&SShape_Type))

/*---------------------------Python BPy_SShape structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::SShape *ss;
  bool borrowed; /* true if *ss is a borrowed object */
} BPy_SShape;

/*---------------------------Python BPy_SShape visible prototypes-----------*/

int SShape_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
