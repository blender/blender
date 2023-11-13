/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Iterator.h"

#include "../../view_map/ViewMapIterators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ViewEdgeIterator_Type;

#define BPy_ViewEdgeIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ViewEdgeIterator_Type))

/*---------------------------Python BPy_ViewEdgeIterator structure definition----------*/
typedef struct {
  BPy_Iterator py_it;
  Freestyle::ViewEdgeInternal::ViewEdgeIterator *ve_it;
} BPy_ViewEdgeIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
