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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/mathutils.c
 *  \ingroup pygen
 */

#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

PyDoc_STRVAR(M_Mathutils_doc,
"This module provides access to matrices, eulers, quaternions and vectors."
);
static int mathutils_array_parse_fast(float *array, int array_min, int array_max, PyObject *value, const char *error_prefix)
{
	PyObject *value_fast= NULL;
	PyObject *item;

	int i, size;

	/* non list/tuple cases */
	if(!(value_fast=PySequence_Fast(value, error_prefix))) {
		/* PySequence_Fast sets the error */
		return -1;
	}

	size= PySequence_Fast_GET_SIZE(value_fast);

	if(size > array_max || size < array_min) {
		if (array_max == array_min)	{
			PyErr_Format(PyExc_ValueError,
			             "%.200s: sequence size is %d, expected %d",
			             error_prefix, size, array_max);
		}
		else {
			PyErr_Format(PyExc_ValueError,
			             "%.200s: sequence size is %d, expected [%d - %d]",
			             error_prefix, size, array_min, array_max);
		}
		Py_DECREF(value_fast);
		return -1;
	}

	i= size;
	do {
		i--;
		if(((array[i]= PyFloat_AsDouble((item= PySequence_Fast_GET_ITEM(value_fast, i)))) == -1.0f) && PyErr_Occurred()) {
			PyErr_Format(PyExc_TypeError,
			             "%.200s: sequence index %d expected a number, "
			             "found '%.200s' type, ",
			             error_prefix, i, Py_TYPE(item)->tp_name);
			Py_DECREF(value_fast);
			return -1;
		}
	} while(i);

	Py_XDECREF(value_fast);
	return size;
}

/* helper functionm returns length of the 'value', -1 on error */
int mathutils_array_parse(float *array, int array_min, int array_max, PyObject *value, const char *error_prefix)
{
#if 1 /* approx 6x speedup for mathutils types */
	int size;

	if(	(VectorObject_Check(value) && (size= ((VectorObject *)value)->size)) ||
		(EulerObject_Check(value) && (size= 3)) ||
		(QuaternionObject_Check(value) && (size= 4)) ||
		(ColorObject_Check(value) && (size= 3))
	) {
		if(BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}

		if(size > array_max || size < array_min) {
			if (array_max == array_min)	{
				PyErr_Format(PyExc_ValueError,
				             "%.200s: sequence size is %d, expected %d",
				             error_prefix, size, array_max);
			}
			else {
				PyErr_Format(PyExc_ValueError,
				             "%.200s: sequence size is %d, expected [%d - %d]",
				             error_prefix, size, array_min, array_max);
			}
			return -1;
		}

		memcpy(array, ((BaseMathObject *)value)->data, size * sizeof(float));
		return size;
	}
	else
#endif
	{
		return mathutils_array_parse_fast(array, array_min, array_max, value, error_prefix);
	}
}

int mathutils_any_to_rotmat(float rmat[3][3], PyObject *value, const char *error_prefix)
{
	if(EulerObject_Check(value)) {
		if(BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}
		else {
			eulO_to_mat3(rmat, ((EulerObject *)value)->eul, ((EulerObject *)value)->order);
			return 0;
		}
	}
	else if (QuaternionObject_Check(value)) {
		if(BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}
		else {
			float tquat[4];
			normalize_qt_qt(tquat, ((QuaternionObject *)value)->quat);
			quat_to_mat3(rmat, tquat);
			return 0;
		}
	}
	else if (MatrixObject_Check(value)) {
		if(BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}
		else if(((MatrixObject *)value)->col_size < 3 || ((MatrixObject *)value)->row_size < 3) {
			PyErr_Format(PyExc_ValueError,
			             "%.200s: matrix must have minimum 3x3 dimensions",
			             error_prefix);
			return -1;
		}
		else {
			matrix_as_3x3(rmat, (MatrixObject *)value);
			normalize_m3(rmat);
			return 0;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "%.200s: expected a Euler, Quaternion or Matrix type, "
		             "found %.200s", error_prefix, Py_TYPE(value)->tp_name);
		return -1;
	}
}


//----------------------------------MATRIX FUNCTIONS--------------------


/* Utility functions */

// LomontRRDCompare4, Ever Faster Float Comparisons by Randy Dillon
#define SIGNMASK(i) (-(int)(((unsigned int)(i))>>31))

int EXPP_FloatsAreEqual(float af, float bf, int maxDiff)
{	// solid, fast routine across all platforms
	// with constant time behavior
	int ai = *(int *)(&af);
	int bi = *(int *)(&bf);
	int test = SIGNMASK(ai^bi);
	int diff, v1, v2;

	assert((0 == test) || (0xFFFFFFFF == test));
	diff = (ai ^ (test & 0x7fffffff)) - bi;
	v1 = maxDiff + diff;
	v2 = maxDiff - diff;
	return (v1|v2) >= 0;
}

