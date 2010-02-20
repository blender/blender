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

#include "Mathutils.h"

#include "BLI_blenlib.h"
#include "BKE_utildefines.h"
#include "BLI_math.h"

#define MAX_DIMENSIONS 4
/* Swizzle axes get packed into a single value that is used as a closure. Each
   axis uses SWIZZLE_BITS_PER_AXIS bits. The first bit (SWIZZLE_VALID_AXIS) is
   used as a sentinel: if it is unset, the axis is not valid. */
#define SWIZZLE_BITS_PER_AXIS 3
#define SWIZZLE_VALID_AXIS 0x4
#define SWIZZLE_AXIS       0x3

static PyObject *row_vector_multiplication(VectorObject* vec, MatrixObject * mat); /* utility func */

//----------------------------------Mathutils.Vector() ------------------
// Supports 2D, 3D, and 4D vector objects both int and float values
// accepted. Mixed float and int values accepted. Ints are parsed to float 
static PyObject *Vector_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *listObject = NULL;
	int size, i;
	float vec[4], f;
	PyObject *v;

	size = PyTuple_GET_SIZE(args); /* we know its a tuple because its an arg */
	if (size == 1) {
		listObject = PyTuple_GET_ITEM(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
		} else { // Single argument was not a sequence
			PyErr_SetString(PyExc_TypeError, "Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
			return NULL;
		}
	} else if (size == 0) {
		//returns a new empty 3d vector
		return newVectorObject(NULL, 3, Py_NEW, type);
	} else {
		listObject = args;
	}

	if (size<2 || size>4) { // Invalid vector size
		PyErr_SetString(PyExc_AttributeError, "Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
		return NULL;
	}

	for (i=0; i<size; i++) {
		v=PySequence_GetItem(listObject, i);
		if (v==NULL) { // Failed to read sequence
			PyErr_SetString(PyExc_RuntimeError, "Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
			return NULL;
		}

		f= PyFloat_AsDouble(v);
		if(f==-1 && PyErr_Occurred()) { // parsed item not a number
			Py_DECREF(v);
			PyErr_SetString(PyExc_TypeError, "Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
			return NULL;
		}

		vec[i]= f;
		Py_DECREF(v);
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

static PyObject *Vector_Zero(VectorObject * self)
{
	int i;
	for(i = 0; i < self->size; i++) {
		self->vec[i] = 0.0f;
	}
	
	BaseMath_WriteCallback(self);
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

static PyObject *Vector_Normalize(VectorObject * self)
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
	
	BaseMath_WriteCallback(self);
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

static PyObject *Vector_Resize2D(VectorObject * self)
{
	if(self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError, "vector.resize2D(): cannot resize wrapped data - only python vectors\n");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError, "vector.resize2D(): cannot resize a vector that has an owner");
		return NULL;
	}
	
	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 2));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError, "vector.resize2D(): problem allocating pointer space\n\n");
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

static PyObject *Vector_Resize3D(VectorObject * self)
{
	if (self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError, "vector.resize3D(): cannot resize wrapped data - only python vectors\n");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError, "vector.resize3D(): cannot resize a vector that has an owner");
		return NULL;
	}
	
	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 3));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError, "vector.resize3D(): problem allocating pointer space\n\n");
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

static PyObject *Vector_Resize4D(VectorObject * self)
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
		PyErr_SetString(PyExc_MemoryError, "vector.resize4D(): problem allocating pointer space\n\n");
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
".. method:: to_tuple(precision)\n"
"\n"
"   Return this vector as a tuple with.\n"
"\n"
"   :arg precision: The number to round the value to in [0, 21].\n"
"   :type precision: int\n"
"   :return: the values of the vector rounded by *precision*\n"
"   :rtype: tuple\n";

static PyObject *Vector_ToTuple(VectorObject * self, PyObject *value)
{
	int ndigits= PyLong_AsSsize_t(value);
	int x;

	PyObject *ret;

	if(ndigits > 22 || ndigits < 0) { /* accounts for non ints */
		PyErr_SetString(PyExc_TypeError, "vector.to_tuple(ndigits): ndigits must be between 0 and 21");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self))
		return NULL;

	ret= PyTuple_New(self->size);

	for(x = 0; x < self->size; x++) {
		PyTuple_SET_ITEM(ret, x, PyFloat_FromDouble(double_round((double)self->vec[x], ndigits)));
	}

	return ret;
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
"   :return: rotation from the vector and the track and up axis."
"   :rtype: :class:`Quaternion`\n";

