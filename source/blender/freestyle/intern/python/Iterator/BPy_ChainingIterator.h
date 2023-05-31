/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "BPy_ViewEdgeIterator.h"

#include "../../stroke/ChainingIterators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ChainingIterator_Type;

#define BPy_ChainingIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ChainingIterator_Type))

/*---------------------------Python BPy_ChainingIterator structure definition----------*/
typedef struct {
  BPy_ViewEdgeIterator py_ve_it;
  Freestyle::ChainingIterator *c_it;
} BPy_ChainingIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
