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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * 
 * Contributor(s): Willian P. Germano & Joseph Gilbert, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Mathutils.h"

#include "BLI_blenlib.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"
#include "gen_utils.h"


/*-------------------------DOC STRINGS ---------------------------*/
char Vector_Zero_doc[] = "() - set all values in the vector to 0";
char Vector_Normalize_doc[] = "() - normalize the vector";
char Vector_Negate_doc[] = "() - changes vector to it's additive inverse";
char Vector_Resize2D_doc[] = "() - resize a vector to [x,y]";
char Vector_Resize3D_doc[] = "() - resize a vector to [x,y,z]";
char Vector_Resize4D_doc[] = "() - resize a vector to [x,y,z,w]";
char Vector_toPoint_doc[] = "() - create a new Point Object from this vector";
char Vector_ToTrackQuat_doc[] = "(track, up) - extract a quaternion from the vector and the track and up axis";
char Vector_reflect_doc[] = "(mirror) - return a vector reflected on the mirror normal";
char Vector_copy_doc[] = "() - return a copy of the vector";
/*-----------------------METHOD DEFINITIONS ----------------------*/
struct PyMethodDef Vector_methods[] = {
	{"zero", (PyCFunction) Vector_Zero, METH_NOARGS, Vector_Zero_doc},
	{"normalize", (PyCFunction) Vector_Normalize, METH_NOARGS, Vector_Normalize_doc},
	{"negate", (PyCFunction) Vector_Negate, METH_NOARGS, Vector_Negate_doc},
	{"resize2D", (PyCFunction) Vector_Resize2D, METH_NOARGS, Vector_Resize2D_doc},
	{"resize3D", (PyCFunction) Vector_Resize3D, METH_NOARGS, Vector_Resize2D_doc},
	{"resize4D", (PyCFunction) Vector_Resize4D, METH_NOARGS, Vector_Resize2D_doc},
	{"toPoint", (PyCFunction) Vector_toPoint, METH_NOARGS, Vector_toPoint_doc},
	{"toTrackQuat", ( PyCFunction ) Vector_ToTrackQuat, METH_VARARGS, Vector_ToTrackQuat_doc},
	{"reflect", ( PyCFunction ) Vector_reflect, METH_O, Vector_reflect_doc},
	{"copy", (PyCFunction) Vector_copy, METH_NOARGS, Vector_copy_doc},
	{"__copy__", (PyCFunction) Vector_copy, METH_NOARGS, Vector_copy_doc},
	{NULL, NULL, 0, NULL}
};

/*-----------------------------METHODS----------------------------
  --------------------------Vector.toPoint()----------------------
  create a new point object to represent this vector */
PyObject *Vector_toPoint(VectorObject * self)
{
	float coord[3];
	int i;

	if(self->size < 2 || self->size > 3) {
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector.toPoint(): inappropriate vector size - expects 2d or 3d vector\n");
	} 
	for(i = 0; i < self->size; i++){
		coord[i] = self->vec[i];
	}
	
	return newPointObject(coord, self->size, Py_NEW);
}
/*----------------------------Vector.zero() ----------------------
  set the vector data to 0,0,0 */
PyObject *Vector_Zero(VectorObject * self)
{
	int i;
	for(i = 0; i < self->size; i++) {
		self->vec[i] = 0.0f;
	}
	return EXPP_incr_ret((PyObject*)self);
}
/*----------------------------Vector.normalize() -----------------
  normalize the vector data to a unit vector */
PyObject *Vector_Normalize(VectorObject * self)
{
	int i;
	float norm = 0.0f;

	for(i = 0; i < self->size; i++) {
		norm += self->vec[i] * self->vec[i];
	}
	norm = (float) sqrt(norm);
	for(i = 0; i < self->size; i++) {
		self->vec[i] /= norm;
	}
	return EXPP_incr_ret((PyObject*)self);
}


/*----------------------------Vector.resize2D() ------------------
  resize the vector to x,y */
