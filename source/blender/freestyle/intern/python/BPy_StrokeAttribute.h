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

extern PyTypeObject StrokeAttribute_Type;

#define BPy_StrokeAttribute_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&StrokeAttribute_Type))

/*---------------------------Python BPy_StrokeAttribute structure definition----------*/
typedef struct {
  PyObject_HEAD
  Freestyle::StrokeAttribute *sa;
  bool borrowed; /* true if *sa is a borrowed reference */
} BPy_StrokeAttribute;

/*---------------------------Python BPy_StrokeAttribute visible prototypes-----------*/

int StrokeAttribute_Init(PyObject *module);
void StrokeAttribute_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
