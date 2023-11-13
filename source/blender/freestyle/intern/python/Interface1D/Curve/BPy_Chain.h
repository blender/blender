/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_FrsCurve.h"

#include "../../../stroke/Chain.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Chain_Type;

#define BPy_Chain_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&Chain_Type))

/*---------------------------Python BPy_Chain structure definition----------*/
typedef struct {
  BPy_FrsCurve py_c;
  Freestyle::Chain *c;
} BPy_Chain;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
