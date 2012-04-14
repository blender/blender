/* 
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

/** \file blender/python/mathutils/mathutils.c
 *  \ingroup pymathutils
 */

#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"

PyDoc_STRVAR(M_Mathutils_doc,
"This module provides access to matrices, eulers, quaternions and vectors."
);
static int mathutils_array_parse_fast(float *array,
                                      int size,
                                      PyObject *value_fast,
                                      const char *error_prefix)
{
	PyObject *item;

	int i;

	i = size;
	do {
		i--;
		if (((array[i] = PyFloat_AsDouble((item = PySequence_Fast_GET_ITEM(value_fast, i)))) == -1.0f) &&
		    PyErr_Occurred())
		{
			PyErr_Format(PyExc_TypeError,
			             "%.200s: sequence index %d expected a number, "
			             "found '%.200s' type, ",
			             error_prefix, i, Py_TYPE(item)->tp_name);
			Py_DECREF(value_fast);
			return -1;
		}
	} while (i);

	Py_XDECREF(value_fast);
	return size;
}

/* helper functionm returns length of the 'value', -1 on error */
int mathutils_array_parse(float *array, int array_min, int array_max, PyObject *value, const char *error_prefix)
{
	int size;

#if 1 /* approx 6x speedup for mathutils types */

	if ((size = VectorObject_Check(value)     ? ((VectorObject *)value)->size : 0) ||
	    (size = EulerObject_Check(value)      ? 3 : 0) ||
	    (size = QuaternionObject_Check(value) ? 4 : 0) ||
	    (size = ColorObject_Check(value)      ? 3 : 0))
	{
		if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}

		if (size > array_max || size < array_min) {
			if (array_max == array_min) {
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
		PyObject *value_fast = NULL;

		/* non list/tuple cases */
		if (!(value_fast = PySequence_Fast(value, error_prefix))) {
			/* PySequence_Fast sets the error */
			return -1;
		}

		size = PySequence_Fast_GET_SIZE(value_fast);

		if (size > array_max || size < array_min) {
			if (array_max == array_min) {
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

		return mathutils_array_parse_fast(array, size, value_fast, error_prefix);
	}
}

int mathutils_array_parse_alloc(float **array, int array_min, PyObject *value, const char *error_prefix)
{
	int size;

#if 1 /* approx 6x speedup for mathutils types */

	if ((size = VectorObject_Check(value)     ? ((VectorObject *)value)->size : 0) ||
	    (size = EulerObject_Check(value)      ? 3 : 0) ||
	    (size = QuaternionObject_Check(value) ? 4 : 0) ||
	    (size = ColorObject_Check(value)      ? 3 : 0))
	{
		if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}

		if (size < array_min) {
			PyErr_Format(PyExc_ValueError,
			             "%.200s: sequence size is %d, expected > %d",
			             error_prefix, size, array_min);
			return -1;
		}
		
		*array = PyMem_Malloc(size * sizeof(float));
		memcpy(*array, ((BaseMathObject *)value)->data, size * sizeof(float));
		return size;
	}
	else
#endif
	{
		PyObject *value_fast = NULL;
		// *array = NULL;

		/* non list/tuple cases */
		if (!(value_fast = PySequence_Fast(value, error_prefix))) {
			/* PySequence_Fast sets the error */
			return -1;
		}

		size = PySequence_Fast_GET_SIZE(value_fast);

		if (size < array_min) {
			PyErr_Format(PyExc_ValueError,
			             "%.200s: sequence size is %d, expected > %d",
			             error_prefix, size, array_min);
			return -1;
		}

		*array = PyMem_Malloc(size * sizeof(float));

		return mathutils_array_parse_fast(*array, size, value_fast, error_prefix);
	}
}

int mathutils_any_to_rotmat(float rmat[3][3], PyObject *value, const char *error_prefix)
{
	if (EulerObject_Check(value)) {
		if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}
		else {
			eulO_to_mat3(rmat, ((EulerObject *)value)->eul, ((EulerObject *)value)->order);
			return 0;
		}
	}
	else if (QuaternionObject_Check(value)) {
		if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
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
		if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
			return -1;
		}
		else if (((MatrixObject *)value)->num_row < 3 || ((MatrixObject *)value)->num_col < 3) {
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
#define SIGNMASK(i) (-(int)(((unsigned int)(i)) >> 31))

int EXPP_FloatsAreEqual(float af, float bf, int maxDiff)
{
	/* solid, fast routine across all platforms
	 * with constant time behavior */
	int ai = *(int *)(&af);
	int bi = *(int *)(&bf);
	int test = SIGNMASK(ai ^ bi);
	int diff, v1, v2;

	assert((0 == test) || (0xFFFFFFFF == test));
	diff = (ai ^ (test & 0x7fffffff)) - bi;
	v1 = maxDiff + diff;
	v2 = maxDiff - diff;
	return (v1 | v2) >= 0;
}

/*---------------------- EXPP_VectorsAreEqual -------------------------
 * Builds on EXPP_FloatsAreEqual to test vectors */
int EXPP_VectorsAreEqual(float *vecA, float *vecB, int size, int floatSteps)
{
	int x;
	for (x = 0; x < size; x++) {
		if (EXPP_FloatsAreEqual(vecA[x], vecB[x], floatSteps) == 0)
			return 0;
	}
	return 1;
}

/* dynstr as python string utility funcions, frees 'ds'! */
PyObject *mathutils_dynstr_to_py(struct DynStr *ds)
{
	const int ds_len = BLI_dynstr_get_len(ds); /* space for \0 */
	char *ds_buf     = PyMem_Malloc(ds_len + 1);
	PyObject *ret;
	BLI_dynstr_get_cstring_ex(ds, ds_buf);
	BLI_dynstr_free(ds);
	ret = PyUnicode_FromStringAndSize(ds_buf, ds_len);
	PyMem_Free(ds_buf);
	return ret;
}

/* silly function, we dont use arg. just check its compatible with __deepcopy__ */
int mathutils_deepcopy_args_check(PyObject *args)
{
	PyObject *dummy_pydict;
	return PyArg_ParseTuple(args, "|O!:__deepcopy__", &PyDict_Type, &dummy_pydict) != 0;
}

/* Mathutils Callbacks */

/* for mathutils internal use only, eventually should re-alloc but to start with we only have a few users */
#define MATHUTILS_TOT_CB 10
static Mathutils_Callback *mathutils_callbacks[MATHUTILS_TOT_CB] = {NULL};

unsigned char Mathutils_RegisterCallback(Mathutils_Callback *cb)
{
	unsigned char i;
	
	/* find the first free slot */
	for (i = 0; mathutils_callbacks[i]; i++) {
		if (mathutils_callbacks[i] == cb) /* already registered? */
			return i;
	}

	BLI_assert(i + 1 < MATHUTILS_TOT_CB);

	mathutils_callbacks[i] = cb;
	return i;
}

/* use macros to check for NULL */
int _BaseMathObject_ReadCallback(BaseMathObject *self)
{
	Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
	if (LIKELY(cb->get(self, self->cb_subtype) != -1)) {
		return 0;
	}

	if (!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s read, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

int _BaseMathObject_WriteCallback(BaseMathObject *self)
{
	Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
	if (LIKELY(cb->set(self, self->cb_subtype) != -1)) {
		return 0;
	}

	if (!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s write, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

int _BaseMathObject_ReadIndexCallback(BaseMathObject *self, int index)
{
	Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
	if (LIKELY(cb->get_index(self, self->cb_subtype, index) != -1)) {
		return 0;
	}

	if (!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s read index, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

int _BaseMathObject_WriteIndexCallback(BaseMathObject *self, int index)
{
	Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
	if (LIKELY(cb->set_index(self, self->cb_subtype, index) != -1)) {
		return 0;
	}

	if (!PyErr_Occurred()) {
		PyErr_Format(PyExc_RuntimeError,
		             "%s write index, user has become invalid",
		             Py_TYPE(self)->tp_name);
	}
	return -1;
}

/* BaseMathObject generic functions for all mathutils types */
char BaseMathObject_owner_doc[] = "The item this is wrapping or None  (read-only).";
PyObject *BaseMathObject_owner_get(BaseMathObject *self, void *UNUSED(closure))
{
	PyObject *ret = self->cb_user ? self->cb_user : Py_None;
	Py_INCREF(ret);
	return ret;
}

char BaseMathObject_is_wrapped_doc[] = "True when this object wraps external data (read-only).\n\n:type: boolean";
PyObject *BaseMathObject_is_wrapped_get(BaseMathObject *self, void *UNUSED(closure))
{
	return PyBool_FromLong((self->wrapped == Py_WRAP) ? 1 : 0);
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
	if (self->wrapped != Py_WRAP) {
		PyMem_Free(self->data);
	}

	if (self->cb_user) {
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
	PyObject *sys_modules = PyThreadState_GET()->interp->modules;

	if (PyType_Ready(&vector_Type) < 0)
		return NULL;
	if (PyType_Ready(&matrix_Type) < 0)
		return NULL;
	if (PyType_Ready(&matrix_access_Type) < 0)
		return NULL;
	if (PyType_Ready(&euler_Type) < 0)
		return NULL;
	if (PyType_Ready(&quaternion_Type) < 0)
		return NULL;
	if (PyType_Ready(&color_Type) < 0)
		return NULL;

	submodule = PyModule_Create(&M_Mathutils_module_def);
	
	/* each type has its own new() function */
	PyModule_AddObject(submodule, "Vector",     (PyObject *)&vector_Type);
	PyModule_AddObject(submodule, "Matrix",     (PyObject *)&matrix_Type);
	PyModule_AddObject(submodule, "Euler",      (PyObject *)&euler_Type);
	PyModule_AddObject(submodule, "Quaternion", (PyObject *)&quaternion_Type);
	PyModule_AddObject(submodule, "Color",      (PyObject *)&color_Type);
	
	/* submodule */
	PyModule_AddObject(submodule, "geometry",       (item = PyInit_mathutils_geometry()));
	/* XXX, python doesnt do imports with this usefully yet
	 * 'from mathutils.geometry import PolyFill'
	 * ...fails without this. */
	PyDict_SetItemString(sys_modules, "mathutils.geometry", item);
	Py_INCREF(item);

	/* Noise submodule */
	PyModule_AddObject(submodule, "noise", (item = PyInit_mathutils_noise()));
	PyDict_SetItemString(sys_modules, "mathutils.noise", item);
	Py_INCREF(item);

	mathutils_matrix_row_cb_index = Mathutils_RegisterCallback(&mathutils_matrix_row_cb);
	mathutils_matrix_col_cb_index = Mathutils_RegisterCallback(&mathutils_matrix_col_cb);
	mathutils_matrix_translation_cb_index = Mathutils_RegisterCallback(&mathutils_matrix_translation_cb);

	return submodule;
}