PyObject *Vector_Resize2D(VectorObject * self)
{
	if(self->wrapped==Py_WRAP)
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector.resize2d(): cannot resize wrapped data - only python vectors\n");

	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 2));
	if(self->vec == NULL)
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"vector.resize2d(): problem allocating pointer space\n\n");
	
	self->size = 2;
	return EXPP_incr_ret((PyObject*)self);
}
/*----------------------------Vector.resize3D() ------------------
  resize the vector to x,y,z */
PyObject *Vector_Resize3D(VectorObject * self)
{
	if (self->wrapped==Py_WRAP)
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector.resize3d(): cannot resize wrapped data - only python vectors\n");

	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 3));
	if(self->vec == NULL)
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"vector.resize3d(): problem allocating pointer space\n\n");
	
	if(self->size == 2)
		self->vec[2] = 0.0f;
	
	self->size = 3;
	return EXPP_incr_ret((PyObject*)self);
}
/*----------------------------Vector.resize4D() ------------------
  resize the vector to x,y,z,w */
PyObject *Vector_Resize4D(VectorObject * self)
{
	if(self->wrapped==Py_WRAP)
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector.resize4d(): cannot resize wrapped data - only python vectors\n");

	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 4));
	if(self->vec == NULL)
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"vector.resize4d(): problem allocating pointer space\n\n");
	
	if(self->size == 2){
		self->vec[2] = 0.0f;
		self->vec[3] = 1.0f;
	}else if(self->size == 3){
		self->vec[3] = 1.0f;
	}
	self->size = 4;
	return EXPP_incr_ret((PyObject*)self);
}
/*----------------------------Vector.toTrackQuat(track, up) ----------------------
  extract a quaternion from the vector and the track and up axis */
PyObject *Vector_ToTrackQuat( VectorObject * self, PyObject * args )
{
	float vec[3], quat[4];
	char *strack, *sup;
	short track = 2, up = 1;

	if( !PyArg_ParseTuple ( args, "|ss", &strack, &sup ) ) {
		return EXPP_ReturnPyObjError( PyExc_TypeError, 
			"expected optional two strings\n" );
	}
	if (self->size != 3) {
		return EXPP_ReturnPyObjError( PyExc_TypeError, "only for 3D vectors\n" );
	}

	if (strack) {
		if (strlen(strack) == 2) {
			if (strack[0] == '-') {
				switch(strack[1]) {
					case 'X':
					case 'x':
						track = 3;
						break;
					case 'Y':
					case 'y':
						track = 4;
						break;
					case 'z':
					case 'Z':
						track = 5;
						break;
					default:
						return EXPP_ReturnPyObjError( PyExc_ValueError,
										  "only X, -X, Y, -Y, Z or -Z for track axis\n" );
				}
			}
			else {
				return EXPP_ReturnPyObjError( PyExc_ValueError,
								  "only X, -X, Y, -Y, Z or -Z for track axis\n" );
			}
		}
		else if (strlen(strack) == 1) {
			switch(strack[0]) {
			case '-':
			case 'X':
			case 'x':
				track = 0;
				break;
			case 'Y':
			case 'y':
				track = 1;
				break;
			case 'z':
			case 'Z':
				track = 2;
				break;
			default:
				return EXPP_ReturnPyObjError( PyExc_ValueError,
								  "only X, -X, Y, -Y, Z or -Z for track axis\n" );
			}
		}
		else {
			return EXPP_ReturnPyObjError( PyExc_ValueError,
							  "only X, -X, Y, -Y, Z or -Z for track axis\n" );
		}
	}

	if (sup) {
		if (strlen(sup) == 1) {
			switch(*sup) {
			case 'X':
			case 'x':
				up = 0;
				break;
			case 'Y':
			case 'y':
				up = 1;
				break;
			case 'z':
			case 'Z':
				up = 2;
				break;
			default:
				return EXPP_ReturnPyObjError( PyExc_ValueError,
								  "only X, Y or Z for up axis\n" );
			}
		}
		else {
			return EXPP_ReturnPyObjError( PyExc_ValueError,
							  "only X, Y or Z for up axis\n" );
		}
	}

	if (track == up) {
			return EXPP_ReturnPyObjError( PyExc_ValueError,
						      "Can't have the same axis for track and up\n" );
	}

	/*
		flip vector around, since vectoquat expect a vector from target to tracking object 
		and the python function expects the inverse (a vector to the target).
	*/
	vec[0] = -self->vec[0];
	vec[1] = -self->vec[1];
	vec[2] = -self->vec[2];

	vectoquat(vec, track, up, quat);

	return newQuaternionObject(quat, Py_NEW);
}

