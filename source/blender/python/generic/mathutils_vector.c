/*
 * $Id$
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
 * 
 * Contributor(s): Willian P. Germano, Joseph Gilbert, Ken Hughes, Alex Fraser, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "mathutils.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"



#define MAX_DIMENSIONS 4
/* Swizzle axes get packed into a single value that is used as a closure. Each
   axis uses SWIZZLE_BITS_PER_AXIS bits. The first bit (SWIZZLE_VALID_AXIS) is
   used as a sentinel: if it is unset, the axis is not valid. */
#define SWIZZLE_BITS_PER_AXIS 3
#define SWIZZLE_VALID_AXIS 0x4
#define SWIZZLE_AXIS       0x3

static PyObject *Vector_ToTupleExt(VectorObject *self, int ndigits);

//----------------------------------mathutils.Vector() ------------------
// Supports 2D, 3D, and 4D vector objects both int and float values
// accepted. Mixed float and int values accepted. Ints are parsed to float 
static PyObject *Vector_new(PyTypeObject *type, PyObject *args, PyObject *UNUSED(kwds))
{
	float vec[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	int size= 3; /* default to a 3D vector */

	switch(PyTuple_GET_SIZE(args)) {
	case 0:
		break;
	case 1:
		if((size=mathutils_array_parse(vec, 2, 4, PyTuple_GET_ITEM(args, 0), "mathutils.Vector()")) == -1)
			return NULL;
		break;
	default:
		PyErr_SetString(PyExc_TypeError, "mathutils.Vector(): more then a single arg given");
		return NULL;
	}
	return newVectorObject(vec, size, Py_NEW, type);
}

/*-----------------------------METHODS---------------------------- */
static char Vector_Zero_doc[] =
".. method:: zero()\n"
"\n"
"   Set all values to zero.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Zero(VectorObject *self)
{
	int i;
	for(i = 0; i < self->size; i++) {
		self->vec[i] = 0.0f;
	}
	
	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
/*----------------------------Vector.normalize() ----------------- */
static char Vector_Normalize_doc[] =
".. method:: normalize()\n"
"\n"
"   Normalize the vector, making the length of the vector always 1.0.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. warning:: Normalizing a vector where all values are zero results in all axis having a nan value (not a number).\n"
"\n"
"   .. note:: Normalize works for vectors of all sizes, however 4D Vectors w axis is left untouched.\n";

static PyObject *Vector_Normalize(VectorObject *self)
{
	int i;
	float norm = 0.0f;

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	for(i = 0; i < self->size; i++) {
		norm += self->vec[i] * self->vec[i];
	}
	norm = (float) sqrt(norm);
	for(i = 0; i < self->size; i++) {
		self->vec[i] /= norm;
	}
	
	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}


/*----------------------------Vector.resize2D() ------------------ */
static char Vector_Resize2D_doc[] =
".. method:: resize2D()\n"
"\n"
"   Resize the vector to 2D  (x, y).\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Resize2D(VectorObject *self)
{
	if(self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError, "vector.resize2D(): cannot resize wrapped data - only python vectors");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError, "vector.resize2D(): cannot resize a vector that has an owner");
		return NULL;
	}
	
	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 2));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError, "vector.resize2D(): problem allocating pointer space");
		return NULL;
	}
	
	self->size = 2;
	Py_INCREF(self);
	return (PyObject*)self;
}
/*----------------------------Vector.resize3D() ------------------ */
static char Vector_Resize3D_doc[] =
".. method:: resize3D()\n"
"\n"
"   Resize the vector to 3D  (x, y, z).\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Resize3D(VectorObject *self)
{
	if (self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError, "vector.resize3D(): cannot resize wrapped data - only python vectors");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError, "vector.resize3D(): cannot resize a vector that has an owner");
		return NULL;
	}
	
	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 3));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError, "vector.resize3D(): problem allocating pointer space");
		return NULL;
	}
	
	if(self->size == 2)
		self->vec[2] = 0.0f;
	
	self->size = 3;
	Py_INCREF(self);
	return (PyObject*)self;
}
/*----------------------------Vector.resize4D() ------------------ */
static char Vector_Resize4D_doc[] =
".. method:: resize4D()\n"
"\n"
"   Resize the vector to 4D (x, y, z, w).\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Resize4D(VectorObject *self)
{
	if(self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError, "vector.resize4D(): cannot resize wrapped data - only python vectors");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError, "vector.resize4D(): cannot resize a vector that has an owner");
		return NULL;
	}
	
	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 4));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError, "vector.resize4D(): problem allocating pointer space");
		return NULL;
	}
	if(self->size == 2){
		self->vec[2] = 0.0f;
		self->vec[3] = 1.0f;
	}else if(self->size == 3){
		self->vec[3] = 1.0f;
	}
	self->size = 4;
	Py_INCREF(self);
	return (PyObject*)self;
}

/*----------------------------Vector.toTuple() ------------------ */
static char Vector_ToTuple_doc[] =
".. method:: to_tuple(precision=-1)\n"
"\n"
"   Return this vector as a tuple with.\n"
"\n"
"   :arg precision: The number to round the value to in [-1, 21].\n"
"   :type precision: int\n"
"   :return: the values of the vector rounded by *precision*\n"
"   :rtype: tuple\n";

/* note: BaseMath_ReadCallback must be called beforehand */
static PyObject *Vector_ToTupleExt(VectorObject *self, int ndigits)
{
	PyObject *ret;
	int i;

	ret= PyTuple_New(self->size);

	if(ndigits >= 0) {
		for(i = 0; i < self->size; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round((double)self->vec[i], ndigits)));
		}
	}
	else {
		for(i = 0; i < self->size; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->vec[i]));
		}
	}

	return ret;
}

static PyObject *Vector_ToTuple(VectorObject *self, PyObject *args)
{
	int ndigits= 0;

	if(!PyArg_ParseTuple(args, "|i:to_tuple", &ndigits))
		return NULL;

	if(ndigits > 22 || ndigits < 0) {
		PyErr_SetString(PyExc_ValueError, "vector.to_tuple(ndigits): ndigits must be between 0 and 21");
		return NULL;
	}

	if(PyTuple_GET_SIZE(args)==0)
		ndigits= -1;

	if(!BaseMath_ReadCallback(self))
		return NULL;

	return Vector_ToTupleExt(self, ndigits);
}

