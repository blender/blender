/*
 * $Id$
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
 * 
 * Contributor(s): Willian P. Germano & Joseph Gilbert, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Mathutils.h"

#include "BLI_blenlib.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"
#include "gen_utils.h"


//-------------------------DOC STRINGS ---------------------------
char Vector_Zero_doc[] = "() - set all values in the vector to 0";
char Vector_Normalize_doc[] = "() - normalize the vector";
char Vector_Negate_doc[] = "() - changes vector to it's additive inverse";
char Vector_Resize2D_doc[] = "() - resize a vector to [x,y]";
char Vector_Resize3D_doc[] = "() - resize a vector to [x,y,z]";
char Vector_Resize4D_doc[] = "() - resize a vector to [x,y,z,w]";
char Vector_toPoint_doc[] = "() - create a new Point Object from this vector";
char Vector_ToTrackQuat_doc[] = "(track, up) - extract a quaternion from the vector and the track and up axis";
//-----------------------METHOD DEFINITIONS ----------------------
struct PyMethodDef Vector_methods[] = {
	{"zero", (PyCFunction) Vector_Zero, METH_NOARGS, Vector_Zero_doc},
	{"normalize", (PyCFunction) Vector_Normalize, METH_NOARGS, Vector_Normalize_doc},
	{"negate", (PyCFunction) Vector_Negate, METH_NOARGS, Vector_Negate_doc},
	{"resize2D", (PyCFunction) Vector_Resize2D, METH_NOARGS, Vector_Resize2D_doc},
	{"resize3D", (PyCFunction) Vector_Resize3D, METH_NOARGS, Vector_Resize2D_doc},
	{"resize4D", (PyCFunction) Vector_Resize4D, METH_NOARGS, Vector_Resize2D_doc},
	{"toPoint", (PyCFunction) Vector_toPoint, METH_NOARGS, Vector_toPoint_doc},
	{"toTrackQuat", ( PyCFunction ) Vector_ToTrackQuat, METH_VARARGS, Vector_ToTrackQuat_doc},
	{NULL, NULL, 0, NULL}
};
//-----------------------------METHODS----------------------------
//--------------------------Vector.toPoint()----------------------
//create a new point object to represent this vector
PyObject *Vector_toPoint(VectorObject * self)
{
	float coord[3];
	int x;

	if(self->size < 2 || self->size > 3) {
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector.toPoint(): inappropriate vector size - expects 2d or 3d vector\n");
	} 
	for(x = 0; x < self->size; x++){
		coord[x] = self->vec[x];
	}
	
	return newPointObject(coord, self->size, Py_NEW);
}
//----------------------------Vector.zero() ----------------------
//set the vector data to 0,0,0
PyObject *Vector_Zero(VectorObject * self)
{
	int x;
	for(x = 0; x < self->size; x++) {
		self->vec[x] = 0.0f;
	}
	return EXPP_incr_ret((PyObject*)self);
}
//----------------------------Vector.normalize() -----------------
//normalize the vector data to a unit vector
PyObject *Vector_Normalize(VectorObject * self)
{
	int x;
	float norm = 0.0f;

	for(x = 0; x < self->size; x++) {
		norm += self->vec[x] * self->vec[x];
	}
	norm = (float) sqrt(norm);
	for(x = 0; x < self->size; x++) {
		self->vec[x] /= norm;
	}
	return EXPP_incr_ret((PyObject*)self);
}
//----------------------------Vector.resize2D() ------------------
//resize the vector to x,y
PyObject *Vector_Resize2D(VectorObject * self)
{
	if(self->data.blend_data){
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector.resize2d(): cannot resize wrapped data - only python vectors\n");
	}

	self->data.py_data = 
		PyMem_Realloc(self->data.py_data, (sizeof(float) * 2));
	if(self->data.py_data == NULL) {
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"vector.resize2d(): problem allocating pointer space\n\n");
	}
	self->vec = self->data.py_data;  //force
	self->size = 2;
	return EXPP_incr_ret((PyObject*)self);
}
//----------------------------Vector.resize3D() ------------------
//resize the vector to x,y,z
PyObject *Vector_Resize3D(VectorObject * self)
{
	if(self->data.blend_data){
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector.resize3d(): cannot resize wrapped data - only python vectors\n");
	}

	self->data.py_data = 
		PyMem_Realloc(self->data.py_data, (sizeof(float) * 3));
	if(self->data.py_data == NULL) {
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"vector.resize3d(): problem allocating pointer space\n\n");
	}
	self->vec = self->data.py_data;  //force
	if(self->size == 2){
		self->data.py_data[2] = 0.0f;
	}
	self->size = 3;
	return EXPP_incr_ret((PyObject*)self);
}
//----------------------------Vector.resize4D() ------------------
//resize the vector to x,y,z,w
PyObject *Vector_Resize4D(VectorObject * self)
{
	if(self->data.blend_data){
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector.resize4d(): cannot resize wrapped data - only python vectors\n");
	}

	self->data.py_data = 
		PyMem_Realloc(self->data.py_data, (sizeof(float) * 4));
	if(self->data.py_data == NULL) {
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"vector.resize4d(): problem allocating pointer space\n\n");
	}
	self->vec = self->data.py_data;  //force
	if(self->size == 2){
		self->data.py_data[2] = 0.0f;
		self->data.py_data[3] = 0.0f;
	}else if(self->size == 3){
		self->data.py_data[3] = 0.0f;
	}
	self->size = 4;
	return EXPP_incr_ret((PyObject*)self);
}
//----------------------------Vector.toTrackQuat(track, up) ----------------------
//extract a quaternion from the vector and the track and up axis
PyObject *Vector_ToTrackQuat( VectorObject * self, PyObject * args )
{
	float vec[3];
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

	return newQuaternionObject(vectoquat(vec, track, up), Py_NEW);
}
//----------------------------dealloc()(internal) ----------------
//free the py_object
static void Vector_dealloc(VectorObject * self)
{
	Py_XDECREF(self->coerced_object);
	//only free py_data
	if(self->data.py_data){
		PyMem_Free(self->data.py_data);
	}
	PyObject_DEL(self);
}
//----------------------------getattr()(internal) ----------------
//object.attribute access (get)
static PyObject *Vector_getattr(VectorObject * self, char *name)
{
	int x;
	double dot = 0.0f;

	if(STREQ(name,"x")){
		return PyFloat_FromDouble(self->vec[0]);
	}else if(STREQ(name, "y")){
		return PyFloat_FromDouble(self->vec[1]);
	}else if(STREQ(name, "z")){
		if(self->size > 2){
			return PyFloat_FromDouble(self->vec[2]);
		}else{
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"vector.z: illegal attribute access\n");
		}
	}else if(STREQ(name, "w")){
		if(self->size > 3){
			return PyFloat_FromDouble(self->vec[3]);
		}else{
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"vector.w: illegal attribute access\n");
		}
	}else if(STREQ2(name, "length", "magnitude")) {
		for(x = 0; x < self->size; x++){
			dot += (self->vec[x] * self->vec[x]);
		}
		return PyFloat_FromDouble(sqrt(dot));
	}
	if(STREQ(name, "wrapped")){
		if(self->wrapped == Py_WRAP)
			return EXPP_incr_ret((PyObject *)Py_True);
		else 
			return EXPP_incr_ret((PyObject *)Py_False);
	}
	return Py_FindMethod(Vector_methods, (PyObject *) self, name);
}
//----------------------------setattr()(internal) ----------------
//object.attribute access (set)
static int Vector_setattr(VectorObject * self, char *name, PyObject * v)
{
	PyObject *f = NULL;

	f = PyNumber_Float(v);
	if(f == NULL) { // parsed item not a number
		return EXPP_ReturnIntError(PyExc_TypeError, 
			"vector.attribute = x: argument not a number\n");
	}

	if(STREQ(name,"x")){
		self->vec[0] = (float)PyFloat_AS_DOUBLE(f);
	}else if(STREQ(name, "y")){
		self->vec[1] = (float)PyFloat_AS_DOUBLE(f);
	}else if(STREQ(name, "z")){
		if(self->size > 2){
			self->vec[2] = (float)PyFloat_AS_DOUBLE(f);
		}else{
			Py_DECREF(f);
			return EXPP_ReturnIntError(PyExc_AttributeError,
				"vector.z = x: illegal attribute access\n");
		}
	}else if(STREQ(name, "w")){
		if(self->size > 3){
			self->vec[3] = (float)PyFloat_AS_DOUBLE(f);
		}else{
			Py_DECREF(f);
			return EXPP_ReturnIntError(PyExc_AttributeError,
				"vector.w = x: illegal attribute access\n");
		}
	}else{
		Py_DECREF(f);
		return EXPP_ReturnIntError(PyExc_AttributeError,
				"vector.attribute = x: unknown attribute\n");
	}

	Py_DECREF(f);
	return 0;
}
//----------------------------print object (internal)-------------
//print the object to screen
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
//---------------------SEQUENCE PROTOCOLS------------------------
//----------------------------len(object)------------------------
//sequence length
static int Vector_len(VectorObject * self)
{
	return self->size;
}
//----------------------------object[]---------------------------
//sequence accessor (get)
static PyObject *Vector_item(VectorObject * self, int i)
{
	if(i < 0 || i >= self->size)
		return EXPP_ReturnPyObjError(PyExc_IndexError,
		"vector[attribute]: array index out of range\n");

	return Py_BuildValue("f", self->vec[i]);

}
//----------------------------object[]-------------------------
//sequence accessor (set)
static int Vector_ass_item(VectorObject * self, int i, PyObject * ob)
{
	PyObject *f = NULL;

	f = PyNumber_Float(ob);
	if(f == NULL) { // parsed item not a number
		return EXPP_ReturnIntError(PyExc_TypeError, 
			"vector[attribute] = x: argument not a number\n");
	}

	if(i < 0 || i >= self->size){
		Py_DECREF(f);
		return EXPP_ReturnIntError(PyExc_IndexError,
			"vector[attribute] = x: array assignment index out of range\n");
	}
	self->vec[i] = (float)PyFloat_AS_DOUBLE(f);
	Py_DECREF(f);
	return 0;
}
//----------------------------object[z:y]------------------------
//sequence slice (get)
static PyObject *Vector_slice(VectorObject * self, int begin, int end)
{
	PyObject *list = NULL;
	int count;

	CLAMP(begin, 0, self->size);
	CLAMP(end, 0, self->size);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SetItem(list, count - begin,
				PyFloat_FromDouble(self->vec[count]));
	}

	return list;
}
//----------------------------object[z:y]------------------------
//sequence slice (set)
static int Vector_ass_slice(VectorObject * self, int begin, int end,
			     PyObject * seq)
{
	int i, y, size = 0;
	float vec[4];
	PyObject *v, *f;

	CLAMP(begin, 0, self->size);
	CLAMP(end, 0, self->size);
	begin = MIN2(begin,end);

	size = PySequence_Length(seq);
	if(size != (end - begin)){
		return EXPP_ReturnIntError(PyExc_TypeError,
			"vector[begin:end] = []: size mismatch in slice assignment\n");
	}

	for (i = 0; i < size; i++) {
		v = PySequence_GetItem(seq, i);
		if (v == NULL) { // Failed to read sequence
			return EXPP_ReturnIntError(PyExc_RuntimeError, 
				"vector[begin:end] = []: unable to read sequence\n");
		}

		f = PyNumber_Float(v);
		if(f == NULL) { // parsed item not a number
			Py_DECREF(v);
			return EXPP_ReturnIntError(PyExc_TypeError, 
				"vector[begin:end] = []: sequence argument not a number\n");
		}

		vec[i] = (float)PyFloat_AS_DOUBLE(f);
		EXPP_decr2(f,v);
	}
	//parsed well - now set in vector
	for(y = 0; y < size; y++){
		self->vec[begin + y] = vec[y];
	}
	return 0;
}
//------------------------NUMERIC PROTOCOLS----------------------
//------------------------obj + obj------------------------------
//addition
static PyObject *Vector_add(PyObject * v1, PyObject * v2)
{
	int x, size;
	float vec[4];
	VectorObject *vec1 = NULL, *vec2 = NULL;
	PointObject *pt = NULL;

	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(!vec1->coerced_object){
		if(vec2->coerced_object){
			if(PointObject_Check(vec2->coerced_object)){  //VECTOR + POINT
				//Point translation
				pt = (PointObject*)vec2->coerced_object;
				size = vec1->size;
				if(pt->size == size){
					for(x = 0; x < size; x++){
						vec[x] = vec1->vec[x] + pt->coord[x];
					}	
				}else{
					return EXPP_ReturnPyObjError(PyExc_AttributeError,
						"Vector addition: arguments are the wrong size....\n");
				}
				return newPointObject(vec, size, Py_NEW);
			}
		}else{ //VECTOR + VECTOR
			if(vec1->size != vec2->size){
				return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Vector addition: vectors must have the same dimensions for this operation\n");
			}
			size = vec1->size;
			for(x = 0; x < size; x++) {
				vec[x] = vec1->vec[x] +	vec2->vec[x];
			}
			return newVectorObject(vec, size, Py_NEW);
		}
	}

	return EXPP_ReturnPyObjError(PyExc_AttributeError,
		"Vector addition: arguments not valid for this operation....\n");
}
//------------------------obj - obj------------------------------
//subtraction
static PyObject *Vector_sub(PyObject * v1, PyObject * v2)
{
	int x, size;
	float vec[4];
	VectorObject *vec1 = NULL, *vec2 = NULL;

	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(vec1->coerced_object || vec2->coerced_object){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Vector subtraction: arguments not valid for this operation....\n");
	}
	if(vec1->size != vec2->size){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		"Vector subtraction: vectors must have the same dimensions for this operation\n");
	}

	size = vec1->size;
	for(x = 0; x < size; x++) {
		vec[x] = vec1->vec[x] -	vec2->vec[x];
	}

	return newVectorObject(vec, size, Py_NEW);
}
//------------------------obj * obj------------------------------
//mulplication
static PyObject *Vector_mul(PyObject * v1, PyObject * v2)
{
	int x, size;
	float vec[4], scalar;
	double dot = 0.0f;
	VectorObject *vec1 = NULL, *vec2 = NULL;
	PyObject *f = NULL, *retObj = NULL;
	MatrixObject *mat = NULL;
	QuaternionObject *quat = NULL;

	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(vec1->coerced_object){
		if (PyFloat_Check(vec1->coerced_object) || 
			PyInt_Check(vec1->coerced_object)){	// FLOAT/INT * VECTOR
			f = PyNumber_Float(vec1->coerced_object);
			if(f == NULL) { // parsed item not a number
				return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"Vector multiplication: arguments not acceptable for this operation\n");
			}

			scalar = (float)PyFloat_AS_DOUBLE(f);
			size = vec2->size;
			for(x = 0; x < size; x++) {
				vec[x] = vec2->vec[x] *	scalar;
			}
			Py_DECREF(f);
			return newVectorObject(vec, size, Py_NEW);
		}
	}else{
		if(vec2->coerced_object){
			if(MatrixObject_Check(vec2->coerced_object)){ //VECTOR * MATRIX
				mat = (MatrixObject*)vec2->coerced_object;
				return retObj = row_vector_multiplication(vec1, mat);
			}else if (PyFloat_Check(vec2->coerced_object) || 
				PyInt_Check(vec2->coerced_object)){	// VECTOR * FLOAT/INT
				f = PyNumber_Float(vec2->coerced_object);
				if(f == NULL) { // parsed item not a number
					return EXPP_ReturnPyObjError(PyExc_TypeError, 
						"Vector multiplication: arguments not acceptable for this operation\n");
				}

				scalar = (float)PyFloat_AS_DOUBLE(f);
				size = vec1->size;
				for(x = 0; x < size; x++) {
					vec[x] = vec1->vec[x] *	scalar;
				}
				Py_DECREF(f);
				return newVectorObject(vec, size, Py_NEW);
			}else if(QuaternionObject_Check(vec2->coerced_object)){  //VECTOR * QUATERNION
				quat = (QuaternionObject*)vec2->coerced_object;
				if(vec1->size != 3){
					return EXPP_ReturnPyObjError(PyExc_TypeError, 
						"Vector multiplication: only 3D vector rotations (with quats) currently supported\n");
				}
				return quat_rotation((PyObject*)vec1, (PyObject*)quat);
			}
		}else{  //VECTOR * VECTOR
			if(vec1->size != vec2->size){
				return EXPP_ReturnPyObjError(PyExc_AttributeError,
					"Vector multiplication: vectors must have the same dimensions for this operation\n");
			}
			size = vec1->size;
			//dot product
			for(x = 0; x < size; x++) {
				dot += vec1->vec[x] * vec2->vec[x];
			}
			return PyFloat_FromDouble(dot);
		}
	}

	return EXPP_ReturnPyObjError(PyExc_TypeError, 
		"Vector multiplication: arguments not acceptable for this operation\n");
}
//-------------------------- -obj -------------------------------
//returns the negative of this object
static PyObject *Vector_neg(VectorObject *self)
{
	int x;
	for(x = 0; x < self->size; x++){
		self->vec[x] = -self->vec[x];
	}

	return EXPP_incr_ret((PyObject *)self);
}
//------------------------coerce(obj, obj)-----------------------
//coercion of unknown types to type VectorObject for numeric protocols
/*Coercion() is called whenever a math operation has 2 operands that
 it doesn't understand how to evaluate. 2+Matrix for example. We want to 
 evaluate some of these operations like: (vector * 2), however, for math
 to proceed, the unknown operand must be cast to a type that python math will
 understand. (e.g. in the case above case, 2 must be cast to a vector and 
 then call vector.multiply(vector, scalar_cast_as_vector)*/