/*----------------------------Vector.reflect(mirror) ----------------------
  return a reflected vector on the mirror normal
  ((2 * DotVecs(vec, mirror)) * mirror) - vec
  using arithb.c would be nice here */
PyObject *Vector_reflect( VectorObject * self, PyObject * value )
{
	VectorObject *mirrvec;
	float mirror[3];
	float vec[3];
	float reflect[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float dot2;
	
	/* for normalizing */
	int i;
	float norm = 0.0f;
	
	if (!VectorObject_Check(value)) 
		return EXPP_ReturnPyObjError( PyExc_TypeError, "expected a vector argument" );
	
	mirrvec = (VectorObject *)value;
	
	mirror[0] = mirrvec->vec[0];
	mirror[1] = mirrvec->vec[1];
	if (mirrvec->size > 2)	mirror[2] = mirrvec->vec[2];
	else					mirror[2] = 0.0;
	
	/* normalize, whos idea was it not to use arithb.c? :-/ */
	for(i = 0; i < 3; i++) {
		norm += mirror[i] * mirror[i];
	}
	norm = (float) sqrt(norm);
	for(i = 0; i < 3; i++) {
		mirror[i] /= norm;
	}
	/* done */
	
	vec[0] = self->vec[0];
	vec[1] = self->vec[1];
	if (self->size > 2)		vec[2] = self->vec[2];
	else					vec[2] = 0.0;
	
	dot2 = 2 * vec[0]*mirror[0]+vec[1]*mirror[1]+vec[2]*mirror[2];
	
	reflect[0] = (dot2 * mirror[0]) - vec[0];
	reflect[1] = (dot2 * mirror[1]) - vec[1];
	reflect[2] = (dot2 * mirror[2]) - vec[2];
	
	return newVectorObject(reflect, self->size, Py_NEW);
}

/*----------------------------Vector.copy() --------------------------------------
  return a copy of the vector */
PyObject *Vector_copy(VectorObject * self)
{
	return newVectorObject(self->vec, self->size, Py_NEW);
}

/*----------------------------dealloc()(internal) ----------------
  free the py_object */
static void Vector_dealloc(VectorObject * self)
{
	/* only free non wrapped */
	if(self->wrapped != Py_WRAP){
		PyMem_Free(self->vec);
	}
	PyObject_DEL(self);
}

/*----------------------------print object (internal)-------------
  print the object to screen */
static PyObject *Vector_repr(VectorObject * self)
{
	int i;
	char buffer[48], str[1024];

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

	return PyString_FromString(str);
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
	if(i < 0 || i >= self->size)
		return EXPP_ReturnPyObjError(PyExc_IndexError,
		"vector[index]: out of range\n");

	return PyFloat_FromDouble(self->vec[i]);

}
/*----------------------------object[]-------------------------
  sequence accessor (set)*/
static int Vector_ass_item(VectorObject * self, int i, PyObject * ob)
{
	
	if(!(PyNumber_Check(ob))) { /* parsed item not a number */
		return EXPP_ReturnIntError(PyExc_TypeError, 
			"vector[index] = x: index argument not a number\n");
	}

	if(i < 0 || i >= self->size){
		return EXPP_ReturnIntError(PyExc_IndexError,
			"vector[index] = x: assignment index out of range\n");
	}
	self->vec[i] = (float)PyFloat_AsDouble(ob);
	return 0;
}

/*----------------------------object[z:y]------------------------
  sequence slice (get) */
static PyObject *Vector_slice(VectorObject * self, int begin, int end)
{
	PyObject *list = NULL;
	int count;

	CLAMP(begin, 0, self->size);
	if (end<0) end= self->size+end+1;
	CLAMP(end, 0, self->size);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SetItem(list, count - begin,
				PyFloat_FromDouble(self->vec[count]));
	}

	return list;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set) */