/*----------------------------Vector.toTrackQuat(track, up) ---------------------- */
static char Vector_ToTrackQuat_doc[] =
".. method:: to_track_quat(track, up)\n"
"\n"
"   Return a quaternion rotation from the vector and the track and up axis.\n"
"\n"
"   :arg track: Track axis in ['X', 'Y', 'Z', '-X', '-Y', '-Z'].\n"
"   :type track: string\n"
"   :arg up: Up axis in ['X', 'Y', 'Z'].\n"
"   :type up: string\n"
"   :return: rotation from the vector and the track and up axis.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Vector_ToTrackQuat(VectorObject *self, PyObject *args )
{
	float vec[3], quat[4];
	char *strack, *sup;
	short track = 2, up = 1;

	if(!PyArg_ParseTuple( args, "|ss:to_track_quat", &strack, &sup))
		return NULL;

	if (self->size != 3) {
		PyErr_SetString(PyExc_TypeError, "only for 3D vectors");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self))
		return NULL;

	if (strack) {
		if (strlen(strack) == 2) {
			if (strack[0] == '-') {
				switch(strack[1]) {
					case 'X':
						track = 3;
						break;
					case 'Y':
						track = 4;
						break;
					case 'Z':
						track = 5;
						break;
					default:
						PyErr_SetString(PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis");
						return NULL;
				}
			}
			else {
				PyErr_SetString(PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis");
				return NULL;
			}
		}
		else if (strlen(strack) == 1) {
			switch(strack[0]) {
			case '-':
			case 'X':
				track = 0;
				break;
			case 'Y':
				track = 1;
				break;
			case 'Z':
				track = 2;
				break;
			default:
				PyErr_SetString(PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis");
				return NULL;
			}
		}
		else {
			PyErr_SetString(PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis");
			return NULL;
		}
	}

	if (sup) {
		if (strlen(sup) == 1) {
			switch(*sup) {
			case 'X':
				up = 0;
				break;
			case 'Y':
				up = 1;
				break;
			case 'Z':
				up = 2;
				break;
			default:
				PyErr_SetString(PyExc_ValueError, "only X, Y or Z for up axis");
				return NULL;
			}
		}
		else {
			PyErr_SetString(PyExc_ValueError, "only X, Y or Z for up axis");
			return NULL;
		}
	}

	if (track == up) {
		PyErr_SetString(PyExc_ValueError, "Can't have the same axis for track and up");
		return NULL;
	}

	/*
		flip vector around, since vectoquat expect a vector from target to tracking object 
		and the python function expects the inverse (a vector to the target).
	*/
	negate_v3_v3(vec, self->vec);

	vec_to_quat( quat,vec, track, up);

	return newQuaternionObject(quat, Py_NEW, NULL);
}

/*----------------------------Vector.reflect(mirror) ----------------------
  return a reflected vector on the mirror normal
   vec - ((2 * DotVecs(vec, mirror)) * mirror)
*/
static char Vector_Reflect_doc[] =
".. method:: reflect(mirror)\n"
"\n"
"   Return the reflection vector from the *mirror* argument.\n"
"\n"
"   :arg mirror: This vector could be a normal from the reflecting surface.\n"
"   :type mirror: :class:`Vector`\n"
"   :return: The reflected vector matching the size of this vector.\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Reflect(VectorObject *self, VectorObject *value )
{
	float mirror[3], vec[3];
	float reflect[3] = {0.0f, 0.0f, 0.0f};
	
	if (!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "vec.reflect(value): expected a vector argument");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;
	
	mirror[0] = value->vec[0];
	mirror[1] = value->vec[1];
	if (value->size > 2)	mirror[2] = value->vec[2];
	else					mirror[2] = 0.0;
	
	vec[0] = self->vec[0];
	vec[1] = self->vec[1];
	if (self->size > 2)		vec[2] = self->vec[2];
	else					vec[2] = 0.0;
	
	normalize_v3(mirror);
	reflect_v3_v3v3(reflect, vec, mirror);
	
	return newVectorObject(reflect, self->size, Py_NEW, Py_TYPE(self));
}

static char Vector_Cross_doc[] =
".. method:: cross(other)\n"
"\n"
"   Return the cross product of this vector and another.\n"
"\n"
"   :arg other: The other vector to perform the cross product with.\n"
"   :type other: :class:`Vector`\n"
"   :return: The cross product.\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. note:: both vectors must be 3D\n";

static PyObject *Vector_Cross(VectorObject *self, VectorObject *value )
{
	VectorObject *vecCross = NULL;

	if (!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "vec.cross(value): expected a vector argument");
		return NULL;
	}
	
	if(self->size != 3 || value->size != 3) {
		PyErr_SetString(PyExc_AttributeError, "vec.cross(value): expects both vectors to be 3D");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;
	
	vecCross = (VectorObject *)newVectorObject(NULL, 3, Py_NEW, Py_TYPE(self));
	cross_v3_v3v3(vecCross->vec, self->vec, value->vec);
	return (PyObject *)vecCross;
}

static char Vector_Dot_doc[] =
".. method:: dot(other)\n"
"\n"
"   Return the dot product of this vector and another.\n"
"\n"
"   :arg other: The other vector to perform the dot product with.\n"
"   :type other: :class:`Vector`\n"
"   :return: The dot product.\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Dot(VectorObject *self, VectorObject *value )
{
	double dot = 0.0;
	int x;
	
	if (!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "vec.dot(value): expected a vector argument");
		return NULL;
	}
	
	if(self->size != value->size) {
		PyErr_SetString(PyExc_AttributeError, "vec.dot(value): expects both vectors to have the same size");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;
	
	for(x = 0; x < self->size; x++) {
		dot += self->vec[x] * value->vec[x];
	}
	return PyFloat_FromDouble(dot);
}

static char Vector_angle_doc[] = 
".. function:: angle(other, fallback)\n"
"\n"
"   Return the angle between two vectors.\n"
"\n"
"   :arg other: another vector to compare the angle with\n"
"   :type other: :class:`Vector`\n"
"   :arg fallback: return this value when the angle cant be calculated (zero length vector)\n"
"   :type fallback: any\n"
"   :return: angle in radians or fallback when given\n"
"   :rtype: float\n"
"\n"
"   .. note:: Zero length vectors raise an :exc:`AttributeError`.\n";
static PyObject *Vector_angle(VectorObject *self, PyObject *args)
{
	VectorObject *value;
	double dot = 0.0f, angleRads, test_v1 = 0.0f, test_v2 = 0.0f;
	int x, size;
	PyObject *fallback= NULL;
	
	if(!PyArg_ParseTuple(args, "O!|O:angle", &vector_Type, &value, &fallback))
		return NULL;

	if (!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "vec.angle(value): expected a vector argument");
		return NULL;
	}
	
	if(self->size != value->size) {
		PyErr_SetString(PyExc_AttributeError, "vec.angle(value): expects both vectors to have the same size");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	//since size is the same
	size = self->size;

	for(x = 0; x < size; x++) {
		test_v1 += self->vec[x] * self->vec[x];
		test_v2 += value->vec[x] * value->vec[x];
	}
	if (!test_v1 || !test_v2){
		/* avoid exception */
		if(fallback) {
			Py_INCREF(fallback);
			return fallback;
		}
		else {
			PyErr_SetString(PyExc_ValueError, "vector.angle(other): zero length vectors have no valid angle");
			return NULL;
		}
	}

	//dot product
	for(x = 0; x < size; x++) {
		dot += self->vec[x] * value->vec[x];
	}
	dot /= (sqrt(test_v1) * sqrt(test_v2));

	angleRads = (double)saacos(dot);

	return PyFloat_FromDouble(angleRads);
}

static char Vector_Difference_doc[] =
".. function:: difference(other)\n"
"\n"
"   Returns a quaternion representing the rotational difference between this vector and another.\n"
"\n"
"   :arg other: second vector.\n"
"   :type other: :class:`Vector`\n"
"   :return: the rotational difference between the two vectors.\n"
"   :rtype: :class:`Quaternion`\n"
"\n"
"   .. note:: 2D vectors raise an :exc:`AttributeError`.\n";

static PyObject *Vector_Difference(VectorObject *self, VectorObject *value )
{
	float quat[4], vec_a[3], vec_b[3];

	if (!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "vec.difference(value): expected a vector argument");
		return NULL;
	}

	if(self->size < 3 || value->size < 3) {
		PyErr_SetString(PyExc_AttributeError, "vec.difference(value): expects both vectors to be size 3 or 4");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	normalize_v3_v3(vec_a, self->vec);
	normalize_v3_v3(vec_b, value->vec);

	rotation_between_vecs_to_quat(quat, vec_a, vec_b);

	return newQuaternionObject(quat, Py_NEW, NULL);
}

static char Vector_Project_doc[] =
".. function:: project(other)\n"
"\n"
"   Return the projection of this vector onto the *other*.\n"
"\n"
"   :arg other: second vector.\n"
"   :type other: :class:`Vector`\n"
"   :return: the parallel projection vector\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Project(VectorObject *self, VectorObject *value)
{
	float vec[4];
	double dot = 0.0f, dot2 = 0.0f;
	int x, size;

	if (!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "vec.project(value): expected a vector argument");
		return NULL;
	}

	if(self->size != value->size) {
		PyErr_SetString(PyExc_AttributeError, "vec.project(value): expects both vectors to have the same size");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;


	//since they are the same size
	size = self->size;

	//get dot products
	for(x = 0; x < size; x++) {
		dot += self->vec[x] * value->vec[x];
		dot2 += value->vec[x] * value->vec[x];
	}
	//projection
	dot /= dot2;
	for(x = 0; x < size; x++) {
		vec[x] = (float)(dot * value->vec[x]);
	}
	return newVectorObject(vec, size, Py_NEW, Py_TYPE(self));
}

static char Vector_Lerp_doc[] =
".. function:: lerp(other, factor)\n"
"\n"
"   Returns the interpolation of two vectors.\n"
"\n"
"   :arg other: value to interpolate with.\n"
"   :type other: :class:`Vector`\n"
"   :arg factor: The interpolation value in [0.0, 1.0].\n"
"   :type factor: float\n"
"   :return: The interpolated rotation.\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Lerp(VectorObject *self, PyObject *args)
{
	VectorObject *vec2 = NULL;
	float fac, ifac, vec[4];
	int x;

	if(!PyArg_ParseTuple(args, "O!f:lerp", &vector_Type, &vec2, &fac))
		return NULL;

	if(self->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "vector.lerp(): expects both vector objects to have the same size");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(vec2))
		return NULL;

	ifac= 1.0 - fac;

	for(x = 0; x < self->size; x++) {
		vec[x] = (ifac * self->vec[x]) + (fac * vec2->vec[x]);
	}
	return newVectorObject(vec, self->size, Py_NEW, Py_TYPE(self));
}

/*---------------------------- Vector.rotate(angle, axis) ----------------------*/
static char Vector_Rotate_doc[] =
".. function:: rotate(axis, angle)\n"
"\n"
"   Return vector rotated around axis by angle.\n"
"\n"
"   :arg axis: rotation axis.\n"
"   :type axis: :class:`Vector`\n"
"   :arg angle: angle in radians.\n"
"   :type angle: float\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Rotate(VectorObject *self, PyObject *args)
{
	VectorObject *axis_vec = NULL;
	float angle, vec[3];

	if(!PyArg_ParseTuple(args, "O!f", &vector_Type, &axis_vec, &angle)){
		PyErr_SetString(PyExc_TypeError, "vec.rotate(axis, angle): expected 3D axis (Vector) and angle (float)");
		return NULL;
	}

	if(self->size != 3 || axis_vec->size != 3) {
		PyErr_SetString(PyExc_AttributeError, "vec.rotate(axis, angle): expects both vectors to be 3D");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(axis_vec))
		return NULL;

	rotate_v3_v3v3fl(vec, self->vec, axis_vec->vec, angle);

	copy_v3_v3(self->vec, vec);

	Py_INCREF(self);
	return (PyObject *)self;
}

/*----------------------------Vector.copy() -------------------------------------- */
static char Vector_copy_doc[] =
".. function:: copy()\n"
"\n"
"   Returns a copy of this vector.\n"
"\n"
"   :return: A copy of the vector.\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. note:: use this to get a copy of a wrapped vector with no reference to the original data.\n";

static PyObject *Vector_copy(VectorObject *self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	return newVectorObject(self->vec, self->size, Py_NEW, Py_TYPE(self));
}

/*----------------------------print object (internal)-------------
  print the object to screen */
static PyObject *Vector_repr(VectorObject *self)
{
	PyObject *ret, *tuple;

	if(!BaseMath_ReadCallback(self))
		return NULL;

	tuple= Vector_ToTupleExt(self, -1);
	ret= PyUnicode_FromFormat("Vector(%R)", tuple);
	Py_DECREF(tuple);
	return ret;
}

/*---------------------SEQUENCE PROTOCOLS------------------------
  ----------------------------len(object)------------------------
  sequence length*/
static int Vector_len(VectorObject *self)
{
	return self->size;
}
/*----------------------------object[]---------------------------
  sequence accessor (get)*/
static PyObject *Vector_item(VectorObject *self, int i)
{
	if(i<0)	i= self->size-i;

	if(i < 0 || i >= self->size) {
		PyErr_SetString(PyExc_IndexError,"vector[index]: out of range");
		return NULL;
	}

	if(!BaseMath_ReadIndexCallback(self, i))
		return NULL;
	
	return PyFloat_FromDouble(self->vec[i]);

}
/*----------------------------object[]-------------------------
  sequence accessor (set)*/
static int Vector_ass_item(VectorObject *self, int i, PyObject * ob)
{
	float scalar;
	if((scalar=PyFloat_AsDouble(ob))==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "vector[index] = x: index argument not a number");
		return -1;
	}

	if(i<0)	i= self->size-i;

	if(i < 0 || i >= self->size){
		PyErr_SetString(PyExc_IndexError, "vector[index] = x: assignment index out of range");
		return -1;
	}
	self->vec[i] = scalar;
	
	if(!BaseMath_WriteIndexCallback(self, i))
		return -1;
	return 0;
}

/*----------------------------object[z:y]------------------------
  sequence slice (get) */
static PyObject *Vector_slice(VectorObject *self, int begin, int end)
{
	PyObject *tuple;
	int count;

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	CLAMP(begin, 0, self->size);
	if (end<0) end= self->size+end+1;
	CLAMP(end, 0, self->size);
	begin= MIN2(begin, end);

	tuple= PyTuple_New(end - begin);
	for(count = begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->vec[count]));
	}

	return tuple;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set) */
