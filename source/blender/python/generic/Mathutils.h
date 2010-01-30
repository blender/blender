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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
*/
//Include this file for access to vector, quat, matrix, euler, etc...

#ifndef EXPP_Mathutils_H
#define EXPP_Mathutils_H

#include <Python.h>
#include "vector.h"
#include "matrix.h"
#include "quat.h"
#include "euler.h"

/* #define USE_MATHUTILS_DEG - for backwards compat */

/* Can cast different mathutils types to this, use for generic funcs */

extern char BaseMathObject_Wrapped_doc[];
extern char BaseMathObject_Owner_doc[];

typedef struct {
	PyObject_VAR_HEAD
	float *data;					/*array of data (alias), wrapped status depends on wrapped status */
	PyObject *cb_user;					/* if this vector references another object, otherwise NULL, *Note* this owns its reference */
	unsigned char cb_type;	/* which user funcs do we adhere to, RNA, GameObject, etc */
	unsigned char cb_subtype;		/* subtype: location, rotation... to avoid defining many new functions for every attribute of the same type */
	unsigned char wrapped;		/* wrapped data type? */
} BaseMathObject;

PyObject *BaseMathObject_getOwner( BaseMathObject * self, void * );
PyObject *BaseMathObject_getWrapped( BaseMathObject *self, void * );
void BaseMathObject_dealloc(BaseMathObject * self);

PyObject *Mathutils_Init(void);

PyObject *quat_rotation(PyObject *arg1, PyObject *arg2);

int EXPP_FloatsAreEqual(float A, float B, int floatSteps);
int EXPP_VectorsAreEqual(float *vecA, float *vecB, int size, int floatSteps);


#define Py_PI  3.14159265358979323846

#define Py_NEW  1
#define Py_WRAP 2


/* Mathutils is used by the BGE and Blender so have to define 
 * some things here for luddite mac users of py2.3 */
#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE  return Py_INCREF(Py_None), Py_None
#endif
#ifndef Py_RETURN_FALSE
#define Py_RETURN_FALSE  return Py_INCREF(Py_False), Py_False
#endif
#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE  return Py_INCREF(Py_True), Py_True
#endif

typedef struct Mathutils_Callback Mathutils_Callback;

typedef int (*BaseMathCheckFunc)(PyObject *);
typedef int (*BaseMathGetFunc)(PyObject *, int, float *);
typedef int (*BaseMathSetFunc)(PyObject *, int, float *);
typedef int (*BaseMathGetIndexFunc)(PyObject *, int, float *, int);
typedef int (*BaseMathSetIndexFunc)(PyObject *, int, float *, int);

struct Mathutils_Callback {
	int		(*check)(PyObject *user);					/* checks the user is still valid */
	int		(*get)(PyObject *user, int subtype, float *from);	/* gets the vector from the user */
	int		(*set)(PyObject *user, int subtype, float *to);	/* sets the users vector values once the vector is modified */
	int		(*get_index)(PyObject *user, int subtype, float *from,int index);	/* same as above but only for an index */
	int		(*set_index)(PyObject *user, int subtype, float *to,	int index);	/* same as above but only for an index */
};

int Mathutils_RegisterCallback(Mathutils_Callback *cb);

int _BaseMathObject_ReadCallback(BaseMathObject *self);
int _BaseMathObject_WriteCallback(BaseMathObject *self);
int _BaseMathObject_ReadIndexCallback(BaseMathObject *self, int index);
int _BaseMathObject_WriteIndexCallback(BaseMathObject *self, int index);

/* since this is called so often avoid where possible */
#define BaseMath_ReadCallback(_self) (((_self)->cb_user ?	_BaseMathObject_ReadCallback((BaseMathObject *)_self):1))
#define BaseMath_WriteCallback(_self) (((_self)->cb_user ?_BaseMathObject_WriteCallback((BaseMathObject *)_self):1))
#define BaseMath_ReadIndexCallback(_self, _index) (((_self)->cb_user ?	_BaseMathObject_ReadIndexCallback((BaseMathObject *)_self, _index):1))
#define BaseMath_WriteIndexCallback(_self, _index) (((_self)->cb_user ?	_BaseMathObject_WriteIndexCallback((BaseMathObject *)_self, _index):1))

#endif				/* EXPP_Mathutils_H */