static int Vector_ass_slice(VectorObject * self, int begin, int end,
			     PyObject * seq)
{
	int i, y, size = 0;
	float vec[4];
	PyObject *v;

	CLAMP(begin, 0, self->size);
	if (end<0) end= self->size+end+1;
	CLAMP(end, 0, self->size);
	begin = MIN2(begin,end);

	size = PySequence_Length(seq);
	if(size != (end - begin)){
		return EXPP_ReturnIntError(PyExc_TypeError,
			"vector[begin:end] = []: size mismatch in slice assignment\n");
	}

	for (i = 0; i < size; i++) {
		v = PySequence_GetItem(seq, i);
		if (v == NULL) { /* Failed to read sequence */
			return EXPP_ReturnIntError(PyExc_RuntimeError, 
				"vector[begin:end] = []: unable to read sequence\n");
		}
		
		if(!PyNumber_Check(v)) { /* parsed item not a number */
			Py_DECREF(v);
			return EXPP_ReturnIntError(PyExc_TypeError, 
				"vector[begin:end] = []: sequence argument not a number\n");
		}

		vec[i] = (float)PyFloat_AsDouble(v);
		Py_DECREF(v);
	}
	/*parsed well - now set in vector*/
	for(y = 0; y < size; y++){
		self->vec[begin + y] = vec[y];
	}
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
		/*VECTOR + VECTOR*/
		if(vec1->size != vec2->size)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector addition: vectors must have the same dimensions for this operation\n");
		
		for(i = 0; i < vec1->size; i++) {
			vec[i] = vec1->vec[i] +	vec2->vec[i];
		}
		return newVectorObject(vec, vec1->size, Py_NEW);
	}
	
	if(PointObject_Check(v2)){  /*VECTOR + POINT*/
		/*Point translation*/
		PointObject *pt = (PointObject*)v2;
		
		if(pt->size == vec1->size){
			for(i = 0; i < vec1->size; i++){
				vec[i] = vec1->vec[i] + pt->coord[i];
			}
		}else{
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Vector addition: arguments are the wrong size....\n");
		}
		return newPointObject(vec, vec1->size, Py_NEW);
	}
	
	return EXPP_ReturnPyObjError(PyExc_AttributeError,
		"Vector addition: arguments not valid for this operation....\n");
}

/*  ------------------------obj += obj------------------------------
  addition in place */
static PyObject *Vector_iadd(PyObject * v1, PyObject * v2)
{
	int i;

	VectorObject *vec1 = NULL, *vec2 = NULL;
	
	if VectorObject_Check(v1)
		vec1= (VectorObject *)v1;
	
	if VectorObject_Check(v2)
		vec2= (VectorObject *)v2;
	
	/* make sure v1 is always the vector */
	if (vec1 && vec2 ) {
		/*VECTOR + VECTOR*/
		if(vec1->size != vec2->size)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector addition: vectors must have the same dimensions for this operation\n");
		
		for(i = 0; i < vec1->size; i++) {
			vec1->vec[i] +=	vec2->vec[i];
		}
		Py_INCREF( v1 );
		return v1;
	}
	
	if(PointObject_Check(v2)){  /*VECTOR + POINT*/
		/*Point translation*/
		PointObject *pt = (PointObject*)v2;
		
		if(pt->size == vec1->size){
			for(i = 0; i < vec1->size; i++){
				vec1->vec[i] += pt->coord[i];
			}
		}else{
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Vector addition: arguments are the wrong size....\n");
		}
		Py_INCREF( v1 );
		return v1;
	}
	
	return EXPP_ReturnPyObjError(PyExc_AttributeError,
		"Vector addition: arguments not valid for this operation....\n");
}

/*------------------------obj - obj------------------------------
  subtraction*/
static PyObject *Vector_sub(PyObject * v1, PyObject * v2)
{
	int i;
	float vec[4];
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2))
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector subtraction: arguments not valid for this operation....\n");
	
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(vec1->size != vec2->size)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		"Vector subtraction: vectors must have the same dimensions for this operation\n");
	
	for(i = 0; i < vec1->size; i++) {
		vec[i] = vec1->vec[i] -	vec2->vec[i];
	}

	return newVectorObject(vec, vec1->size, Py_NEW);
}