static int Vector_ass_slice(VectorObject *self, int begin, int end,
				 PyObject * seq)
{
	int i, y, size = 0;
	float vec[4], scalar;
	PyObject *v;

	if(!BaseMath_ReadCallback(self))
		return -1;
	
	CLAMP(begin, 0, self->size);
	if (end<0) end= self->size+end+1;
	CLAMP(end, 0, self->size);
	begin = MIN2(begin,end);

	size = PySequence_Size(seq);
	if(size != (end - begin)){
		PyErr_SetString(PyExc_TypeError, "vector[begin:end] = []: size mismatch in slice assignment");
		return -1;
	}

	for (i = 0; i < size; i++) {
		v = PySequence_GetItem(seq, i);
		if (v == NULL) { /* Failed to read sequence */
			PyErr_SetString(PyExc_RuntimeError, "vector[begin:end] = []: unable to read sequence");
			return -1;
		}
		
		if((scalar=PyFloat_AsDouble(v)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
			Py_DECREF(v);
			PyErr_SetString(PyExc_TypeError, "vector[begin:end] = []: sequence argument not a number");
			return -1;
		}

		vec[i] = scalar;
		Py_DECREF(v);
	}
	/*parsed well - now set in vector*/
	for(y = 0; y < size; y++){
		self->vec[begin + y] = vec[y];
	}
	
	if(!BaseMath_WriteCallback(self))
		return -1;
	
	return 0;
}
/*------------------------NUMERIC PROTOCOLS----------------------
  ------------------------obj + obj------------------------------
  addition*/
static PyObject *Vector_add(PyObject * v1, PyObject * v2)
{
	int i;
	float vec[4];

	VectorObject *vec1 = NULL, *vec2 = NULL;
	
	if VectorObject_Check(v1)
		vec1= (VectorObject *)v1;
	
	if VectorObject_Check(v2)
		vec2= (VectorObject *)v2;
	
	/* make sure v1 is always the vector */
	if (vec1 && vec2 ) {
		
		if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
			return NULL;
		
		/*VECTOR + VECTOR*/
		if(vec1->size != vec2->size) {
			PyErr_SetString(PyExc_AttributeError, "Vector addition: vectors must have the same dimensions for this operation");
			return NULL;
		}
		for(i = 0; i < vec1->size; i++) {
			vec[i] = vec1->vec[i] +	vec2->vec[i];
		}
		return newVectorObject(vec, vec1->size, Py_NEW, Py_TYPE(v1));
	}
	
	PyErr_SetString(PyExc_AttributeError, "Vector addition: arguments not valid for this operation");
	return NULL;
}

/*  ------------------------obj += obj------------------------------
  addition in place */
static PyObject *Vector_iadd(PyObject * v1, PyObject * v2)
{
	int i;
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError, "Vector addition: arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Vector addition: vectors must have the same dimensions for this operation");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;

	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] = vec1->vec[i] + vec2->vec[i];
	}

	(void)BaseMath_WriteCallback(vec1);
	Py_INCREF( v1 );
	return v1;
}

