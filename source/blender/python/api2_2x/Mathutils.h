/* * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_Mathutils_H
#define EXPP_Mathutils_H

#include <Python.h>
#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_utildefines.h>
#include <BLI_blenlib.h>
#include <BLI_arithb.h>
#include <PIL_time.h>
#include <BLI_rand.h>
#include <math.h>
#include "vector.h"
#include "euler.h"
#include "quat.h"
#include "matrix.h"
#include "blendef.h"
#include "mydevice.h"
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"
#include "Types.h"


/*****************************************************************************/
// Python API function prototypes for the Mathutils module.												
/*****************************************************************************/
static PyObject *M_Mathutils_Rand (PyObject *self, PyObject *args);
static PyObject *M_Mathutils_Vector(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_CrossVecs(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_DotVecs (PyObject *self, PyObject *args);
static PyObject *M_Mathutils_AngleBetweenVecs(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_MidpointVecs(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_VecMultMat(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_ProjectVecs(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_CopyVec(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_Matrix(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_RotationMatrix(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_ScaleMatrix(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_OrthoProjectionMatrix(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_ShearMatrix(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_TranslationMatrix(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_MatMultVec(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_CopyMat(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_Quaternion(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_CrossQuats(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_DotQuats(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_CopyQuat(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_DifferenceQuats(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_Slerp(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_Euler(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_CopyEuler(PyObject *self, PyObject *args);
static PyObject *M_Mathutils_RotateEuler(PyObject *self, PyObject *args);

/*****************************************************************************/
// The following string definitions are used for documentation strings.		
// In Python these will be written to the console when doing a				
// Blender.Mathutils.__doc__		Mathutils Module strings																								 */
/*****************************************************************************/
static char M_Mathutils_doc[] =
"The Blender Mathutils module\n\n";
static char M_Mathutils_Vector_doc[] =
"() - create a new vector object from a list of floats";
static char M_Mathutils_Matrix_doc[] =
"() - create a new matrix object from a list of floats";
static char M_Mathutils_Quaternion_doc[] =
"() - create a quaternion from a list or an axis of rotation and an angle";
static char M_Mathutils_Euler_doc[] =
"() - create and return a new euler object";
static char M_Mathutils_Rand_doc[] =
"() - return a random number";
static char M_Mathutils_CrossVecs_doc[] =
"() - returns a vector perpedicular to the 2 vectors crossed";
static char M_Mathutils_CopyVec_doc[] =
"() - create a copy of vector";
static char M_Mathutils_DotVecs_doc[] =
"() - return the dot product of two vectors";
static char M_Mathutils_AngleBetweenVecs_doc[] =
"() - returns the angle between two vectors in degrees";
static char M_Mathutils_MidpointVecs_doc[] =
"() - return the vector to the midpoint between two vectors";
static char M_Mathutils_MatMultVec_doc[] =
"() - multiplies a matrix by a column vector";
static char M_Mathutils_VecMultMat_doc[] =
"() - multiplies a row vector by a matrix";
static char M_Mathutils_ProjectVecs_doc[] =
"() - returns the projection vector from the projection of vecA onto vecB";
static char M_Mathutils_RotationMatrix_doc[] =
"() - construct a rotation matrix from an angle and axis of rotation";
static char M_Mathutils_ScaleMatrix_doc[] =
"() - construct a scaling matrix from a scaling factor";
static char M_Mathutils_OrthoProjectionMatrix_doc[] =
"() - construct a orthographic projection matrix from a selected plane";
static char M_Mathutils_ShearMatrix_doc[] =
"() - construct a shearing matrix from a plane of shear and a shear factor";
static char M_Mathutils_CopyMat_doc[] =
"() - create a copy of a matrix";
static char M_Mathutils_TranslationMatrix_doc[] =
"() - create a translation matrix from a vector";
static char M_Mathutils_CopyQuat_doc[] =
"() - copy quatB to quatA";
static char M_Mathutils_CopyEuler_doc[] =
"() - copy eulB to eultA";
static char M_Mathutils_CrossQuats_doc[] =
"() - return the mutliplication of two quaternions";
static char M_Mathutils_DotQuats_doc[] =
"() - return the dot product of two quaternions";
static char M_Mathutils_Slerp_doc[] =
"() - returns the interpolation between two quaternions";
static char M_Mathutils_DifferenceQuats_doc[] =
"() - return the angular displacment difference between two quats";
static char M_Mathutils_RotateEuler_doc[] =
"() - rotate euler by an axis and angle";


/*****************************************************************************/
// Python method structure definition for Blender.Mathutils module:		
/*****************************************************************************/
struct PyMethodDef M_Mathutils_methods[] = {
	{"Rand",(PyCFunction)M_Mathutils_Rand, METH_VARARGS,
					M_Mathutils_Rand_doc},
	{"Vector",(PyCFunction)M_Mathutils_Vector, METH_VARARGS,
					M_Mathutils_Vector_doc},
	{"CrossVecs",(PyCFunction)M_Mathutils_CrossVecs, METH_VARARGS,
					M_Mathutils_CrossVecs_doc},
	{"DotVecs",(PyCFunction)M_Mathutils_DotVecs, METH_VARARGS,
					M_Mathutils_DotVecs_doc},
	{"AngleBetweenVecs",(PyCFunction)M_Mathutils_AngleBetweenVecs, METH_VARARGS,
					M_Mathutils_AngleBetweenVecs_doc},
	{"MidpointVecs",(PyCFunction)M_Mathutils_MidpointVecs, METH_VARARGS,
					M_Mathutils_MidpointVecs_doc},
	{"VecMultMat",(PyCFunction)M_Mathutils_VecMultMat, METH_VARARGS,
					M_Mathutils_VecMultMat_doc},
	{"ProjectVecs",(PyCFunction)M_Mathutils_ProjectVecs, METH_VARARGS,
					M_Mathutils_ProjectVecs_doc},
	{"CopyVec",(PyCFunction)M_Mathutils_CopyVec, METH_VARARGS,
					M_Mathutils_CopyVec_doc},
	{"Matrix",(PyCFunction)M_Mathutils_Matrix, METH_VARARGS,
					M_Mathutils_Matrix_doc},
	{"RotationMatrix",(PyCFunction)M_Mathutils_RotationMatrix, METH_VARARGS,
					M_Mathutils_RotationMatrix_doc},
	{"ScaleMatrix",(PyCFunction)M_Mathutils_ScaleMatrix, METH_VARARGS,
					M_Mathutils_ScaleMatrix_doc},
	{"ShearMatrix",(PyCFunction)M_Mathutils_ShearMatrix, METH_VARARGS,
					M_Mathutils_ShearMatrix_doc},
	{"TranslationMatrix",(PyCFunction)M_Mathutils_TranslationMatrix, METH_VARARGS,
					M_Mathutils_TranslationMatrix_doc},
	{"CopyMat",(PyCFunction)M_Mathutils_CopyMat, METH_VARARGS,
					M_Mathutils_CopyMat_doc},
	{"OrthoProjectionMatrix",(PyCFunction)M_Mathutils_OrthoProjectionMatrix, METH_VARARGS,
					M_Mathutils_OrthoProjectionMatrix_doc},
	{"MatMultVec",(PyCFunction)M_Mathutils_MatMultVec, METH_VARARGS,
					M_Mathutils_MatMultVec_doc},
	{"Quaternion",(PyCFunction)M_Mathutils_Quaternion, METH_VARARGS,
					M_Mathutils_Quaternion_doc},
	{"CopyQuat",(PyCFunction)M_Mathutils_CopyQuat, METH_VARARGS,
					M_Mathutils_CopyQuat_doc},
	{"CrossQuats",(PyCFunction)M_Mathutils_CrossQuats, METH_VARARGS,
					M_Mathutils_CrossQuats_doc},
	{"DotQuats",(PyCFunction)M_Mathutils_DotQuats, METH_VARARGS,
					M_Mathutils_DotQuats_doc},
	{"DifferenceQuats",(PyCFunction)M_Mathutils_DifferenceQuats, METH_VARARGS,
					M_Mathutils_DifferenceQuats_doc},
	{"Slerp",(PyCFunction)M_Mathutils_Slerp, METH_VARARGS,
					M_Mathutils_Slerp_doc},
	{"Euler",(PyCFunction)M_Mathutils_Euler, METH_VARARGS,
					M_Mathutils_Euler_doc},
	{"CopyEuler",(PyCFunction)M_Mathutils_CopyEuler, METH_VARARGS,
					M_Mathutils_CopyEuler_doc},
	{"RotateEuler",(PyCFunction)M_Mathutils_RotateEuler, METH_VARARGS,
					M_Mathutils_RotateEuler_doc},
	{NULL, NULL, 0, NULL}
};

#endif /* EXPP_Mathutils_H */