/*------------------------obj -= obj------------------------------
  subtraction*/
static PyObject *Vector_isub(PyObject * v1, PyObject * v2)
{
	int i, size;
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2))
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector subtraction: arguments not valid for this operation....\n");
	
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;
	
	if(vec1->size != vec2->size)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		"Vector subtraction: vectors must have the same dimensions for this operation\n");

	size = vec1->size;
	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] = vec1->vec[i] -	vec2->vec[i];
	}

	Py_INCREF( v1 );
	return v1;
}

/*------------------------obj * obj------------------------------
  mulplication*/
static PyObject *Vector_mul(PyObject * v1, PyObject * v2)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	
	if VectorObject_Check(v1)
		vec1= (VectorObject *)v1;
	
	if VectorObject_Check(v2)
		vec2= (VectorObject *)v2;
	
	/* make sure v1 is always the vector */
	if (vec1 && vec2 ) {
		int i;
		double dot = 0.0f;
		
		if(vec1->size != vec2->size)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector multiplication: vectors must have the same dimensions for this operation\n");
		
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
	
	if (PyNumber_Check(v2)) {
		/* VEC * NUM */
		int i;
		float vec[4];
		float scalar = (float)PyFloat_AsDouble( v2 );
		
		for(i = 0; i < vec1->size; i++) {
			vec[i] = vec1->vec[i] *	scalar;
		}
		return newVectorObject(vec, vec1->size, Py_NEW);
		
	} else if (MatrixObject_Check(v2)) {
		/* VEC * MATRIX */
		if (v1==v2) /* mat*vec, we have swapped the order */
			return column_vector_multiplication((MatrixObject*)v2, vec1);
		else /* vec*mat */
			return row_vector_multiplication(vec1, (MatrixObject*)v2);
	} else if (QuaternionObject_Check(v2)) {
		QuaternionObject *quat = (QuaternionObject*)v2;
		if(vec1->size != 3)
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Vector multiplication: only 3D vector rotations (with quats) currently supported\n");
		
		return quat_rotation((PyObject*)vec1, (PyObject*)quat);
	}
	
	return EXPP_ReturnPyObjError(PyExc_TypeError,
		"Vector multiplication: arguments not acceptable for this operation\n");
}

/*------------------------obj *= obj------------------------------
  in place mulplication */
