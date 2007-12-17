/* 
 * $Id: quat.h 8367 2006-08-22 09:13:44Z campbellbarton $
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
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#ifndef EXPP_quat_h
#define EXPP_quat_h

#include <Python.h>

extern PyTypeObject quaternion_Type;

#define QuaternionObject_Check(v) ((v)->ob_type == &quaternion_Type)

typedef struct {
	PyObject_VAR_HEAD 
	struct{
		float *py_data;		//python managed
		float *blend_data;	//blender managed
	}data;
	float *quat;				//1D array of data (alias)
	int wrapped;			//is wrapped data?
	PyObject *coerced_object;
} QuaternionObject;
/*coerced_object is a pointer to the object that it was
coerced from when a dummy vector needs to be created from
the coerce() function for numeric protocol operations*/

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
PyObject *Quaternion_ToEuler( QuaternionObject * self );
PyObject *Quaternion_ToMatrix( QuaternionObject * self );
PyObject *Quaternion_copy( QuaternionObject * self );
PyObject *newQuaternionObject( float *quat, int type );

#endif				/* EXPP_quat_h */
