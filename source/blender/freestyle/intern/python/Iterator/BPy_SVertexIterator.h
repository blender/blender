/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

extern PyTypeObject SVertexIterator_Type;

#define BPy_SVertexIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&SVertexIterator_Type))

/*---------------------------Python BPy_SVertexIterator structure definition----------*/
typedef struct {
  BPy_Iterator py_it;
  Freestyle::ViewEdgeInternal::SVertexIterator *sv_it;
} BPy_SVertexIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