static PyObject *Vector_imul(PyObject * v1, PyObject * v2)
{
	VectorObject *vec = (VectorObject *)v1;
	int i;
	
	/* only support vec*=float and vec*=mat
	   vec*=vec result is a float so that wont work */
	if (PyNumber_Check(v2)) {
		/* VEC * NUM */
		float scalar = (float)PyFloat_AsDouble( v2 );
		
		for(i = 0; i < vec->size; i++) {
			vec->vec[i] *=	scalar;
		}
		
		Py_INCREF( v1 );
		return v1;
		
	} else if (MatrixObject_Check(v2)) {
		float vecCopy[4];
		int x,y, size = vec->size;
		MatrixObject *mat= (MatrixObject*)v2;
		
		if(mat->colSize != size){
			if(mat->rowSize == 4 && vec->size != 3){
				return EXPP_ReturnPyObjError(PyExc_AttributeError, 
					"vector * matrix: matrix column size and the vector size must be the same");
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
		Py_INCREF( v1 );
		return v1;
	}
	return EXPP_ReturnPyObjError(PyExc_TypeError,
		"Vector multiplication: arguments not acceptable for this operation\n");
}

/*------------------------obj / obj------------------------------
  divide*/
static PyObject *Vector_div(PyObject * v1, PyObject * v2)
{
	int i, size;
	float vec[4], scalar;
	VectorObject *vec1 = NULL;
	
	if(!VectorObject_Check(v1)) /* not a vector */
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Vector division: Vector must be divided by a float\n");
	
	vec1 = (VectorObject*)v1; /* vector */
	
	if(!PyNumber_Check(v2)) /* parsed item not a number */
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Vector division: Vector must be divided by a float\n");

	scalar = (float)PyFloat_AsDouble(v2);
	
	if(scalar==0.0) /* not a vector */
		return EXPP_ReturnPyObjError(PyExc_ZeroDivisionError, 
			"Vector division: divide by zero error.\n");
	
	size = vec1->size;
	for(i = 0; i < size; i++) {
		vec[i] = vec1->vec[i] /	scalar;
	}
	return newVectorObject(vec, size, Py_NEW);
}

/*------------------------obj / obj------------------------------
  divide*/
static PyObject *Vector_idiv(PyObject * v1, PyObject * v2)
{
	int i, size;
	float scalar;
	VectorObject *vec1 = NULL;
	
	/*if(!VectorObject_Check(v1))
		return EXPP_ReturnIntError(PyExc_TypeError, 
			"Vector division: Vector must be divided by a float\n");*/
	
	vec1 = (VectorObject*)v1; /* vector */
	
	if(!PyNumber_Check(v2)) /* parsed item not a number */
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Vector division: Vector must be divided by a float\n");

	scalar = (float)PyFloat_AsDouble(v2);
	
	if(scalar==0.0) /* not a vector */
		return EXPP_ReturnPyObjError(PyExc_ZeroDivisionError, 
			"Vector division: divide by zero error.\n");
	
	size = vec1->size;
	for(i = 0; i < size; i++) {
		vec1->vec[i] /=	scalar;
	}
	Py_INCREF( v1 );
	return v1;
}

/*-------------------------- -obj -------------------------------
  returns the negative of this object*/
static PyObject *Vector_neg(VectorObject *self)
{
	int i;
	float vec[4];
	for(i = 0; i < self->size; i++){
		vec[i] = -self->vec[i];
	}

	return newVectorObject(vec, self->size, Py_NEW);
}
/*------------------------coerce(obj, obj)-----------------------
  coercion of unknown types to type VectorObject for numeric protocols
  Coercion() is called whenever a math operation has 2 operands that
 it doesn't understand how to evaluate. 2+Matrix for example. We want to 
 evaluate some of these operations like: (vector * 2), however, for math
 to proceed, the unknown operand must be cast to a type that python math will
 understand. (e.g. in the case above case, 2 must be cast to a vector and 
 then call vector.multiply(vector, scalar_cast_as_vector)*/


static int Vector_coerce(PyObject ** v1, PyObject ** v2)
{
	/* Just incref, each functon must raise errors for bad types */
	Py_INCREF (*v1);
	Py_INCREF (*v2);
	return 0;
}


/*------------------------tp_doc*/
static char VectorObject_doc[] = "This is a wrapper for vector objects.";
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
PyObject* Vector_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	VectorObject *vecA = NULL, *vecB = NULL;
	int result = 0;
	float epsilon = .000001f;
	double lenA,lenB;

	if (!VectorObject_Check(objectA) || !VectorObject_Check(objectB)){
		if (comparison_type == Py_NE){
			return EXPP_incr_ret(Py_True); 
		}else{
			return EXPP_incr_ret(Py_False);
		}
	}
	vecA = (VectorObject*)objectA;
	vecB = (VectorObject*)objectB;

	if (vecA->size != vecB->size){
		if (comparison_type == Py_NE){
			return EXPP_incr_ret(Py_True); 
		}else{
			return EXPP_incr_ret(Py_False);
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
		return EXPP_incr_ret(Py_True);
	}else{
		return EXPP_incr_ret(Py_False);
	}
}
/*-----------------PROTCOL DECLARATIONS--------------------------*/
static PySequenceMethods Vector_SeqMethods = {
	(inquiry) Vector_len,						/* sq_length */
	(binaryfunc) 0,								/* sq_concat */
	(intargfunc) 0,								/* sq_repeat */
	(intargfunc) Vector_item,					/* sq_item */
	(intintargfunc) Vector_slice,				/* sq_slice */
	(intobjargproc) Vector_ass_item,			/* sq_ass_item */
	(intintobjargproc) Vector_ass_slice,		/* sq_ass_slice */
};


/* For numbers without flag bit Py_TPFLAGS_CHECKTYPES set, all
   arguments are guaranteed to be of the object's type (modulo
   coercion hacks -- i.e. if the type's coercion function
   returns other types, then these are allowed as well).  Numbers that
   have the Py_TPFLAGS_CHECKTYPES flag bit set should check *both*
   arguments for proper type and implement the necessary conversions
   in the slot functions themselves. */
 
static PyNumberMethods Vector_NumMethods = {
	(binaryfunc) Vector_add,					/* __add__ */
	(binaryfunc) Vector_sub,					/* __sub__ */
	(binaryfunc) Vector_mul,					/* __mul__ */
	(binaryfunc) Vector_div,					/* __div__ */
	(binaryfunc) NULL,							/* __mod__ */
	(binaryfunc) NULL,							/* __divmod__ */
	(ternaryfunc) NULL,							/* __pow__ */
	(unaryfunc) Vector_neg,						/* __neg__ */
	(unaryfunc) NULL,							/* __pos__ */
	(unaryfunc) NULL,							/* __abs__ */
	(inquiry) NULL,								/* __nonzero__ */
	(unaryfunc) NULL,							/* __invert__ */
	(binaryfunc) NULL,							/* __lshift__ */
	(binaryfunc) NULL,							/* __rshift__ */
	(binaryfunc) NULL,							/* __and__ */
	(binaryfunc) NULL,							/* __xor__ */
	(binaryfunc) NULL,							/* __or__ */
	(coercion)  Vector_coerce,					/* __coerce__ */
	(unaryfunc) NULL,							/* __int__ */
	(unaryfunc) NULL,							/* __long__ */
	(unaryfunc) NULL,							/* __float__ */
	(unaryfunc) NULL,							/* __oct__ */
	(unaryfunc) NULL,							/* __hex__ */
	
	/* Added in release 2.0 */
	(binaryfunc) Vector_iadd,					/*__iadd__*/
	(binaryfunc) Vector_isub,					/*__isub__*/
	(binaryfunc) Vector_imul,					/*__imul__*/
	(binaryfunc) Vector_idiv,					/*__idiv__*/
	(binaryfunc) NULL,							/*__imod__*/
	(ternaryfunc) NULL,							/*__ipow__*/
	(binaryfunc) NULL,							/*__ilshift__*/
	(binaryfunc) NULL,							/*__irshift__*/
	(binaryfunc) NULL,							/*__iand__*/
	(binaryfunc) NULL,							/*__ixor__*/
	(binaryfunc) NULL,							/*__ior__*/
 
	/* Added in release 2.2 */
	/* The following require the Py_TPFLAGS_HAVE_CLASS flag */
	(binaryfunc) NULL,							/*__floordiv__  __rfloordiv__*/
	(binaryfunc) NULL,							/*__truediv__ __rfloordiv__*/
	(binaryfunc) NULL,							/*__ifloordiv__*/
	(binaryfunc) NULL,							/*__itruediv__*/
};
/*------------------PY_OBECT DEFINITION--------------------------*/

/*
 * vector axis, vector.x/y/z/w
 */
	
static PyObject *Vector_getAxis( VectorObject * self, void *type )
{
	switch( (long)type ) {
    case 'X':	/* these are backwards, but that how it works */
		return PyFloat_FromDouble(self->vec[0]);
    case 'Y':
		return PyFloat_FromDouble(self->vec[1]);
    case 'Z':	/* these are backwards, but that how it works */
		if(self->size < 3)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"vector.z: error, cannot get this axis for a 2D vector\n");
		else
			return PyFloat_FromDouble(self->vec[2]);
    case 'W':
		if(self->size < 4)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"vector.w: error, cannot get this axis for a 3D vector\n");
	
		return PyFloat_FromDouble(self->vec[3]);
	default:
		{
			char errstr[1024];
			sprintf( errstr, "undefined type '%d' in Vector_getAxis",
					(int)((long)type & 0xff));
			return EXPP_ReturnPyObjError( PyExc_RuntimeError, errstr );
		}
	}
}

