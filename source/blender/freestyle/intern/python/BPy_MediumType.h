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

#include "../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject MediumType_Type;

#define BPy_MediumType_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&MediumType_Type))

/*---------------------------Python BPy_MediumType structure definition----------*/
typedef struct {
  PyLongObject i;
} BPy_MediumType;

/*---------------------------Python BPy_MediumType visible prototypes-----------*/

int MediumType_Init(PyObject *module);

// internal constants
extern PyLongObject _BPy_MediumType_DRY_MEDIUM;
extern PyLongObject _BPy_MediumType_HUMID_MEDIUM;
extern PyLongObject _BPy_MediumType_OPAQUE_MEDIUM;
// public constants
#define BPy_MediumType_DRY_MEDIUM ((PyObject *)&_BPy_MediumType_DRY_MEDIUM)
#define BPy_MediumType_HUMID_MEDIUM ((PyObject *)&_BPy_MediumType_HUMID_MEDIUM)
#define BPy_MediumType_OPAQUE_MEDIUM ((PyObject *)&_BPy_MediumType_OPAQUE_MEDIUM)

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