/*---------------------- EXPP_VectorsAreEqual -------------------------
  Builds on EXPP_FloatsAreEqual to test vectors */
int EXPP_VectorsAreEqual(float *vecA, float *vecB, int size, int floatSteps)
{
	int x;
	for (x=0; x< size; x++){
		if (EXPP_FloatsAreEqual(vecA[x], vecB[x], floatSteps) == 0)
			return 0;
	}
	return 1;
}


/* Mathutils Callbacks */

/* for mathutils internal use only, eventually should re-alloc but to start with we only have a few users */
static Mathutils_Callback *mathutils_callbacks[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

int Mathutils_RegisterCallback(Mathutils_Callback *cb)
{
	int i;
	
	/* find the first free slot */
	for(i= 0; mathutils_callbacks[i]; i++) {
		if(mathutils_callbacks[i]==cb) /* already registered? */
			return i;
	}
	
	mathutils_callbacks[i] = cb;
	return i;
}

/* use macros to check for NULL */
int _BaseMathObject_ReadCallback(BaseMathObject *self)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->get(self, self->cb_subtype) != -1)
		return 0;

	if(!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s read, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

int _BaseMathObject_WriteCallback(BaseMathObject *self)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->set(self, self->cb_subtype) != -1)
		return 0;

	if(!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s write, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

int _BaseMathObject_ReadIndexCallback(BaseMathObject *self, int index)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->get_index(self, self->cb_subtype, index) != -1)
		return 0;

	if(!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s read index, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

int _BaseMathObject_WriteIndexCallback(BaseMathObject *self, int index)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->set_index(self, self->cb_subtype, index) != -1)
		return 0;

	if(!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s write index, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

/* BaseMathObject generic functions for all mathutils types */
char BaseMathObject_Owner_doc[] = "The item this is wrapping or None  (readonly).";
PyObject *BaseMathObject_getOwner(BaseMathObject *self, void *UNUSED(closure))
{
	PyObject *ret= self->cb_user ? self->cb_user : Py_None;
	Py_INCREF(ret);
	return ret;
}

char BaseMathObject_Wrapped_doc[] = "True when this object wraps external data (readonly).\n\n:type: boolean";
PyObject *BaseMathObject_getWrapped(BaseMathObject *self, void *UNUSED(closure))
{
	return PyBool_FromLong((self->wrapped == Py_WRAP) ? 1:0);
}

int BaseMathObject_traverse(BaseMathObject *self, visitproc visit, void *arg)
{
	Py_VISIT(self->cb_user);
	return 0;
}

int BaseMathObject_clear(BaseMathObject *self)
{
	Py_CLEAR(self->cb_user);
	return 0;
}

void BaseMathObject_dealloc(BaseMathObject *self)
{
	/* only free non wrapped */
	if(self->wrapped != Py_WRAP) {
		PyMem_Free(self->data);
	}

	if(self->cb_user) {
		PyObject_GC_UnTrack(self);
		BaseMathObject_clear(self);
	}

	Py_TYPE(self)->tp_free(self); // PyObject_DEL(self); // breaks subtypes
}

/*----------------------------MODULE INIT-------------------------*/
static struct PyMethodDef M_Mathutils_methods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef M_Mathutils_module_def = {
	PyModuleDef_HEAD_INIT,
	"mathutils",  /* m_name */
	M_Mathutils_doc,  /* m_doc */
	0,  /* m_size */
	M_Mathutils_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyMODINIT_FUNC PyInit_mathutils(void)
{
	PyObject *submodule;
	PyObject *item;

	if(PyType_Ready(&vector_Type) < 0)
		return NULL;
	if(PyType_Ready(&matrix_Type) < 0)
		return NULL;	
	if(PyType_Ready(&euler_Type) < 0)
		return NULL;
	if(PyType_Ready(&quaternion_Type) < 0)
		return NULL;
	if(PyType_Ready(&color_Type) < 0)
		return NULL;

	submodule = PyModule_Create(&M_Mathutils_module_def);
	
	/* each type has its own new() function */
	PyModule_AddObject(submodule, "Vector",		(PyObject *)&vector_Type);
	PyModule_AddObject(submodule, "Matrix",		(PyObject *)&matrix_Type);
	PyModule_AddObject(submodule, "Euler",		(PyObject *)&euler_Type);
	PyModule_AddObject(submodule, "Quaternion",	(PyObject *)&quaternion_Type);
	PyModule_AddObject(submodule, "Color",		(PyObject *)&color_Type);
	
	/* submodule */
	PyModule_AddObject(submodule, "geometry",		(item=PyInit_mathutils_geometry()));
	/* XXX, python doesnt do imports with this usefully yet
	 * 'from mathutils.geometry import PolyFill'
	 * ...fails without this. */
	PyDict_SetItemString(PyThreadState_GET()->interp->modules, "mathutils.geometry", item);
	Py_INCREF(item);

	mathutils_matrix_vector_cb_index= Mathutils_RegisterCallback(&mathutils_matrix_vector_cb);

	return submodule;
}
