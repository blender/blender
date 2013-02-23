/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/Iterator/BPy_ChainSilhouetteIterator.h
 *  \ingroup freestyle
 */

#ifndef FREESTYLE_PYTHON_CHAINSILHOUETTEITERATOR_H
#define FREESTYLE_PYTHON_CHAINSILHOUETTEITERATOR_H


#include "../../stroke/ChainingIterators.h"

#include "BPy_ChainingIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ChainSilhouetteIterator_Type;

#define BPy_ChainSilhouetteIterator_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ChainSilhouetteIterator_Type)  )

/*---------------------------Python BPy_ChainSilhouetteIterator structure definition----------*/
typedef struct {
	BPy_ChainingIterator py_c_it;
	ChainSilhouetteIterator *cs_it;
} BPy_ChainSilhouetteIterator;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_CHAINSILHOUETTEITERATOR_H */
