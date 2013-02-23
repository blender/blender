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

/** \file source/blender/freestyle/intern/python/BPy_ViewShape.h
 *  \ingroup freestyle
 */

#ifndef FREESTYLE_PYTHON_VIEWSHAPE_H
#define FREESTYLE_PYTHON_VIEWSHAPE_H

#include <Python.h>

#include "../view_map/ViewMap.h"

#include "BPy_SShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject ViewShape_Type;

#define BPy_ViewShape_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &ViewShape_Type)  )

/*---------------------------Python BPy_ViewShape structure definition----------*/
typedef struct {
	PyObject_HEAD
	ViewShape *vs;
	int borrowed; /* non-zero if *vs a borrowed object */
	BPy_SShape *py_ss;
} BPy_ViewShape;

/*---------------------------Python BPy_ViewShape visible prototypes-----------*/

int ViewShape_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_VIEWSHAPE_H */
