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

extern PyTypeObject orientedViewEdgeIterator_Type;

#define BPy_orientedViewEdgeIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&orientedViewEdgeIterator_Type))

/*---------------------------Python BPy_orientedViewEdgeIterator structure definition----------*/
typedef struct {
  BPy_Iterator py_it;
  Freestyle::ViewVertexInternal::orientedViewEdgeIterator *ove_it;
  bool reversed;
  bool at_start;
} BPy_orientedViewEdgeIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