static PyObject *Vector_ToTrackQuat( VectorObject * self, PyObject * args )
{
	float vec[3], quat[4];
	char *strack, *sup;
	short track = 2, up = 1;

	if(!PyArg_ParseTuple( args, "|ss:to_track_quat", &strack, &sup)) {
		PyErr_SetString( PyExc_TypeError, "expected optional two strings\n" );
		return NULL;
	}
	if (self->size != 3) {
		PyErr_SetString( PyExc_TypeError, "only for 3D vectors\n" );
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
						PyErr_SetString( PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis\n" );
						return NULL;
				}
			}
			else {
				PyErr_SetString( PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis\n" );
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
				PyErr_SetString( PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis\n" );
				return NULL;
			}
		}
		else {
			PyErr_SetString( PyExc_ValueError, "only X, -X, Y, -Y, Z or -Z for track axis\n" );
			return NULL;
		}
	}

	if (sup) {
		if (strlen(sup) == 1) {
			switch(*sup) {
			case 'X':
				up = 0;
				break;
			case 'y':
				up = 1;
				break;
			case 'Z':
				up = 2;
				break;
			default:
				PyErr_SetString( PyExc_ValueError, "only X, Y or Z for up axis\n" );
				return NULL;
			}
		}
		else {
			PyErr_SetString( PyExc_ValueError, "only X, Y or Z for up axis\n" );
			return NULL;
		}
	}

	if (track == up) {
		PyErr_SetString( PyExc_ValueError, "Can't have the same axis for track and up\n" );
		return NULL;
	}

	/*
		flip vector around, since vectoquat expect a vector from target to tracking object 
		and the python function expects the inverse (a vector to the target).
	*/
	vec[0] = -self->vec[0];
	vec[1] = -self->vec[1];
	vec[2] = -self->vec[2];

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

static PyObject *Vector_Reflect( VectorObject * self, VectorObject * value )
{
	float mirror[3], vec[3];
	float reflect[3] = {0.0f, 0.0f, 0.0f};
	
	if (!VectorObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "vec.reflect(value): expected a vector argument" );
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
	
	return newVectorObject(reflect, self->size, Py_NEW, NULL);
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

static PyObject *Vector_Cross( VectorObject * self, VectorObject * value )
{
	VectorObject *vecCross = NULL;

	if (!VectorObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "vec.cross(value): expected a vector argument" );
		return NULL;
	}
	
	if(self->size != 3 || value->size != 3) {
		PyErr_SetString(PyExc_AttributeError, "vec.cross(value): expects both vectors to be 3D\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;
	
	vecCross = (VectorObject *)newVectorObject(NULL, 3, Py_NEW, NULL);
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

static PyObject *Vector_Dot( VectorObject * self, VectorObject * value )
{
	double dot = 0.0;
	int x;
	
	if (!VectorObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "vec.dot(value): expected a vector argument" );
		return NULL;
	}
	
	if(self->size != value->size) {
		PyErr_SetString(PyExc_AttributeError, "vec.dot(value): expects both vectors to have the same size\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;
	
	for(x = 0; x < self->size; x++) {
		dot += self->vec[x] * value->vec[x];
	}
	return PyFloat_FromDouble(dot);
}

static char Vector_Angle_doc[] = 
".. function:: angle(other)\n"
"\n"
"   Return the angle between two vectors.\n"
"\n"
"   :type other: :class:`Vector`\n"
"   :return angle: angle in radians\n"
"   :rtype: float\n"
"\n"
"   .. note:: Zero length vectors raise an :exc:`AttributeError`.\n";
static PyObject *Vector_Angle(VectorObject * self, VectorObject * value)
{
    double dot = 0.0f, angleRads, test_v1 = 0.0f, test_v2 = 0.0f;
	int x, size;
	
	if (!VectorObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "vec.angle(value): expected a vector argument" );
		return NULL;
	}
	
	if(self->size != value->size) {
		PyErr_SetString(PyExc_AttributeError, "vec.angle(value): expects both vectors to have the same size\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	//since size is the same....
	size = self->size;

	for(x = 0; x < size; x++) {
		test_v1 += self->vec[x] * self->vec[x];
		test_v2 += value->vec[x] * value->vec[x];
	}
	if (!test_v1 || !test_v2){
		PyErr_SetString(PyExc_AttributeError, "vector.angle(other): zero length vectors are not acceptable arguments\n");
		return NULL;
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
"   .. note:: 2D vectors raise an :exc:`AttributeError`.\n";;

static PyObject *Vector_Difference( VectorObject * self, VectorObject * value )
{
	float quat[4], vec_a[3], vec_b[3];

	if (!VectorObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "vec.difference(value): expected a vector argument" );
		return NULL;
	}

	if(self->size < 3 || value->size < 3) {
		PyErr_SetString(PyExc_AttributeError, "vec.difference(value): expects both vectors to be size 3 or 4\n");
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
"   :type other: :class:`Vector`\n"
"   :return projection: the parallel projection vector\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Project(VectorObject * self, VectorObject * value)
{
	float vec[4];
	double dot = 0.0f, dot2 = 0.0f;
	int x, size;

	if (!VectorObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "vec.project(value): expected a vector argument" );
		return NULL;
	}

	if(self->size != value->size) {
		PyErr_SetString(PyExc_AttributeError, "vec.project(value): expects both vectors to have the same size\n");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;


	//since they are the same size...
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
	return newVectorObject(vec, size, Py_NEW, NULL);
}

//----------------------------------Mathutils.MidpointVecs() -------------
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

static PyObject *Vector_Lerp(VectorObject * self, PyObject * args)
{
	VectorObject *vec2 = NULL;
	float fac, ifac, vec[4];
	int x;

	if(!PyArg_ParseTuple(args, "O!f:lerp", &vector_Type, &vec2, &fac)) {
		PyErr_SetString(PyExc_TypeError, "vector.lerp(): expects a vector of the same size and float");
		return NULL;
	}
	if(self->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "vector.lerp(): expects (2) vector objects of the same size");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(vec2))
		return NULL;

	ifac= 1.0 - fac;

	for(x = 0; x < self->size; x++) {
		vec[x] = (ifac * self->vec[x]) + (fac * vec2->vec[x]);
	}
	return newVectorObject(vec, self->size, Py_NEW, NULL);
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

static PyObject *Vector_copy(VectorObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	return newVectorObject(self->vec, self->size, Py_NEW, Py_TYPE(self));
}

/*----------------------------print object (internal)-------------
  print the object to screen */
static PyObject *Vector_repr(VectorObject * self)
{
	int i;
	char buffer[48], str[1024];

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	BLI_strncpy(str,"[",1024);
	for(i = 0; i < self->size; i++){
		if(i < (self->size - 1)){
			sprintf(buffer, "%.6f, ", self->vec[i]);
			strcat(str,buffer);
		}else{
			sprintf(buffer, "%.6f", self->vec[i]);
			strcat(str,buffer);
		}
	}
	strcat(str, "](vector)");

	return PyUnicode_FromString(str);
}
/*---------------------SEQUENCE PROTOCOLS------------------------
  ----------------------------len(object)------------------------
  sequence length*/
static int Vector_len(VectorObject * self)
{
	return self->size;
}
/*----------------------------object[]---------------------------
  sequence accessor (get)*/
static PyObject *Vector_item(VectorObject * self, int i)
{
	if(i<0)	i= self->size-i;

	if(i < 0 || i >= self->size) {
		PyErr_SetString(PyExc_IndexError,"vector[index]: out of range\n");
		return NULL;
	}

	if(!BaseMath_ReadIndexCallback(self, i))
		return NULL;
	
	return PyFloat_FromDouble(self->vec[i]);

}
/*----------------------------object[]-------------------------
  sequence accessor (set)*/
static int Vector_ass_item(VectorObject * self, int i, PyObject * ob)
{
	float scalar= (float)PyFloat_AsDouble(ob);
	if(scalar==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "vector[index] = x: index argument not a number\n");
		return -1;
	}

	if(i<0)	i= self->size-i;

	if(i < 0 || i >= self->size){
		PyErr_SetString(PyExc_IndexError, "vector[index] = x: assignment index out of range\n");
		return -1;
	}
	self->vec[i] = scalar;
	
	if(!BaseMath_WriteIndexCallback(self, i))
		return -1;
	return 0;
}

/*----------------------------object[z:y]------------------------
  sequence slice (get) */
static PyObject *Vector_slice(VectorObject * self, int begin, int end)
{
	PyObject *list = NULL;
	int count;

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	CLAMP(begin, 0, self->size);
	if (end<0) end= self->size+end+1;
	CLAMP(end, 0, self->size);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SET_ITEM(list, count - begin, PyFloat_FromDouble(self->vec[count]));
	}

	return list;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set) */
static int Vector_ass_slice(VectorObject * self, int begin, int end,
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

	size = PySequence_Length(seq);
	if(size != (end - begin)){
		PyErr_SetString(PyExc_TypeError, "vector[begin:end] = []: size mismatch in slice assignment\n");
		return -1;
	}

	for (i = 0; i < size; i++) {
		v = PySequence_GetItem(seq, i);
		if (v == NULL) { /* Failed to read sequence */
			PyErr_SetString(PyExc_RuntimeError, "vector[begin:end] = []: unable to read sequence\n");
			return -1;
		}
		
		scalar= (float)PyFloat_AsDouble(v);
		if(scalar==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
			Py_DECREF(v);
			PyErr_SetString(PyExc_TypeError, "vector[begin:end] = []: sequence argument not a number\n");
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
			PyErr_SetString(PyExc_AttributeError, "Vector addition: vectors must have the same dimensions for this operation\n");
			return NULL;
		}
		for(i = 0; i < vec1->size; i++) {
			vec[i] = vec1->vec[i] +	vec2->vec[i];
		}
		return newVectorObject(vec, vec1->size, Py_NEW, NULL);
	}
	
	PyErr_SetString(PyExc_AttributeError, "Vector addition: arguments not valid for this operation....\n");
	return NULL;
}

/*  ------------------------obj += obj------------------------------
  addition in place */
static PyObject *Vector_iadd(PyObject * v1, PyObject * v2)
{
	int i;
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError, "Vector addition: arguments not valid for this operation....\n");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Vector addition: vectors must have the same dimensions for this operation\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;

	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] = vec1->vec[i] + vec2->vec[i];
	}

	BaseMath_WriteCallback(vec1);
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
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: arguments not valid for this operation....\n");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;
	
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: vectors must have the same dimensions for this operation\n");
		return NULL;
	}
	for(i = 0; i < vec1->size; i++) {
		vec[i] = vec1->vec[i] -	vec2->vec[i];
	}

	return newVectorObject(vec, vec1->size, Py_NEW, NULL);
}

/*------------------------obj -= obj------------------------------
  subtraction*/
static PyObject *Vector_isub(PyObject * v1, PyObject * v2)
{
	int i;
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: arguments not valid for this operation....\n");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Vector subtraction: vectors must have the same dimensions for this operation\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;

	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] = vec1->vec[i] -	vec2->vec[i];
	}

	BaseMath_WriteCallback(vec1);
	Py_INCREF( v1 );
	return v1;
}

/*------------------------obj * obj------------------------------
  mulplication*/
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
			PyErr_SetString(PyExc_AttributeError, "Vector multiplication: vectors must have the same dimensions for this operation\n");
			return NULL;
		}
		
		/*dot product*/
		for(i = 0; i < vec1->size; i++) {
			dot += vec1->vec[i] * vec2->vec[i];
		}
		return PyFloat_FromDouble(dot);
	}
	
	/*swap so vec1 is always the vector */
	if (vec2) {
		vec1= vec2;
		v2= v1;
	}
	
	if (MatrixObject_Check(v2)) {
		/* VEC * MATRIX */
		return row_vector_multiplication(vec1, (MatrixObject*)v2);
	} else if (QuaternionObject_Check(v2)) {
		QuaternionObject *quat = (QuaternionObject*)v2; /* quat_rotation validates */

		if(vec1->size != 3) {
			PyErr_SetString(PyExc_TypeError, "Vector multiplication: only 3D vector rotations (with quats) currently supported\n");
			return NULL;
		}
		return quat_rotation((PyObject*)vec1, (PyObject*)quat);
	}
	else if (((scalar= PyFloat_AsDouble(v2)) == -1.0 && PyErr_Occurred())==0) { /* VEC*FLOAT */
		int i;
		float vec[4];
		
		for(i = 0; i < vec1->size; i++) {
			vec[i] = vec1->vec[i] * scalar;
		}
		return newVectorObject(vec, vec1->size, Py_NEW, NULL);
		
	}
	
	PyErr_SetString(PyExc_TypeError, "Vector multiplication: arguments not acceptable for this operation\n");
	return NULL;
}

