/* 
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

#include "Python.h"
#include "gen_utils.h"
#include "Types.h"
#include <BLI_arithb.h>
#include "euler.h"
#include "matrix.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*****************************/
//    Quaternion Python Object   
/*****************************/

#define QuaternionObject_Check(v) ((v)->ob_type == &quaternion_Type)

typedef struct {
	PyObject_VAR_HEAD
	float * quat;
	int flag;
		//0 - no coercion
		//1 - coerced from int
		//2 - coerced from float
} QuaternionObject;


//prototypes
PyObject *newQuaternionObject(float *quat);
PyObject *Quaternion_Identity(QuaternionObject *self);
PyObject *Quaternion_Negate(QuaternionObject *self);
PyObject *Quaternion_Conjugate(QuaternionObject *self);
PyObject *Quaternion_Inverse(QuaternionObject *self);
PyObject *Quaternion_Normalize(QuaternionObject *self);
PyObject *Quaternion_ToEuler(QuaternionObject *self);
PyObject *Quaternion_ToMatrix(QuaternionObject *self);

#endif /* EXPP_quat_h */

