/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "BPy_ChainingIterator.h"

#include "../../stroke/ChainingIterators.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ChainSilhouetteIterator_Type;

#define BPy_ChainSilhouetteIterator_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&ChainSilhouetteIterator_Type))

/*---------------------------Python BPy_ChainSilhouetteIterator structure definition----------*/
typedef struct {
  BPy_ChainingIterator py_c_it;
  Freestyle::ChainSilhouetteIterator *cs_it;
} BPy_ChainSilhouetteIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