/*------------------------obj - obj------------------------------
  subtraction*/
static PyObject *Vector_sub(PyObject * v1, PyObject * v2)
{
	int i;
	float vec[4];
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;
	
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: vectors must have the same dimensions for this operation");
		return NULL;
	}
	for(i = 0; i < vec1->size; i++) {
		vec[i] = vec1->vec[i] -	vec2->vec[i];
	}

	return newVectorObject(vec, vec1->size, Py_NEW, Py_TYPE(v1));
}

/*------------------------obj -= obj------------------------------
  subtraction*/
static PyObject *Vector_isub(PyObject * v1, PyObject * v2)
{
	int i;
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: vectors must have the same dimensions for this operation");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;

	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] = vec1->vec[i] -	vec2->vec[i];
	}

	(void)BaseMath_WriteCallback(vec1);
	Py_INCREF( v1 );
	return v1;
}

/*------------------------obj * obj------------------------------
  mulplication*/


/* COLUMN VECTOR Multiplication (Vector X Matrix)
 * [a] * [1][4][7]
 * [b] * [2][5][8]
 * [c] * [3][6][9]
 *
 * note: vector/matrix multiplication IS NOT COMMUTATIVE!!!!
 * note: assume read callbacks have been done first.
 */
static int column_vector_multiplication(float *rvec, VectorObject* vec, MatrixObject * mat)
{
	float vecCopy[4];
	double dot = 0.0f;
	int x, y, z = 0;
	
	if(mat->rowSize != vec->size){
		if(mat->rowSize == 4 && vec->size != 3){
			PyErr_SetString(PyExc_AttributeError, "matrix * vector: matrix row size and vector size must be the same");
			return -1;
		}else{
			vecCopy[3] = 1.0f;
		}
	}

	for(x = 0; x < vec->size; x++){
		vecCopy[x] = vec->vec[x];
	}
	rvec[3] = 1.0f;

	for(x = 0; x < mat->colSize; x++) {
		for(y = 0; y < mat->rowSize; y++) {
			dot += mat->matrix[y][x] * vecCopy[y];
		}
		rvec[z++] = (float)dot;
		dot = 0.0f;
	}
	
	return 0;
}

static PyObject *vector_mul_float(VectorObject *vec, const float scalar)
{
	float tvec[MAX_DIMENSIONS];
	int i;

	for(i = 0; i < vec->size; i++) {
		tvec[i] = vec->vec[i] * scalar;
	}
	return newVectorObject(tvec, vec->size, Py_NEW, Py_TYPE(vec));
}

static PyObject *Vector_mul(PyObject * v1, PyObject * v2)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float scalar;
	
	if VectorObject_Check(v1) {
		vec1= (VectorObject *)v1;
		if(!BaseMath_ReadCallback(vec1))
			return NULL;
	}
	if VectorObject_Check(v2) {
		vec2= (VectorObject *)v2;
		if(!BaseMath_ReadCallback(vec2))
			return NULL;
	}
	
	
	/* make sure v1 is always the vector */
	if (vec1 && vec2 ) {
		int i;
		double dot = 0.0f;
		
		if(vec1->size != vec2->size) {
			PyErr_SetString(PyExc_AttributeError, "Vector multiplication: vectors must have the same dimensions for this operation");
			return NULL;
		}
		
		/*dot product*/
		for(i = 0; i < vec1->size; i++) {
			dot += vec1->vec[i] * vec2->vec[i];
		}
		return PyFloat_FromDouble(dot);
	}
	else if (vec1) {
		if (MatrixObject_Check(v2)) {
			/* VEC * MATRIX */
			float tvec[MAX_DIMENSIONS];
			if(!BaseMath_ReadCallback((MatrixObject *)v2))
				return NULL;
			if(column_vector_multiplication(tvec, vec1, (MatrixObject*)v2) == -1) {
				return NULL;
			}

			return newVectorObject(tvec, vec1->size, Py_NEW, Py_TYPE(vec1));
		}
		else if (QuaternionObject_Check(v2)) {
			/* VEC * QUAT */
			QuaternionObject *quat2 = (QuaternionObject*)v2;
			float tvec[3];

			if(vec1->size != 3) {
				PyErr_SetString(PyExc_TypeError, "Vector multiplication: only 3D vector rotations (with quats) currently supported");
				return NULL;
			}
			if(!BaseMath_ReadCallback(quat2)) {
				return NULL;
			}
			copy_v3_v3(tvec, vec1->vec);
			mul_qt_v3(quat2->quat, tvec);
			return newVectorObject(tvec, 3, Py_NEW, Py_TYPE(vec1));
		}
		else if (((scalar= PyFloat_AsDouble(v2)) == -1.0 && PyErr_Occurred())==0) { /* VEC*FLOAT */
			return vector_mul_float(vec1, scalar);
		}
	}
	else if (vec2) {
		if (((scalar= PyFloat_AsDouble(v1)) == -1.0 && PyErr_Occurred())==0) { /* VEC*FLOAT */
			return vector_mul_float(vec2, scalar);
		}
	}
	else {
		BKE_assert(!"internal error");
	}

	PyErr_Format(PyExc_TypeError, "Vector multiplication: not supported between '%.200s' and '%.200s' types", Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
	return NULL;
}

/*------------------------obj *= obj------------------------------
  in place mulplication */
static PyObject *Vector_imul(PyObject * v1, PyObject * v2)
{
	VectorObject *vec = (VectorObject *)v1;
	float scalar;
	
	if(!BaseMath_ReadCallback(vec))
		return NULL;
	
	/* only support vec*=float and vec*=mat
	   vec*=vec result is a float so that wont work */
	if (MatrixObject_Check(v2)) {
		float rvec[MAX_DIMENSIONS];
		if(!BaseMath_ReadCallback((MatrixObject *)v2))
			return NULL;
		
		if(column_vector_multiplication(rvec, vec, (MatrixObject*)v2) == -1)
			return NULL;

		memcpy(vec->vec, rvec, sizeof(float) * vec->size);
	}
	else if (QuaternionObject_Check(v2)) {
		/* VEC *= QUAT */
		QuaternionObject *quat2 = (QuaternionObject*)v2;

		if(vec->size != 3) {
			PyErr_SetString(PyExc_TypeError, "Vector multiplication: only 3D vector rotations (with quats) currently supported");
			return NULL;
		}

		if(!BaseMath_ReadCallback(quat2)) {
			return NULL;
		}
		mul_qt_v3(quat2->quat, vec->vec);
	}
	else if (((scalar= PyFloat_AsDouble(v2)) == -1.0 && PyErr_Occurred())==0) { /* VEC*=FLOAT */
		mul_vn_fl(vec->vec, vec->size, scalar);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "Vector multiplication: arguments not acceptable for this operation");
		return NULL;
	}
	
	(void)BaseMath_WriteCallback(vec);
	Py_INCREF( v1 );
	return v1;
}

/*------------------------obj / obj------------------------------
  divide*/
static PyObject *Vector_div(PyObject * v1, PyObject * v2)
{
	int i;
	float vec[4], scalar;
	VectorObject *vec1 = NULL;
	
	if(!VectorObject_Check(v1)) { /* not a vector */
		PyErr_SetString(PyExc_TypeError, "Vector division: Vector must be divided by a float");
		return NULL;
	}
	vec1 = (VectorObject*)v1; /* vector */
	
	if(!BaseMath_ReadCallback(vec1))
		return NULL;

	if((scalar=PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "Vector division: Vector must be divided by a float");
		return NULL;
	}
	
	if(scalar==0.0) {
		PyErr_SetString(PyExc_ZeroDivisionError, "Vector division: divide by zero error");
		return NULL;
	}
	
	for(i = 0; i < vec1->size; i++) {
		vec[i] = vec1->vec[i] /	scalar;
	}
	return newVectorObject(vec, vec1->size, Py_NEW, Py_TYPE(v1));
}

