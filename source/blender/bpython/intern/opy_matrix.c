/*  python.c      MIXED MODEL
 * 
 *  june 99
 * $Id$
 *
 * this code might die...
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Python.h"
#include "BPY_tools.h"
#include "BPY_macros.h"

#include "BLI_arithb.h"
#include "opy_vector.h"

PyObject *BPY_tuple_repr(PyObject *self, int size);

/* PROTOS */
// PyObject *newVectorObject(float *vec, int size);

/*****************************/
/*    Matrix Python Object   */
/*****************************/


#define GETROWVECTOR(mat, i) ( (float *) mat[i])

static void Matrix_dealloc(MatrixObject *self) {
	Py_DECREF(self->rows[0]);
	Py_DECREF(self->rows[1]);
	Py_DECREF(self->rows[2]);
	Py_DECREF(self->rows[3]);

	PyMem_DEL(self);
}

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Matrix)

static char Matrix_inverse_doc[] = "() - returns inverse of matrix";

static PyObject *Matrix_inverse(PyObject *self, PyObject *args) 
{
	float inverse[4][4];
	MatrixObject *mat = (MatrixObject *) self;
	Mat4Invert(inverse, mat->mat);
	return newMatrixObject(inverse);
}
	
struct PyMethodDef Matrix_methods[] = {
	MethodDef(inverse),
	{NULL, NULL}
};

static PyObject *Matrix_getattr(MatrixObject *self, char *name) 
{
	PyObject *list;
	float val[3];
	
	if (strcmp(name, "rot")==0) {
		float mat3[3][3];

		Mat3CpyMat4(mat3, self->mat);
		Mat3ToEul(mat3, val);

	} else if (strcmp(name, "size")==0) {
		Mat4ToSize(self->mat, val);
	/* Oh man, this is BAD. */
	} else if (strcmp(name, "loc")==0) {
		VECCOPY(val, (float *) (self->mat)[3]);

	} else {
		PyErr_SetString(PyExc_AttributeError, name);
		return NULL;
	}

	list= PyList_New(3);
	PyList_SetItem(list, 0, PyFloat_FromDouble(val[0]));
	PyList_SetItem(list, 1, PyFloat_FromDouble(val[1]));
	PyList_SetItem(list, 2, PyFloat_FromDouble(val[2]));
		
	return list;
}

static int Matrix_setattr(MatrixObject *self, char *name, PyObject *v) {
	return -1;
}

static PyObject *Matrix_repr (MatrixObject *self) {
	return BPY_tuple_repr((PyObject *) self, 4);
}

static PyObject *Matrix_item(MatrixObject *self, int i)
{
	if (i < 0 || i >= 4) {
		PyErr_SetString(PyExc_IndexError, "array index out of range");
		return NULL;
	}
	return BPY_incr_ret(self->rows[i]);
}

static PySequenceMethods Matrix_SeqMethods = {
	(inquiry) 0,				/*sq_length*/
	(binaryfunc) 0,				/*sq_concat*/
	(intargfunc) 0,				/*sq_repeat*/
	(intargfunc) Matrix_item,	/*sq_item*/
	(intintargfunc) 0,			/*sq_slice*/
	(intobjargproc) 0,			/*sq_ass_item*/
	(intintobjargproc) 0,		/*sq_ass_slice*/
};

PyTypeObject Matrix_Type = {
	PyObject_HEAD_INIT(NULL)
	0,								/*ob_size*/
	"Matrix",						/*tp_name*/
	sizeof(MatrixObject),			/*tp_basicsize*/
	0,								/*tp_itemsize*/
	/* methods */
	(destructor)	Matrix_dealloc,	/*tp_dealloc*/
	(printfunc)		0,				/*tp_print*/
	(getattrfunc)	Matrix_getattr,	/*tp_getattr*/
	(setattrfunc)	Matrix_setattr,	/*tp_setattr*/
	0,								/*tp_compare*/
	(reprfunc)		Matrix_repr,	/*tp_repr*/
	0,								/*tp_as_number*/
	&Matrix_SeqMethods,				/*tp_as_sequence*/
};

PyObject *newMatrixObject(Matrix4Ptr mat) {
	MatrixObject *self;
	
	self= PyObject_NEW(MatrixObject, &Matrix_Type);
	self->mat= mat;
		
	BPY_TRY(self->rows[0]= newVectorObject(GETROWVECTOR(self->mat, 0), 4));
	BPY_TRY(self->rows[1]= newVectorObject(GETROWVECTOR(self->mat, 1), 4));
	BPY_TRY(self->rows[2]= newVectorObject(GETROWVECTOR(self->mat, 2), 4));
	BPY_TRY(self->rows[3]= newVectorObject(GETROWVECTOR(self->mat, 3), 4));
	
	return (PyObject*) self;
}

void init_py_matrix(void) {
	Matrix_Type.ob_type = &PyType_Type;
}
