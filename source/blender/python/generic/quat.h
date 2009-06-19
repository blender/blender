/* 
 * $Id: quat.h 20332 2009-05-22 03:22:56Z campbellbarton $
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef EXPP_quat_h
#define EXPP_quat_h

#include <Python.h>
#include "../intern/bpy_compat.h"

extern PyTypeObject quaternion_Type;

#define QuaternionObject_Check(v) (Py_TYPE(v) == &quaternion_Type)

typedef struct {
	PyObject_VAR_HEAD 
	struct{
		float *py_data;		//python managed
		float *blend_data;	//blender managed
	}data;
	float *quat;				//1D array of data (alias)
	int wrapped;			//is wrapped data?
} QuaternionObject;

/*struct data contains a pointer to the actual data that the
object uses. It can use either PyMem allocated data (which will
be stored in py_data) or be a wrapper for data allocated through
blender (stored in blend_data). This is an either/or struct not both*/

//prototypes
PyObject *Quaternion_Identity( QuaternionObject * self );
PyObject *Quaternion_Negate( QuaternionObject * self );
PyObject *Quaternion_Conjugate( QuaternionObject * self );
PyObject *Quaternion_Inverse( QuaternionObject * self );
PyObject *Quaternion_Normalize( QuaternionObject * self );
PyObject *Quaternion_ToEuler( QuaternionObject * self, PyObject *args );
PyObject *Quaternion_ToMatrix( QuaternionObject * self );
PyObject *Quaternion_Cross( QuaternionObject * self, QuaternionObject * value );
PyObject *Quaternion_Dot( QuaternionObject * self, QuaternionObject * value );
PyObject *Quaternion_copy( QuaternionObject * self );
PyObject *newQuaternionObject( float *quat, int type );

#endif				/* EXPP_quat_h */