/*------------------------obj *= obj------------------------------
  in place mulplication */
static PyObject *Vector_imul(PyObject * v1, PyObject * v2)
{
	VectorObject *vec = (VectorObject *)v1;
	int i;
	float scalar;
	
	if(!BaseMath_ReadCallback(vec))
		return NULL;
	
	/* only support vec*=float and vec*=mat
	   vec*=vec result is a float so that wont work */
	if (MatrixObject_Check(v2)) {
		float vecCopy[4];
		int x,y, size = vec->size;
		MatrixObject *mat= (MatrixObject*)v2;
		
		if(!BaseMath_ReadCallback(mat))
			return NULL;
		
		if(mat->colSize != size){
			if(mat->rowSize == 4 && vec->size != 3){
				PyErr_SetString(PyExc_AttributeError, "vector * matrix: matrix column size and the vector size must be the same");
				return NULL;
			} else {
				vecCopy[3] = 1.0f;
			}
		}
		
		for(i = 0; i < size; i++){
			vecCopy[i] = vec->vec[i];
		}
		
		size = MIN2(size, mat->colSize);
		
		/*muliplication*/
		for(x = 0, i = 0; x < size; x++, i++) {
			double dot = 0.0f;
			for(y = 0; y < mat->rowSize; y++) {
				dot += mat->matrix[y][x] * vecCopy[y];
			}
			vec->vec[i] = (float)dot;
		}
	}
	else if (((scalar= PyFloat_AsDouble(v2)) == -1.0 && PyErr_Occurred())==0) { /* VEC*=FLOAT */
		
		for(i = 0; i < vec->size; i++) {
			vec->vec[i] *=	scalar;
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError, "Vector multiplication: arguments not acceptable for this operation\n");
		return NULL;
	}
	
	BaseMath_WriteCallback(vec);
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
		PyErr_SetString(PyExc_TypeError, "Vector division: Vector must be divided by a float\n");
		return NULL;
	}
	vec1 = (VectorObject*)v1; /* vector */
	
	if(!BaseMath_ReadCallback(vec1))
		return NULL;
	
	scalar = (float)PyFloat_AsDouble(v2);
	if(scalar== -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "Vector division: Vector must be divided by a float\n");
		return NULL;
	}
	
	if(scalar==0.0) { /* not a vector */
		PyErr_SetString(PyExc_ZeroDivisionError, "Vector division: divide by zero error.\n");
		return NULL;
	}
	
	for(i = 0; i < vec1->size; i++) {
		vec[i] = vec1->vec[i] /	scalar;
	}
	return newVectorObject(vec, vec1->size, Py_NEW, NULL);
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

	scalar = (float)PyFloat_AsDouble(v2);
	if(scalar==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "Vector division: Vector must be divided by a float\n");
		return NULL;
	}
	
	if(scalar==0.0) { /* not a vector */
		PyErr_SetString(PyExc_ZeroDivisionError, "Vector division: divide by zero error.\n");
		return NULL;
	}
	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] /=	scalar;
	}
	
	BaseMath_WriteCallback(vec1);
	
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
			result = EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->size, 1);
			if (result == 0){
				result = 1;
			}else{
				result = 0;
			}
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
	(lenfunc) Vector_len,						/* sq_length */
	(binaryfunc) 0,								/* sq_concat */
	(ssizeargfunc) 0,							/* sq_repeat */
	(ssizeargfunc) Vector_item,					/* sq_item */
	NULL,										/* py3 deprecated slice func */
	(ssizeobjargproc) Vector_ass_item,			/* sq_ass_item */
	NULL,										/* py3 deprecated slice assign func */
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
		PyErr_Format(PyExc_TypeError,
			     "vector indices must be integers, not %.200s",
			     item->ob_type->tp_name);
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
		PyErr_Format(PyExc_TypeError,
			     "vector indices must be integers, not %.200s",
			     item->ob_type->tp_name);
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
	