static int Vector_setAxis( VectorObject * self, PyObject * value, void * type )
{
	float param;
	
	if (!PyNumber_Check(value))
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a number for the vector axis" );
	
	param= (float)PyFloat_AsDouble( value );
	
	switch( (long)type ) {
    case 'X':	/* these are backwards, but that how it works */
		self->vec[0]= param;
		break;
    case 'Y':
		self->vec[1]= param;
		break;
    case 'Z':	/* these are backwards, but that how it works */
		if(self->size < 3)
			return EXPP_ReturnIntError(PyExc_AttributeError,
				"vector.z: error, cannot get this axis for a 2D vector\n");
		self->vec[2]= param;
		break;
    case 'W':
		if(self->size < 4)
			return EXPP_ReturnIntError(PyExc_AttributeError,
				"vector.w: error, cannot get this axis for a 3D vector\n");
	
		self->vec[3]= param;
		break;
	default:
		{
			char errstr[1024];
			sprintf( errstr, "undefined type '%d' in Vector_setAxis",
					(int)((long)type & 0xff));
			return EXPP_ReturnIntError( PyExc_RuntimeError, errstr );
		}
	}

	return 0;
}

/* vector.length */
static PyObject *Vector_getLength( VectorObject * self, void *type )
{
	double dot = 0.0f;
	int i;
	
	for(i = 0; i < self->size; i++){
		dot += (self->vec[i] * self->vec[i]);
	}
	return PyFloat_FromDouble(sqrt(dot));
}