/*------------------------obj /= obj------------------------------
  divide*/
static PyObject *Vector_idiv(PyObject * v1, PyObject * v2)
{
	int i;
	float scalar;
	VectorObject *vec1 = (VectorObject*)v1;
	
	if(!BaseMath_ReadCallback(vec1))
		return NULL;

	if((scalar=PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "Vector division: Vector must be divided by a float");
		return NULL;
	}

	if(scalar==0.0) {
		PyErr_SetString(PyExc_ZeroDivisionError, "Vector division: divide by zero error");
		return NULL;
	}
	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] /=	scalar;
	}
	
	(void)BaseMath_WriteCallback(vec1);
	
	Py_INCREF( v1 );
	return v1;
}

/*-------------------------- -obj -------------------------------
  returns the negative of this object*/
static PyObject *Vector_neg(VectorObject *self)
{
	int i;
	float vec[4];
	
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	for(i = 0; i < self->size; i++){
		vec[i] = -self->vec[i];
	}

	return newVectorObject(vec, self->size, Py_NEW, Py_TYPE(self));
}

/*------------------------vec_magnitude_nosqrt (internal) - for comparing only */
static double vec_magnitude_nosqrt(float *data, int size)
{
	double dot = 0.0f;
	int i;

	for(i=0; i<size; i++){
		dot += data[i];
	}
	/*return (double)sqrt(dot);*/
	/* warning, line above removed because we are not using the length,
	   rather the comparing the sizes and for this we do not need the sqrt
	   for the actual length, the dot must be sqrt'd */
	return (double)dot;
}


/*------------------------tp_richcmpr
  returns -1 execption, 0 false, 1 true */
static PyObject* Vector_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	VectorObject *vecA = NULL, *vecB = NULL;
	int result = 0;
	float epsilon = .000001f;
	double lenA,lenB;

	if (!VectorObject_Check(objectA) || !VectorObject_Check(objectB)){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}
	vecA = (VectorObject*)objectA;
	vecB = (VectorObject*)objectB;

	if(!BaseMath_ReadCallback(vecA) || !BaseMath_ReadCallback(vecB))
		return NULL;
	
	if (vecA->size != vecB->size){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}

	switch (comparison_type){
		case Py_LT:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if( lenA < lenB ){
				result = 1;
			}
			break;
		case Py_LE:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if( lenA < lenB ){
				result = 1;
			}else{
				result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
			}
			break;
		case Py_EQ:
			result = EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->size, 1);
			break;
		case Py_NE:
			result = !EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->size, 1);
			break;
		case Py_GT:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if( lenA > lenB ){
				result = 1;
			}
			break;
		case Py_GE:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if( lenA > lenB ){
				result = 1;
			}else{
				result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
			}
			break;
		default:
			printf("The result of the comparison could not be evaluated");
			break;
	}
	if (result == 1){
		Py_RETURN_TRUE;
	}else{
		Py_RETURN_FALSE;
	}
}

/*-----------------PROTCOL DECLARATIONS--------------------------*/
static PySequenceMethods Vector_SeqMethods = {
	(lenfunc) Vector_len,				/* sq_length */
	(binaryfunc) 0,						/* sq_concat */
	(ssizeargfunc) 0,					/* sq_repeat */
	(ssizeargfunc) Vector_item,			/* sq_item */
	NULL,								/* py3 deprecated slice func */
	(ssizeobjargproc) Vector_ass_item,	/* sq_ass_item */
	NULL,								/* py3 deprecated slice assign func */
	(objobjproc) NULL,					/* sq_contains */
	(binaryfunc) NULL,					/* sq_inplace_concat */
	(ssizeargfunc) NULL,				/* sq_inplace_repeat */
};

static PyObject *Vector_subscript(VectorObject* self, PyObject* item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += self->size;
		return Vector_item(self, i);
	} else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((PySliceObject*)item, self->size, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyList_New(0);
		}
		else if (step == 1) {
			return Vector_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with vectors");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "vector indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return NULL;
	}
}

static int Vector_ass_subscript(VectorObject* self, PyObject* item, PyObject* value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += self->size;
		return Vector_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((PySliceObject*)item, self->size, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Vector_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with vectors");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "vector indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return -1;
	}
}

static PyMappingMethods Vector_AsMapping = {
	(lenfunc)Vector_len,
	(binaryfunc)Vector_subscript,
	(objobjargproc)Vector_ass_subscript
};


static PyNumberMethods Vector_NumMethods = {
		(binaryfunc)	Vector_add,	/*nb_add*/
		(binaryfunc)	Vector_sub,	/*nb_subtract*/
		(binaryfunc)	Vector_mul,	/*nb_multiply*/
		0,							/*nb_remainder*/
		0,							/*nb_divmod*/
		0,							/*nb_power*/
		(unaryfunc) 	Vector_neg,	/*nb_negative*/
		(unaryfunc) 	0,	/*tp_positive*/
		(unaryfunc) 	0,	/*tp_absolute*/
		(inquiry)	0,	/*tp_bool*/
		(unaryfunc)	0,	/*nb_invert*/
		0,				/*nb_lshift*/
		(binaryfunc)0,	/*nb_rshift*/
		0,				/*nb_and*/
		0,				/*nb_xor*/
		0,				/*nb_or*/
		0,				/*nb_int*/
		0,				/*nb_reserved*/
		0,				/*nb_float*/
		Vector_iadd,	/* nb_inplace_add */
		Vector_isub,	/* nb_inplace_subtract */
		Vector_imul,	/* nb_inplace_multiply */
		0,				/* nb_inplace_remainder */
		0,				/* nb_inplace_power */
		0,				/* nb_inplace_lshift */
		0,				/* nb_inplace_rshift */
		0,				/* nb_inplace_and */
		0,				/* nb_inplace_xor */
		0,				/* nb_inplace_or */
		0,				/* nb_floor_divide */
		Vector_div,		/* nb_true_divide */
		0,				/* nb_inplace_floor_divide */
		Vector_idiv,	/* nb_inplace_true_divide */
		0,			/* nb_index */
};

/*------------------PY_OBECT DEFINITION--------------------------*/

/*
 * vector axis, vector.x/y/z/w
 */
	
static PyObject *Vector_getAxis(VectorObject *self, void *type )
{
	return Vector_item(self, GET_INT_FROM_POINTER(type));
}

static int Vector_setAxis(VectorObject *self, PyObject * value, void * type )
{
	return Vector_ass_item(self, GET_INT_FROM_POINTER(type), value);
}

/* vector.length */
static PyObject *Vector_getLength(VectorObject *self, void *UNUSED(closure))
{
	double dot = 0.0f;
	int i;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	for(i = 0; i < self->size; i++){
		dot += (self->vec[i] * self->vec[i]);
	}
	return PyFloat_FromDouble(sqrt(dot));
}

static int Vector_setLength(VectorObject *self, PyObject * value )
{
	double dot = 0.0f, param;
	int i;
	
	if(!BaseMath_ReadCallback(self))
		return -1;

	if((param=PyFloat_AsDouble(value)) == -1.0 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "length must be set to a number");
		return -1;
	}
	
	if (param < 0.0f) {
		PyErr_SetString(PyExc_TypeError, "cannot set a vectors length to a negative value");
		return -1;
	}
	if (param == 0.0f) {
		for(i = 0; i < self->size; i++){
			self->vec[i]= 0;
		}
		return 0;
	}
	
	for(i = 0; i < self->size; i++){
		dot += (self->vec[i] * self->vec[i]);
	}

	if (!dot) /* cant sqrt zero */
		return 0;
	
	dot = sqrt(dot);
	
	if (dot==param)
		return 0;
	
	dot= dot/param;
	
	for(i = 0; i < self->size; i++){
		self->vec[i]= self->vec[i] / (float)dot;
	}
	
	(void)BaseMath_WriteCallback(self); /* checked already */
	
	return 0;
}