static PyObject *Vector_getAxis( VectorObject * self, void *type )
{
	return Vector_item(self, GET_INT_FROM_POINTER(type));
}

static int Vector_setAxis( VectorObject * self, PyObject * value, void * type )
{
	return Vector_ass_item(self, GET_INT_FROM_POINTER(type), value);
}

/* vector.length */
static PyObject *Vector_getLength( VectorObject * self, void *type )
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

static int Vector_setLength( VectorObject * self, PyObject * value )
{
	double dot = 0.0f, param;
	int i;
	
	if(!BaseMath_ReadCallback(self))
		return -1;
	
	param= PyFloat_AsDouble( value );
	if(param==-1.0 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "length must be set to a number");
		return -1;
	}
	
	if (param < 0) {
		PyErr_SetString( PyExc_TypeError, "cannot set a vectors length to a negative value" );
		return -1;
	}
	if (param==0) {
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
	
	BaseMath_WriteCallback(self); /* checked alredy */
	
	return 0;
}

/* Get a new Vector according to the provided swizzle. This function has little
   error checking, as we are in control of the inputs: the closure is set by us
   in Vector_createSwizzleGetSeter. */
static PyObject *Vector_getSwizzle(VectorObject * self, void *closure)
{
	size_t axisA;
	size_t axisB;
	float vec[MAX_DIMENSIONS];
	unsigned int swizzleClosure;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	/* Unpack the axes from the closure into an array. */
	axisA = 0;
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axisB = swizzleClosure & SWIZZLE_AXIS;
		if(axisB >= self->size) {
			PyErr_SetString(PyExc_AttributeError, "Error: vector does not have specified axis.");
			return NULL;
		}

		vec[axisA] = self->vec[axisB];
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axisA++;
	}
	
	return newVectorObject(vec, axisA, Py_NEW, Py_TYPE(self));
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
static int Vector_setSwizzle(VectorObject * self, PyObject * value, void *closure)
{
	VectorObject *vecVal = NULL;
	PyObject *item;
	size_t listLen;
	float scalarVal;

	size_t axisB;
	size_t axisA;
	unsigned int swizzleClosure;
	
	float vecTemp[MAX_DIMENSIONS];
	
	if(!BaseMath_ReadCallback(self))
		return -1;
	
	/* Check that the closure can be used with this vector: even 2D vectors have
	   swizzles defined for axes z and w, but they would be invalid. */
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axisA = swizzleClosure & SWIZZLE_AXIS;
		if (axisA >= self->size)
		{
			PyErr_SetString(PyExc_AttributeError, "Error: vector does not have specified axis.\n");
			return -1;
		}
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
	}
	
	if (VectorObject_Check(value))
	{
		/* Copy vector contents onto swizzled axes. */
		vecVal = (VectorObject*) value;
		axisB = 0;
		swizzleClosure = GET_INT_FROM_POINTER(closure);
		while (swizzleClosure & SWIZZLE_VALID_AXIS && axisB < vecVal->size)
		{
			axisA = swizzleClosure & SWIZZLE_AXIS;

			if(axisB >= vecVal->size) {
				PyErr_SetString(PyExc_AttributeError, "Error: vector does not have specified axis.");
				return -1;
			}

			vecTemp[axisA] = vecVal->vec[axisB];
			
			swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
			axisB++;
		}

		if(axisB != vecVal->size) {
			PyErr_SetString(PyExc_AttributeError, "Error: vector size does not match swizzle.\n");
			return -1;
		}

		memcpy(self->vec, vecTemp, axisB * sizeof(float));
		/* continue with BaseMathObject_WriteCallback at the end */
	}
	else if (PyList_Check(value))
	{
		/* Copy list contents onto swizzled axes. */
		listLen = PyList_Size(value);
		swizzleClosure = GET_INT_FROM_POINTER(closure);
		axisB = 0;
		while (swizzleClosure & SWIZZLE_VALID_AXIS && axisB < listLen)
		{
			item = PyList_GetItem(value, axisB);
			scalarVal = (float)PyFloat_AsDouble(item);

			if (scalarVal==-1.0 && PyErr_Occurred()) {
				PyErr_SetString(PyExc_AttributeError, "Error: list item could not be used as a float.\n");
				return -1;
			}
			
			
			axisA = swizzleClosure & SWIZZLE_AXIS;
			vecTemp[axisA] = scalarVal;
			
			swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
			axisB++;
		}

		if(axisB != listLen) {
			PyErr_SetString(PyExc_AttributeError, "Error: list size does not match swizzle.\n");
			return -1;
		}

		memcpy(self->vec, vecTemp, axisB * sizeof(float));
		/* continue with BaseMathObject_WriteCallback at the end */
	}
	else if (((scalarVal = (float)PyFloat_AsDouble(value)) == -1.0 && PyErr_Occurred())==0)
	{
		/* Assign the same value to each axis. */
		swizzleClosure = GET_INT_FROM_POINTER(closure);
		while (swizzleClosure & SWIZZLE_VALID_AXIS)
		{
			axisA = swizzleClosure & SWIZZLE_AXIS;
			self->vec[axisA] = scalarVal;
			
			swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		}
		/* continue with BaseMathObject_WriteCallback at the end */
	}
	else {
		PyErr_SetString( PyExc_TypeError, "Expected a Vector, list or scalar value." );
		return -1;
	}
	
	if(!BaseMath_WriteCallback(self))
		return -1;
	else
		return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Vector_getseters[] = {
	{"x", (getter)Vector_getAxis, (setter)Vector_setAxis, "Vector X axis. **type** float", (void *)0},
	{"y", (getter)Vector_getAxis, (setter)Vector_setAxis, "Vector Y axis. **type** float", (void *)1},
	{"z", (getter)Vector_getAxis, (setter)Vector_setAxis, "Vector Z axis (3D Vectors only). **type** float", (void *)2},
	{"w", (getter)Vector_getAxis, (setter)Vector_setAxis, "Vector W axis (4D Vectors only). **type** float", (void *)3},
	{"length", (getter)Vector_getLength, (setter)Vector_setLength, "Vector Length. **type** float", NULL},
	{"magnitude", (getter)Vector_getLength, (setter)Vector_setLength, "Vector Length. **type** float", NULL},
	{"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, BaseMathObject_Wrapped_doc, NULL},
	{"_owner", (getter)BaseMathObject_getOwner, (setter)NULL, BaseMathObject_Owner_doc, NULL},
	
	/* autogenerated swizzle attrs, see python script below */
	{"xx",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 36
	{"xxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 292
	{"xxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2340
	{"xxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2852
	{"xxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3364
	{"xxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3876
	{"xxy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 356
	{"xxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2404
	{"xxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2916
	{"xxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3428
	{"xxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3940
	{"xxz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 420
	{"xxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2468
	{"xxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2980
	{"xxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3492
	{"xxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4004
	{"xxw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 484
	{"xxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2532
	{"xxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3044
	{"xxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3556
	{"xxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4068
	{"xy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 44
	{"xyx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 300
	{"xyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2348
	{"xyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2860
	{"xyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3372
	{"xyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3884
	{"xyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 364
	{"xyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2412
	{"xyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2924
	{"xyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3436
	{"xyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3948
	{"xyz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 428
	{"xyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2476
	{"xyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2988
	{"xyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3500
	{"xyzw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4012
	{"xyw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 492
	{"xywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2540
	{"xywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3052
	{"xywz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3564
	{"xyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4076
	{"xz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 52
	{"xzx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 308
	{"xzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2356
	{"xzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2868
	{"xzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3380
	{"xzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3892
	{"xzy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 372
	{"xzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2420
	{"xzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2932
	{"xzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3444
	{"xzyw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3956
	{"xzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 436
	{"xzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2484
	{"xzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2996
	{"xzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3508
	{"xzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4020
	{"xzw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 500
	{"xzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2548
	{"xzwy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3060
	{"xzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3572
	{"xzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4084
	{"xw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 60
	{"xwx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 316
	{"xwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2364
	{"xwxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2876
	{"xwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3388
	{"xwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3900
	{"xwy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 380
	{"xwyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2428
	{"xwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2940
	{"xwyz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3452
	{"xwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3964
	{"xwz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 444
	{"xwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2492
	{"xwzy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3004
	{"xwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3516
	{"xwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4028
	{"xww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 508
	{"xwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2556
	{"xwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3068
	{"xwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3580
	{"xwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4092
	{"yx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 37
	{"yxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 293
	{"yxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2341
	{"yxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2853
	{"yxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3365
	{"yxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3877
	{"yxy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 357
	{"yxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2405
	{"yxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2917
	{"yxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3429
	{"yxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3941
	{"yxz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 421
	{"yxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2469
	{"yxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2981
	{"yxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3493
	{"yxzw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4005
	{"yxw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 485
	{"yxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2533
	{"yxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3045
	{"yxwz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3557
	{"yxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4069
	{"yy",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 45
	{"yyx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 301
	{"yyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2349
	{"yyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2861
	{"yyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3373
	{"yyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3885
	{"yyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 365
	{"yyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2413
	{"yyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2925
	{"yyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3437
	{"yyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3949
	{"yyz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 429
	{"yyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2477
	{"yyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2989
	{"yyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3501
	{"yyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4013
	{"yyw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 493
	{"yywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2541
	{"yywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3053
	{"yywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3565
	{"yyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4077
	{"yz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 53
	{"yzx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 309
	{"yzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2357
	{"yzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2869
	{"yzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3381
	{"yzxw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3893
	{"yzy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 373
	{"yzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2421
	{"yzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2933
	{"yzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3445
	{"yzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3957
	{"yzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 437
	{"yzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2485
	{"yzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2997
	{"yzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3509
	{"yzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4021
	{"yzw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 501
	{"yzwx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2549
	{"yzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3061
	{"yzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3573
	{"yzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4085
	{"yw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 61
	{"ywx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 317
	{"ywxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2365
	{"ywxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2877
	{"ywxz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3389
	{"ywxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3901
	{"ywy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 381
	{"ywyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2429
	{"ywyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2941
	{"ywyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3453
	{"ywyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3965
	{"ywz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 445
	{"ywzx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2493
	{"ywzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3005
	{"ywzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3517
	{"ywzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4029
	{"yww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 509
	{"ywwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2557
	{"ywwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3069
	{"ywwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3581
	{"ywww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4093
	{"zx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 38
	{"zxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 294
	{"zxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2342
	{"zxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2854
	{"zxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3366
	{"zxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3878
	{"zxy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 358
	{"zxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2406
	{"zxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2918
	{"zxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3430
	{"zxyw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3942
	{"zxz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 422
	{"zxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2470
	{"zxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2982
	{"zxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3494
	{"zxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4006
	{"zxw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 486
	{"zxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2534
	{"zxwy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3046
	{"zxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3558
	{"zxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4070
	{"zy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 46
	{"zyx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 302
	{"zyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2350
	{"zyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2862
	{"zyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3374
	{"zyxw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3886
	{"zyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 366
	{"zyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2414
	{"zyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2926
	{"zyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3438
	{"zyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3950
	{"zyz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 430
	{"zyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2478
	{"zyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2990
	{"zyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3502
	{"zyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4014
	{"zyw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 494
	{"zywx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2542
	{"zywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3054
	{"zywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3566
	{"zyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4078
	{"zz",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 54
	{"zzx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 310
	{"zzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2358
	{"zzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2870
	{"zzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3382
	{"zzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3894
	{"zzy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 374
	{"zzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2422
	{"zzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2934
	{"zzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3446
	{"zzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3958
	{"zzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 438
	{"zzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2486
	{"zzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2998
	{"zzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3510
	{"zzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4022
	{"zzw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 502
	{"zzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2550
	{"zzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3062
	{"zzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3574
	{"zzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4086
	{"zw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 62
	{"zwx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 318
	{"zwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2366
	{"zwxy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2878
	{"zwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3390
	{"zwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3902
	{"zwy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 382
	{"zwyx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2430
	{"zwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2942
	{"zwyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3454
	{"zwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3966
	{"zwz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 446
	{"zwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2494
	{"zwzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3006
	{"zwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3518
	{"zwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4030
	{"zww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 510
	{"zwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2558
	{"zwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3070
	{"zwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3582
	{"zwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4094
	{"wx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 39
	{"wxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 295
	{"wxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2343
	{"wxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2855
	{"wxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3367
	{"wxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3879
	{"wxy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 359
	{"wxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2407
	{"wxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2919
	{"wxyz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3431
	{"wxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3943
	{"wxz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 423
	{"wxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2471
	{"wxzy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2983
	{"wxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3495
	{"wxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4007
	{"wxw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 487
	{"wxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2535
	{"wxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3047
	{"wxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3559
	{"wxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4071
	{"wy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 47
	{"wyx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 303
	{"wyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2351
	{"wyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2863
	{"wyxz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3375
	{"wyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3887
	{"wyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 367
	{"wyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2415
	{"wyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2927
	{"wyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3439
	{"wyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3951
	{"wyz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 431
	{"wyzx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2479
	{"wyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2991
	{"wyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3503
	{"wyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4015
	{"wyw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 495
	{"wywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2543
	{"wywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3055
	{"wywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3567
	{"wyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4079
	{"wz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 55
	{"wzx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 311
	{"wzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2359
	{"wzxy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2871
	{"wzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3383
	{"wzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3895
	{"wzy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 375
	{"wzyx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2423
	{"wzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2935
	{"wzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3447
	{"wzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3959
	{"wzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 439
	{"wzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2487
	{"wzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2999
	{"wzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3511
	{"wzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4023
	{"wzw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 503
	{"wzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2551
	{"wzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3063
	{"wzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3575
	{"wzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4087
	{"ww",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 63
	{"wwx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 319
	{"wwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2367
	{"wwxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2879
	{"wwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3391
	{"wwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3903
	{"wwy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 383
	{"wwyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2431
	{"wwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2943
	{"wwyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3455
	{"wwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3967
	{"wwz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 447
	{"wwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2495
	{"wwzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3007
	{"wwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3519
	{"wwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4031
	{"www",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 511
	{"wwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2559
	{"wwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3071
	{"wwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3583
	{"wwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4095
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

//-----------------row_vector_multiplication (internal)-----------
//ROW VECTOR Multiplication - Vector X Matrix
//[x][y][z] *  [1][4][7]
//             [2][5][8]
//             [3][6][9]
//vector/matrix multiplication IS NOT COMMUTATIVE!!!!
static PyObject *row_vector_multiplication(VectorObject* vec, MatrixObject * mat)
{
	float vecNew[4], vecCopy[4];
	double dot = 0.0f;
	int x, y, z = 0, vec_size = vec->size;

	if(mat->colSize != vec_size){
		if(mat->colSize == 4 && vec_size != 3){
			PyErr_SetString(PyExc_AttributeError, "vector * matrix: matrix column size and the vector size must be the same");
			return NULL;
		}else{
			vecCopy[3] = 1.0f;
		}
	}
	
	if(!BaseMath_ReadCallback(vec) || !BaseMath_ReadCallback(mat))
		return NULL;
	
	for(x = 0; x < vec_size; x++){
		vecCopy[x] = vec->vec[x];
	}
	vecNew[3] = 1.0f;
	//muliplication
	for(x = 0; x < mat->rowSize; x++) {
		for(y = 0; y < mat->colSize; y++) {
			dot += mat->matrix[x][y] * vecCopy[y];
		}
		vecNew[z++] = (float)dot;
		dot = 0.0f;
	}
	return newVectorObject(vecNew, vec_size, Py_NEW, NULL);
}

/*----------------------------Vector.negate() -------------------- */
static char Vector_Negate_doc[] =
".. method:: negate()\n"
"\n"
"   Set all values to their negative.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n";

static PyObject *Vector_Negate(VectorObject * self)
{
	int i;
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	for(i = 0; i < self->size; i++)
		self->vec[i] = -(self->vec[i]);
	
	BaseMath_WriteCallback(self); // alredy checked for error
	
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
	{"to_tuple", (PyCFunction) Vector_ToTuple, METH_O, Vector_ToTuple_doc},
	{"to_track_quat", ( PyCFunction ) Vector_ToTrackQuat, METH_VARARGS, Vector_ToTrackQuat_doc},
	{"reflect", ( PyCFunction ) Vector_Reflect, METH_O, Vector_Reflect_doc},
	{"cross", ( PyCFunction ) Vector_Cross, METH_O, Vector_Cross_doc},
	{"dot", ( PyCFunction ) Vector_Dot, METH_O, Vector_Dot_doc},
	{"angle", ( PyCFunction ) Vector_Angle, METH_O, Vector_Angle_doc},
	{"difference", ( PyCFunction ) Vector_Difference, METH_O, Vector_Difference_doc},
	{"project", ( PyCFunction ) Vector_Project, METH_O, Vector_Project_doc},
	{"lerp", ( PyCFunction ) Vector_Lerp, METH_VARARGS, Vector_Lerp_doc},
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
"This object gives access to Vectors in Blender.\n"
"\n"
".. literalinclude:: ../examples/mathutils_vector.py\n";

PyTypeObject vector_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	/*  For printing, in format "<module>.<name>" */
	"vector",             /* char *tp_name; */
	sizeof( VectorObject ),         /* int tp_basicsize; */
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