static int Vector_setLength( VectorObject * self, PyObject * value )
{
	double dot = 0.0f, param;
	int i;
	
	if (!PyNumber_Check(value))
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a number for the vector axis" );
	
	param= PyFloat_AsDouble( value );
	
	if (param < 0)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"cannot set a vectors length to a negative value" );
	
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
	
	return 0;
}

static PyObject *Vector_getWrapped( VectorObject * self, void *type )
{
	if (self->wrapped == Py_WRAP)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}


/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Vector_getseters[] = {
	{"x",
	 (getter)Vector_getAxis, (setter)Vector_setAxis,
	 "Vector X axis",
	 (void *)'X'},
	{"y",
	 (getter)Vector_getAxis, (setter)Vector_setAxis,
	 "Vector Y axis",
	 (void *)'Y'},
	{"z",
	 (getter)Vector_getAxis, (setter)Vector_setAxis,
	 "Vector Z axis",
	 (void *)'Z'},
	{"w",
	 (getter)Vector_getAxis, (setter)Vector_setAxis,
	 "Vector Z axis",
	 (void *)'W'},
	{"length",
	 (getter)Vector_getLength, (setter)Vector_setLength,
	 "Vector Length",
	 NULL},
	{"magnitude",
	 (getter)Vector_getLength, (setter)Vector_setLength,
	 "Vector Length",
	 NULL},
	{"wrapped",
	 (getter)Vector_getWrapped, (setter)NULL,
	 "Vector Length",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};



/* Note
 Py_TPFLAGS_CHECKTYPES allows us to avoid casting all types to Vector when coercing
 but this means for eg that 
 vec*mat and mat*vec both get sent to Vector_mul and it neesd to sort out the order
*/

PyTypeObject vector_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Vector",             /* char *tp_name; */
	sizeof( VectorObject ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) Vector_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,   /* cmpfunc tp_compare; */
	( reprfunc ) Vector_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	&Vector_NumMethods,                       /* PyNumberMethods *tp_as_number; */
	&Vector_SeqMethods,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,         /* long tp_flags; */

	VectorObject_doc,                       /*  char *tp_doc;  Documentation string */
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
	Vector_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
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
PyObject *newVectorObject(float *vec, int size, int type)
{
	int i;
	VectorObject *self = PyObject_NEW(VectorObject, &vector_Type);
	
	if(size > 4 || size < 2)
		return NULL;
	self->size = size;

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

/*
  #############################DEPRECATED################################
  #######################################################################
  ----------------------------Vector.negate() --------------------
  set the vector to it's negative -x, -y, -z */
PyObject *Vector_Negate(VectorObject * self)
{
	int i;
	for(i = 0; i < self->size; i++) {
		self->vec[i] = -(self->vec[i]);
	}
	/*printf("Vector.negate(): Deprecated: use -vector instead\n");*/
	return EXPP_incr_ret((PyObject*)self);
}
/*###################################################################
  ###########################DEPRECATED##############################*/

