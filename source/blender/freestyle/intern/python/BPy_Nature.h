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

#include "../winged_edge/Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Nature_Type;

#define BPy_Nature_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&Nature_Type))

/*---------------------------Python BPy_Nature structure definition----------*/
typedef struct {
  PyLongObject i;
} BPy_Nature;

/*---------------------------Python BPy_Nature visible prototypes-----------*/

int Nature_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
