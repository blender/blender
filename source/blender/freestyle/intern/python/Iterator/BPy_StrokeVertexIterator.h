/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Iterator.h"

#include "../../stroke/StrokeIterators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject StrokeVertexIterator_Type;

#define BPy_StrokeVertexIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&StrokeVertexIterator_Type))

/*---------------------------Python BPy_StrokeVertexIterator structure definition----------*/
typedef struct {
  BPy_Iterator py_it;
  Freestyle::StrokeInternal::StrokeVertexIterator *sv_it;
  bool reversed;
  /* attribute to make next() work correctly */
  bool at_start;
} BPy_StrokeVertexIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