static int Vector_coerce(PyObject ** v1, PyObject ** v2)
{
	PyObject *coerced = NULL;

	if(!VectorObject_Check(*v2)) {
		if(MatrixObject_Check(*v2) || PyFloat_Check(*v2) || PyInt_Check(*v2) || 
			QuaternionObject_Check(*v2) || PointObject_Check(*v2)) {
			coerced = EXPP_incr_ret(*v2);
			*v2 = newVectorObject(NULL,3,Py_NEW);
			((VectorObject*)*v2)->coerced_object = coerced;
		}else{
			return EXPP_ReturnIntError(PyExc_TypeError, 
				"vector.coerce(): unknown operand - can't coerce for numeric protocols\n");
		}
	}
	EXPP_incr2(*v1, *v2);
	return 0;
}
//------------------------tp_doc
static char VectorObject_doc[] = "This is a wrapper for vector objects.";
//------------------------vec_magnitude (internal)
static double vec_magnitude(float *data, int size)
{
	double dot = 0.0f;
	int i;

	for(i=0; i<size; i++){
		dot += data[i];
	}
	return (double)sqrt(dot);
}
//------------------------vec_equality(internal)
static int vec_equality(float *dataA, float *dataB, int size, double epsilon)
{
	int i;

	for(i=0; i<size; i++){
		if(!(((dataA[i] + epsilon) > dataB[i]) && ((dataA[i] - epsilon) < dataB[i]))){
			return 0;
		}
	}
	return 1;
}
//------------------------tp_richcmpr
//returns -1 execption, 0 false, 1 true
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
			lenA = vec_magnitude(vecA->vec, vecA->size);
			lenB = vec_magnitude(vecB->vec, vecB->size);
			if( lenA < lenB ){
				result = 1;
			}
			break;
		case Py_LE:
			lenA = vec_magnitude(vecA->vec, vecA->size);
			lenB = vec_magnitude(vecB->vec, vecB->size);
			if( lenA < lenB ){
				result = 1;
			}else{
				result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
			}
			break;
		case Py_EQ:
			result = vec_equality(vecA->vec, vecB->vec, vecA->size, epsilon);
			break;
		case Py_NE:
			result = vec_equality(vecA->vec, vecB->vec, vecA->size, epsilon);
			if (result == 0){
				result = 1;
			}else{
				result = 0;
			}
			break;
		case Py_GT:
			lenA = vec_magnitude(vecA->vec, vecA->size);
			lenB = vec_magnitude(vecB->vec, vecB->size);
			if( lenA > lenB ){
				result = 1;
			}
			break;
		case Py_GE:
			lenA = vec_magnitude(vecA->vec, vecA->size);
			lenB = vec_magnitude(vecB->vec, vecB->size);
			if( lenA > lenB ){
				result = 1;
			}else{
				result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
			}
			break;
		default:
			break;
	}
	if (result == 1){
		return EXPP_incr_ret(Py_True);
	}else{
		return EXPP_incr_ret(Py_False);
	}
}
//-----------------PROTCOL DECLARATIONS--------------------------
static PySequenceMethods Vector_SeqMethods = {
	(inquiry) Vector_len,						/* sq_length */
	(binaryfunc) 0,								/* sq_concat */
	(intargfunc) 0,								/* sq_repeat */
	(intargfunc) Vector_item,					/* sq_item */
	(intintargfunc) Vector_slice,				/* sq_slice */
	(intobjargproc) Vector_ass_item,			/* sq_ass_item */
	(intintobjargproc) Vector_ass_slice,		/* sq_ass_slice */
};
static PyNumberMethods Vector_NumMethods = {
	(binaryfunc) Vector_add,					/* __add__ */
	(binaryfunc) Vector_sub,					/* __sub__ */
	(binaryfunc) Vector_mul,					/* __mul__ */
	(binaryfunc) 0,								/* __div__ */
	(binaryfunc) 0,								/* __mod__ */
	(binaryfunc) 0,								/* __divmod__ */
	(ternaryfunc) 0,							/* __pow__ */
	(unaryfunc) Vector_neg,						/* __neg__ */
	(unaryfunc) 0,								/* __pos__ */
	(unaryfunc) 0,								/* __abs__ */
	(inquiry) 0,								/* __nonzero__ */
	(unaryfunc) 0,								/* __invert__ */
	(binaryfunc) 0,								/* __lshift__ */
	(binaryfunc) 0,								/* __rshift__ */
	(binaryfunc) 0,								/* __and__ */
	(binaryfunc) 0,								/* __xor__ */
	(binaryfunc) 0,								/* __or__ */
	(coercion)  Vector_coerce,					/* __coerce__ */
	(unaryfunc) 0,								/* __int__ */
	(unaryfunc) 0,								/* __long__ */
	(unaryfunc) 0,								/* __float__ */
	(unaryfunc) 0,								/* __oct__ */
	(unaryfunc) 0,								/* __hex__ */

};
//------------------PY_OBECT DEFINITION--------------------------
PyTypeObject vector_Type = {
	PyObject_HEAD_INIT(NULL)		//tp_head
	0,								//tp_internal
	"vector",						//tp_name
	sizeof(VectorObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)Vector_dealloc,		//tp_dealloc
	0,								//tp_print
	(getattrfunc)Vector_getattr,	//tp_getattr
	(setattrfunc) Vector_setattr,	//tp_setattr
	0,								//tp_compare
	(reprfunc) Vector_repr,			//tp_repr
	&Vector_NumMethods,				//tp_as_number
	&Vector_SeqMethods,				//tp_as_sequence
	0,								//tp_as_mapping
	0,								//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT,				//tp_flags
	VectorObject_doc,				//tp_doc
	0,								//tp_traverse
	0,								//tp_clear
	(richcmpfunc)Vector_richcmpr,	//tp_richcompare
	0,								//tp_weaklistoffset
	0,								//tp_iter
	0,								//tp_iternext
	0,								//tp_methods
	0,								//tp_members
	0,								//tp_getset
	0,								//tp_base
	0,								//tp_dict
	0,								//tp_descr_get
	0,								//tp_descr_set
	0,								//tp_dictoffset
	0,								//tp_init
	0,								//tp_alloc
	0,								//tp_new
	0,								//tp_free
	0,								//tp_is_gc
	0,								//tp_bases
	0,								//tp_mro
	0,								//tp_cache
	0,								//tp_subclasses
	0,								//tp_weaklist
	0								//tp_del
};
//------------------------newVectorObject (internal)-------------
//creates a new vector object
/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newVectorObject(float *vec, int size, int type)
{
	VectorObject *self;
	int x;

	vector_Type.ob_type = &PyType_Type;
	self = PyObject_NEW(VectorObject, &vector_Type);
	self->data.blend_data = NULL;
	self->data.py_data = NULL;
	if(size > 4 || size < 2)
		return NULL;
	self->size = size;
	self->coerced_object = NULL;

	if(type == Py_WRAP){
		self->data.blend_data = vec;
		self->vec = self->data.blend_data;
		self->wrapped = Py_WRAP;
	}else if (type == Py_NEW){
		self->data.py_data = PyMem_Malloc(size * sizeof(float));
		self->vec = self->data.py_data;
		if(!vec) { //new empty
			for(x = 0; x < size; x++){
				self->vec[x] = 0.0f;
			}
			if(size == 4)  /* do the homogenous thing */
				self->vec[3] = 1.0f;
		}else{
			for(x = 0; x < size; x++){
				self->vec[x] = vec[x];
			}
		}
		self->wrapped = Py_NEW;
	}else{ //bad type
		return NULL;
	}
	return (PyObject *) self;
}

//#############################DEPRECATED################################
//#######################################################################
//----------------------------Vector.negate() --------------------
//set the vector to it's negative -x, -y, -z
PyObject *Vector_Negate(VectorObject * self)
{
	int x;
	for(x = 0; x < self->size; x++) {
		self->vec[x] = -(self->vec[x]);
	}
	printf("Vector.negate(): Deprecated: use -vector instead\n");
	return EXPP_incr_ret((PyObject*)self);
}
//#######################################################################
//#############################DEPRECATED################################

