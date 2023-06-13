/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_Iterator.h"

#include "../../view_map/Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject Interface0DIterator_Type;

#define BPy_Interface0DIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&Interface0DIterator_Type))

/*---------------------------Python BPy_Interface0DIterator structure definition----------*/
typedef struct {
  BPy_Iterator py_it;
  Freestyle::Interface0DIterator *if0D_it;
  bool reversed;
  bool at_start;
} BPy_Interface0DIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