/* Get a new Vector according to the provided swizzle. This function has little
   error checking, as we are in control of the inputs: the closure is set by us
   in Vector_createSwizzleGetSeter. */
static PyObject *Vector_getSwizzle(VectorObject *self, void *closure)
{
	size_t axis_to;
	size_t axis_from;
	float vec[MAX_DIMENSIONS];
	unsigned int swizzleClosure;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	/* Unpack the axes from the closure into an array. */
	axis_to = 0;
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axis_from = swizzleClosure & SWIZZLE_AXIS;
		if(axis_from >= self->size) {
			PyErr_SetString(PyExc_AttributeError, "Error: vector does not have specified axis");
			return NULL;
		}

		vec[axis_to] = self->vec[axis_from];
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axis_to++;
	}
	
	return newVectorObject(vec, axis_to, Py_NEW, Py_TYPE(self));
}

/* Set the items of this vector using a swizzle.
   - If value is a vector or list this operates like an array copy, except that
	 the destination is effectively re-ordered as defined by the swizzle. At
	 most min(len(source), len(dest)) values will be copied.
   - If the value is scalar, it is copied to all axes listed in the swizzle.
   - If an axis appears more than once in the swizzle, the final occurrence is
	 the one that determines its value.

   Returns 0 on success and -1 on failure. On failure, the vector will be
   unchanged. */
