/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Willian P. Germano & Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef EXPP_vector_h
#define EXPP_vector_h

#include <Python.h>

extern PyTypeObject vector_Type;
#define VectorObject_Check(_v) PyObject_TypeCheck((_v), &vector_Type)

typedef struct { /* keep aligned with BaseMathObject in mathutils.h */
	PyObject_VAR_HEAD 
	float *vec;					/*1D array of data (alias), wrapped status depends on wrapped status */
	PyObject *cb_user;					/* if this vector references another object, otherwise NULL, *Note* this owns its reference */
	unsigned char cb_type;	/* which user funcs do we adhere to, RNA, GameObject, etc */
	unsigned char cb_subtype;		/* subtype: location, rotation... to avoid defining many new functions for every attribute of the same type */
	unsigned char wrapped;		/* wrapped data type? */
	/* end BaseMathObject */

	unsigned char size;			/* vec size 2,3 or 4 */
} VectorObject;

/*prototypes*/
PyObject *newVectorObject(float *vec, int size, int type, PyTypeObject *base_type);
PyObject *newVectorObject_cb(PyObject *user, int size, int callback_type, int subtype);

#endif				/* EXPP_vector_h */
