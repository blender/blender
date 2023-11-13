/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Iterator.h"

#include "../../stroke/CurveIterators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject CurvePointIterator_Type;

#define BPy_CurvePointIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&CurvePointIterator_Type))

/*---------------------------Python BPy_CurvePointIterator structure definition----------*/
typedef struct {
  BPy_Iterator py_it;
  Freestyle::CurveInternal::CurvePointIterator *cp_it;
} BPy_CurvePointIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
