

/* Matrix and vector objects in Python */

/* $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#ifndef EXPP_vector_h
#define EXPP_vector_h

#include "Python.h"

#include "BKE_utildefines.h"

#include "gen_utils.h"
#include "vector.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*****************************/
/*    Matrix Python Object   */
/*****************************/
/* temporar hack for typecasts */

typedef float (*Matrix4Ptr)[4];

typedef struct {
	PyObject_VAR_HEAD
	float *vec;
	int size;

} VectorObject;

typedef struct {
	PyObject_VAR_HEAD
	PyObject *rows[4];
	Matrix4Ptr mat;

} MatrixObject;


/* PROTOS */

PyObject *newVectorObject(float *vec, int size);
PyObject *newMatrixObject(float mat[][4]);
void init_py_matrix(void);

#endif /* EXPP_vector_h */