static int Vector_setSwizzle(VectorObject *self, PyObject * value, void *closure)
{
	size_t size_from;
	float scalarVal;

	size_t axis_from;
	size_t axis_to;

	unsigned int swizzleClosure;
	
	float tvec[MAX_DIMENSIONS];
	float vec_assign[MAX_DIMENSIONS];
	
	if(!BaseMath_ReadCallback(self))
		return -1;
	
	/* Check that the closure can be used with this vector: even 2D vectors have
	   swizzles defined for axes z and w, but they would be invalid. */
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	axis_from= 0;
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axis_to = swizzleClosure & SWIZZLE_AXIS;
		if (axis_to >= self->size)
		{
			PyErr_SetString(PyExc_AttributeError, "Error: vector does not have specified axis");
			return -1;
		}
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axis_from++;
	}

	if (((scalarVal=PyFloat_AsDouble(value)) == -1 && PyErr_Occurred())==0) {
		int i;
		for(i=0; i < MAX_DIMENSIONS; i++)
			vec_assign[i]= scalarVal;

		size_from= axis_from;
	}
	else if((size_from=mathutils_array_parse(vec_assign, 2, 4, value, "mathutils.Vector.**** = swizzle assignment")) == -1) {
		return -1;
	}

	if(axis_from != size_from) {
		PyErr_SetString(PyExc_AttributeError, "Error: vector size does not match swizzle");
		return -1;
	}

	/* Copy vector contents onto swizzled axes. */
	axis_from = 0;
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axis_to = swizzleClosure & SWIZZLE_AXIS;
		tvec[axis_to] = vec_assign[axis_from];
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axis_from++;
	}

	memcpy(self->vec, tvec, axis_from * sizeof(float));
	/* continue with BaseMathObject_WriteCallback at the end */
	
	if(!BaseMath_WriteCallback(self))
		return -1;
	else
		return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Vector_getseters[] = {
	{(char *)"x", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector X axis.\n\n:type: float", (void *)0},
	{(char *)"y", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector Y axis.\n\n:type: float", (void *)1},
	{(char *)"z", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector Z axis (3D Vectors only).\n\n:type: float", (void *)2},
	{(char *)"w", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector W axis (4D Vectors only).\n\n:type: float", (void *)3},
	{(char *)"length", (getter)Vector_getLength, (setter)Vector_setLength, (char *)"Vector Length.\n\n:type: float", NULL},
	{(char *)"magnitude", (getter)Vector_getLength, (setter)Vector_setLength, (char *)"Vector Length.\n\n:type: float", NULL},
	{(char *)"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, (char *)BaseMathObject_Wrapped_doc, NULL},
	{(char *)"owner", (getter)BaseMathObject_getOwner, (setter)NULL, (char *)BaseMathObject_Owner_doc, NULL},
	
	/* autogenerated swizzle attrs, see python script below */
	{(char *)"xx",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 36
	{(char *)"xxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 292
	{(char *)"xxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2340
	{(char *)"xxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2852
	{(char *)"xxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3364
	{(char *)"xxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3876
	{(char *)"xxy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 356
	{(char *)"xxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2404
	{(char *)"xxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2916
	{(char *)"xxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3428
	{(char *)"xxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3940
	{(char *)"xxz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 420
	{(char *)"xxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2468
	{(char *)"xxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2980
	{(char *)"xxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3492
	{(char *)"xxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4004
	{(char *)"xxw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 484
	{(char *)"xxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2532
	{(char *)"xxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3044
	{(char *)"xxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3556
	{(char *)"xxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4068
	{(char *)"xy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 44
	{(char *)"xyx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 300
	{(char *)"xyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2348
	{(char *)"xyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2860
	{(char *)"xyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3372
	{(char *)"xyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3884
	{(char *)"xyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 364
	{(char *)"xyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2412
	{(char *)"xyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2924
	{(char *)"xyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3436
	{(char *)"xyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3948
	{(char *)"xyz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 428
	{(char *)"xyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2476
	{(char *)"xyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2988
	{(char *)"xyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3500
	{(char *)"xyzw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4012
	{(char *)"xyw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 492
	{(char *)"xywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2540
	{(char *)"xywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3052
	{(char *)"xywz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3564
	{(char *)"xyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4076
	{(char *)"xz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 52
	{(char *)"xzx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 308
	{(char *)"xzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2356
	{(char *)"xzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2868
	{(char *)"xzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3380
	{(char *)"xzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3892
	{(char *)"xzy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 372
	{(char *)"xzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2420
	{(char *)"xzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2932
	{(char *)"xzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3444
	{(char *)"xzyw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3956
	{(char *)"xzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 436
	{(char *)"xzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2484
	{(char *)"xzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2996
	{(char *)"xzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3508
	{(char *)"xzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4020
	{(char *)"xzw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 500
	{(char *)"xzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2548
	{(char *)"xzwy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3060
	{(char *)"xzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3572
	{(char *)"xzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4084
	{(char *)"xw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 60
	{(char *)"xwx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 316
	{(char *)"xwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2364
	{(char *)"xwxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2876
	{(char *)"xwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3388
	{(char *)"xwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3900
	{(char *)"xwy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 380
	{(char *)"xwyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2428
	{(char *)"xwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2940
	{(char *)"xwyz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3452
	{(char *)"xwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3964
	{(char *)"xwz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 444
	{(char *)"xwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2492
	{(char *)"xwzy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3004
	{(char *)"xwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3516
	{(char *)"xwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4028
	{(char *)"xww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 508
	{(char *)"xwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2556
	{(char *)"xwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3068
	{(char *)"xwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3580
	{(char *)"xwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4092
	{(char *)"yx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 37
	{(char *)"yxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 293
	{(char *)"yxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2341
	{(char *)"yxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2853
	{(char *)"yxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3365
	{(char *)"yxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3877
	{(char *)"yxy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 357
	{(char *)"yxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2405
	{(char *)"yxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2917
	{(char *)"yxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3429
	{(char *)"yxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3941
	{(char *)"yxz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 421
	{(char *)"yxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2469
	{(char *)"yxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2981
	{(char *)"yxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3493
	{(char *)"yxzw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4005
	{(char *)"yxw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 485
	{(char *)"yxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2533
	{(char *)"yxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3045
	{(char *)"yxwz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3557
	{(char *)"yxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4069
	{(char *)"yy",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 45
	{(char *)"yyx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 301
	{(char *)"yyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2349
	{(char *)"yyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2861
	{(char *)"yyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3373
	{(char *)"yyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3885
	{(char *)"yyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 365
	{(char *)"yyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2413
	{(char *)"yyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2925
	{(char *)"yyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3437
	{(char *)"yyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3949
	{(char *)"yyz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 429
	{(char *)"yyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2477
	{(char *)"yyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2989
	{(char *)"yyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3501
	{(char *)"yyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4013
	{(char *)"yyw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 493
	{(char *)"yywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2541
	{(char *)"yywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3053
	{(char *)"yywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3565
	{(char *)"yyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4077
	{(char *)"yz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 53
	{(char *)"yzx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 309
	{(char *)"yzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2357
	{(char *)"yzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2869
	{(char *)"yzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3381
	{(char *)"yzxw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3893
	{(char *)"yzy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 373
	{(char *)"yzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2421
	{(char *)"yzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2933
	{(char *)"yzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3445
	{(char *)"yzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3957
	{(char *)"yzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 437
	{(char *)"yzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2485
	{(char *)"yzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2997
	{(char *)"yzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3509
	{(char *)"yzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4021
	{(char *)"yzw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 501
	{(char *)"yzwx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2549
	{(char *)"yzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3061
	{(char *)"yzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3573
	{(char *)"yzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4085
	{(char *)"yw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 61
	{(char *)"ywx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 317
	{(char *)"ywxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2365
	{(char *)"ywxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2877
	{(char *)"ywxz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3389
	{(char *)"ywxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3901
	{(char *)"ywy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 381
	{(char *)"ywyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2429
	{(char *)"ywyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2941
	{(char *)"ywyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3453
	{(char *)"ywyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3965
	{(char *)"ywz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 445
	{(char *)"ywzx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2493
	{(char *)"ywzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3005
	{(char *)"ywzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3517
	{(char *)"ywzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4029
	{(char *)"yww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 509
	{(char *)"ywwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2557
	{(char *)"ywwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3069
	{(char *)"ywwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3581
	{(char *)"ywww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4093
	{(char *)"zx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 38
	{(char *)"zxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 294
	{(char *)"zxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2342
	{(char *)"zxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2854
	{(char *)"zxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3366
	{(char *)"zxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3878
	{(char *)"zxy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 358
	{(char *)"zxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2406
	{(char *)"zxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2918
	{(char *)"zxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3430
	{(char *)"zxyw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3942
	{(char *)"zxz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 422
	{(char *)"zxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2470
	{(char *)"zxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2982
	{(char *)"zxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3494
	{(char *)"zxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4006
	{(char *)"zxw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 486
	{(char *)"zxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2534
	{(char *)"zxwy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3046
	{(char *)"zxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3558
	{(char *)"zxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4070
	{(char *)"zy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 46
	{(char *)"zyx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 302
	{(char *)"zyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2350
	{(char *)"zyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2862
	{(char *)"zyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3374
	{(char *)"zyxw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3886
	{(char *)"zyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 366
	{(char *)"zyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2414
	{(char *)"zyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2926
	{(char *)"zyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3438
	{(char *)"zyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3950
	{(char *)"zyz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 430
	{(char *)"zyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2478
	{(char *)"zyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2990
	{(char *)"zyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3502
	{(char *)"zyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4014
	{(char *)"zyw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 494
	{(char *)"zywx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2542
	{(char *)"zywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3054
	{(char *)"zywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3566
	{(char *)"zyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4078
	{(char *)"zz",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 54
	{(char *)"zzx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 310
	{(char *)"zzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2358
	{(char *)"zzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2870
	{(char *)"zzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3382
	{(char *)"zzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3894
	{(char *)"zzy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 374
	{(char *)"zzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2422
	{(char *)"zzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2934
	{(char *)"zzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3446
	{(char *)"zzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3958
	{(char *)"zzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 438
	{(char *)"zzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2486
	{(char *)"zzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2998
	{(char *)"zzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3510
	{(char *)"zzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4022
	{(char *)"zzw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 502
	{(char *)"zzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2550
	{(char *)"zzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3062
	{(char *)"zzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3574
	{(char *)"zzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4086
	{(char *)"zw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 62
	{(char *)"zwx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 318
	{(char *)"zwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2366
	{(char *)"zwxy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2878
	{(char *)"zwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3390
	{(char *)"zwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3902
	{(char *)"zwy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 382
	{(char *)"zwyx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2430
	{(char *)"zwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2942
	{(char *)"zwyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3454
	{(char *)"zwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3966
	{(char *)"zwz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 446
	{(char *)"zwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2494
	{(char *)"zwzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3006
	{(char *)"zwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3518
	{(char *)"zwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4030
	{(char *)"zww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 510
	{(char *)"zwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2558
	{(char *)"zwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3070
	{(char *)"zwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3582
	{(char *)"zwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4094
	{(char *)"wx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 39
	{(char *)"wxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 295
	{(char *)"wxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2343
	{(char *)"wxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2855
	{(char *)"wxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3367
	{(char *)"wxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3879
	{(char *)"wxy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 359
	{(char *)"wxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2407
	{(char *)"wxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2919
	{(char *)"wxyz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3431
	{(char *)"wxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3943
	{(char *)"wxz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 423
	{(char *)"wxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2471
	{(char *)"wxzy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2983
	{(char *)"wxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3495
	{(char *)"wxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4007
	{(char *)"wxw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 487
	{(char *)"wxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2535
	{(char *)"wxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3047
	{(char *)"wxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3559
	{(char *)"wxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4071
	{(char *)"wy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 47
	{(char *)"wyx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 303
	{(char *)"wyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2351
	{(char *)"wyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2863
	{(char *)"wyxz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3375
	{(char *)"wyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3887
	{(char *)"wyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 367
	{(char *)"wyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2415
	{(char *)"wyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2927
	{(char *)"wyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3439
	{(char *)"wyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3951
	{(char *)"wyz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 431
	{(char *)"wyzx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2479
	{(char *)"wyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2991
	{(char *)"wyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3503
	{(char *)"wyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4015
	{(char *)"wyw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 495
	{(char *)"wywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2543
	{(char *)"wywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3055
	{(char *)"wywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3567
	{(char *)"wyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4079
	{(char *)"wz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 55
	{(char *)"wzx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 311
	{(char *)"wzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2359
	{(char *)"wzxy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2871
	{(char *)"wzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3383
	{(char *)"wzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3895
	{(char *)"wzy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 375
	{(char *)"wzyx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2423
	{(char *)"wzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2935
	{(char *)"wzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3447
	{(char *)"wzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3959
	{(char *)"wzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 439
	{(char *)"wzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2487
	{(char *)"wzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2999
	{(char *)"wzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3511
	{(char *)"wzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4023
	{(char *)"wzw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 503
	{(char *)"wzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2551
	{(char *)"wzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3063
	{(char *)"wzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3575
	{(char *)"wzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4087
	{(char *)"ww",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 63
	{(char *)"wwx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 319
	{(char *)"wwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2367
	{(char *)"wwxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2879
	{(char *)"wwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3391
	{(char *)"wwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3903
	{(char *)"wwy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 383
	{(char *)"wwyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2431
	{(char *)"wwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2943
	{(char *)"wwyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3455
	{(char *)"wwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3967
	{(char *)"wwz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 447
	{(char *)"wwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2495
	{(char *)"wwzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3007
	{(char *)"wwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3519
	{(char *)"wwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4031
	{(char *)"www",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 511
	{(char *)"wwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2559
	{(char *)"wwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3071
	{(char *)"wwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3583
	{(char *)"wwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4095
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/* Python script used to make swizzle array */
/*
SWIZZLE_BITS_PER_AXIS = 3
SWIZZLE_VALID_AXIS = 0x4

axis_dict = {}
axis_pos = {'x':0, 'y':1, 'z':2, 'w':3}
axises = 'xyzw'
while len(axises) >= 2:
	
	for axis_0 in axises:
		axis_0_pos = axis_pos[axis_0]
		for axis_1 in axises:
			axis_1_pos = axis_pos[axis_1]
			axis_dict[axis_0+axis_1] = '((%s|SWIZZLE_VALID_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS))' % (axis_0_pos, axis_1_pos)
			if len(axises)>2:
				for axis_2 in axises:
					axis_2_pos = axis_pos[axis_2]
					axis_dict[axis_0+axis_1+axis_2] = '((%s|SWIZZLE_VALID_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)))' % (axis_0_pos, axis_1_pos, axis_2_pos)
					if len(axises)>3:
						for axis_3 in axises:
							axis_3_pos = axis_pos[axis_3]
							axis_dict[axis_0+axis_1+axis_2+axis_3] = '((%s|SWIZZLE_VALID_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((%s|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  ' % (axis_0_pos, axis_1_pos, axis_2_pos, axis_3_pos)
	
	axises = axises[:-1]


items = axis_dict.items()
items.sort(key = lambda a: a[0].replace('x', '0').replace('y', '1').replace('z', '2').replace('w', '3'))

unique = set()
for key, val in items:
	num = eval(val)
	set_str = 'Vector_setSwizzle' if (len(set(key)) == len(key)) else 'NULL'
	print '\t{"%s", %s(getter)Vector_getSwizzle, (setter)%s, NULL, SET_INT_IN_POINTER(%s)}, // %s' % (key, (' '*(4-len(key))), set_str, axis_dict[key], num)
	unique.add(num)

if len(unique) != len(items):
	print "ERROR"
*/

#if 0
//ROW VECTOR Multiplication - Vector X Matrix
//[x][y][z] *  [1][4][7]
//             [2][5][8]
//             [3][6][9]
//vector/matrix multiplication IS NOT COMMUTATIVE!!!!
static int row_vector_multiplication(float rvec[4], VectorObject* vec, MatrixObject * mat)
{
	float vecCopy[4];
	double dot = 0.0f;
	int x, y, z = 0, vec_size = vec->size;

	if(mat->colSize != vec_size){
		if(mat->colSize == 4 && vec_size != 3){
			PyErr_SetString(PyExc_AttributeError, "vector * matrix: matrix column size and the vector size must be the same");
			return -1;
		}else{
			vecCopy[3] = 1.0f;
		}
	}
	
	if(!BaseMath_ReadCallback(vec) || !BaseMath_ReadCallback(mat))
		return -1;
	
	for(x = 0; x < vec_size; x++){
		vecCopy[x] = vec->vec[x];
	}
	rvec[3] = 1.0f;
	//muliplication
	for(x = 0; x < mat->rowSize; x++) {
		for(y = 0; y < mat->colSize; y++) {
			dot += mat->matrix[x][y] * vecCopy[y];
		}
		rvec[z++] = (float)dot;
		dot = 0.0f;
	}
	return 0;
}
#endif

/*----------------------------Vector.negate() -------------------- */
static char Vector_Negate_doc[] =
".. method:: negate()\n"
"\n"
"   Set all values to their negative.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Negate(VectorObject *self)
{
	int i;
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	for(i = 0; i < self->size; i++)
		self->vec[i] = -(self->vec[i]);
	
	(void)BaseMath_WriteCallback(self); // already checked for error
	
	Py_INCREF(self);
	return (PyObject*)self;
}

static struct PyMethodDef Vector_methods[] = {
	{"zero", (PyCFunction) Vector_Zero, METH_NOARGS, Vector_Zero_doc},
	{"normalize", (PyCFunction) Vector_Normalize, METH_NOARGS, Vector_Normalize_doc},
	{"negate", (PyCFunction) Vector_Negate, METH_NOARGS, Vector_Negate_doc},
	{"resize2D", (PyCFunction) Vector_Resize2D, METH_NOARGS, Vector_Resize2D_doc},
	{"resize3D", (PyCFunction) Vector_Resize3D, METH_NOARGS, Vector_Resize3D_doc},
	{"resize4D", (PyCFunction) Vector_Resize4D, METH_NOARGS, Vector_Resize4D_doc},
	{"to_tuple", (PyCFunction) Vector_ToTuple, METH_VARARGS, Vector_ToTuple_doc},
	{"to_track_quat", ( PyCFunction ) Vector_ToTrackQuat, METH_VARARGS, Vector_ToTrackQuat_doc},
	{"reflect", ( PyCFunction ) Vector_Reflect, METH_O, Vector_Reflect_doc},
	{"cross", ( PyCFunction ) Vector_Cross, METH_O, Vector_Cross_doc},
	{"dot", ( PyCFunction ) Vector_Dot, METH_O, Vector_Dot_doc},
	{"angle", ( PyCFunction ) Vector_angle, METH_VARARGS, Vector_angle_doc},
	{"difference", ( PyCFunction ) Vector_Difference, METH_O, Vector_Difference_doc},
	{"project", ( PyCFunction ) Vector_Project, METH_O, Vector_Project_doc},
	{"lerp", ( PyCFunction ) Vector_Lerp, METH_VARARGS, Vector_Lerp_doc},
	{"rotate", ( PyCFunction ) Vector_Rotate, METH_VARARGS, Vector_Rotate_doc},
	{"copy", (PyCFunction) Vector_copy, METH_NOARGS, Vector_copy_doc},
	{"__copy__", (PyCFunction) Vector_copy, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};


/* Note
 Py_TPFLAGS_CHECKTYPES allows us to avoid casting all types to Vector when coercing
 but this means for eg that 
 vec*mat and mat*vec both get sent to Vector_mul and it neesd to sort out the order
*/

static char vector_doc[] =
"This object gives access to Vectors in Blender.";

PyTypeObject vector_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	/*  For printing, in format "<module>.<name>" */
	"mathutils.Vector",             /* char *tp_name; */
	sizeof(VectorObject),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) BaseMathObject_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,   /* cmpfunc tp_compare; */
	( reprfunc ) Vector_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	&Vector_NumMethods,                       /* PyNumberMethods *tp_as_number; */
	&Vector_SeqMethods,                       /* PySequenceMethods *tp_as_sequence; */
	&Vector_AsMapping,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	vector_doc,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)Vector_richcmpr,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	Vector_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	Vector_getseters,           /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	Vector_new,                 /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*------------------------newVectorObject (internal)-------------
  creates a new vector object
  pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newVectorObject(float *vec, int size, int type, PyTypeObject *base_type)
{
	int i;
	VectorObject *self;

	if(size > 4 || size < 2)
		return NULL;

	if(base_type)	self = (VectorObject *)base_type->tp_alloc(base_type, 0);
	else			self = PyObject_NEW(VectorObject, &vector_Type);

	self->size = size;
	
	/* init callbacks as NULL */
	self->cb_user= NULL;
	self->cb_type= self->cb_subtype= 0;

	if(type == Py_WRAP) {
		self->vec = vec;
		self->wrapped = Py_WRAP;
	} else if (type == Py_NEW) {
		self->vec = PyMem_Malloc(size * sizeof(float));
		if(!vec) { /*new empty*/
			for(i = 0; i < size; i++){
				self->vec[i] = 0.0f;
			}
			if(size == 4)  /* do the homogenous thing */
				self->vec[3] = 1.0f;
		}else{
			for(i = 0; i < size; i++){
				self->vec[i] = vec[i];
			}
		}
		self->wrapped = Py_NEW;
	}else{ /*bad type*/
		return NULL;
	}
	return (PyObject *) self;
}

PyObject *newVectorObject_cb(PyObject *cb_user, int size, int cb_type, int cb_subtype)
{
	float dummy[4] = {0.0, 0.0, 0.0, 0.0}; /* dummy init vector, callbacks will be used on access */
	VectorObject *self= (VectorObject *)newVectorObject(dummy, size, Py_NEW, NULL);
	if(self) {
		Py_INCREF(cb_user);
		self->cb_user=			cb_user;
		self->cb_type=			(unsigned char)cb_type;
		self->cb_subtype=		(unsigned char)cb_subtype;
	}

	return (PyObject *)self;
}
