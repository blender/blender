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
 * Copyright 2015, Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/gpu_py_matrix.c
 *  \ingroup pythonintern
 *
 * This file defines the gpu.matrix stack API.
 */

#include <Python.h>


#include "BLI_utildefines.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu.h"

#include "GPU_matrix.h"

/* -------------------------------------------------------------------- */

/** \name Manage Stack
 * \{ */

PyDoc_STRVAR(pygpu_matrix_push_doc,
"push()\n"
"\n"
"   Add to the matrix stack.\n"
);
static PyObject *pygpu_matrix_push(PyObject *UNUSED(self))
{
	gpuPushMatrix();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_pop_doc,
"pop()\n"
"\n"
"   Remove the last matrix from the stack.\n"
);
static PyObject *pygpu_matrix_pop(PyObject *UNUSED(self))
{
	gpuPopMatrix();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_push_projection_doc,
"push_projection()\n"
"\n"
"   Add to the projection matrix stack.\n"
);
static PyObject *pygpu_matrix_push_projection(PyObject *UNUSED(self))
{
	gpuPushMatrix();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_pop_projection_doc,
"pop_projection()\n"
"\n"
"   Remove the last projection matrix from the stack.\n"
);
static PyObject *pygpu_matrix_pop_projection(PyObject *UNUSED(self))
{
	gpuPopMatrix();
	Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Manipulate State
 * \{ */

PyDoc_STRVAR(pygpu_matrix_multiply_matrix_doc,
"multiply_matrix(matrix)\n"
"\n"
"   Multiply the current stack matrix.\n"
"\n"
"   :param matrix: A 4x4 matrix.\n"
"   :type matrix: :class:`mathutils.Matrix`\n"
);
static PyObject *pygpu_matrix_multiply_matrix(PyObject *UNUSED(self), PyObject *value)
{
	MatrixObject *pymat;
	if (!Matrix_Parse4x4(value, &pymat)) {
		return NULL;
	}
	gpuMultMatrix(pymat->matrix);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_scale_doc,
"scale(scale)\n"
"\n"
"   Scale the current stack matrix.\n"
"\n"
"   :param scale: Scale the current stack matrix.\n"
"   :type scale: sequence of 2 or 3 floats\n"
);
static PyObject *pygpu_matrix_scale(PyObject *UNUSED(self), PyObject *value)
{
	float scale[3];
	int len;
	if ((len = mathutils_array_parse(scale, 2, 3, value, "gpu.matrix.scale(): invalid vector arg")) == -1) {
		return NULL;
	}
	if (len == 2) {
		gpuScale2fv(scale);
	}
	else {
		gpuScale3fv(scale);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_scale_uniform_doc,
"scale_uniform(scale)\n"
"\n"
"   :param scale: Scale the current stack matrix.\n"
"   :type scale: sequence of 2 or 3 floats\n"
);
static PyObject *pygpu_matrix_scale_uniform(PyObject *UNUSED(self), PyObject *value)
{
	float scalar;
	if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) {
		PyErr_Format(PyExc_TypeError,
		             "expected a number, not %.200s",
		             Py_TYPE(value)->tp_name);
		return NULL;
	}
	gpuScaleUniform(scalar);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_translate_doc,
"translate(offset)\n"
"\n"
"   Scale the current stack matrix.\n"
"\n"
"   :param offset: Translate the current stack matrix.\n"
"   :type offset: sequence of 2 or 3 floats\n"
);
static PyObject *pygpu_matrix_translate(PyObject *UNUSED(self), PyObject *value)
{
	float offset[3];
	int len;
	if ((len = mathutils_array_parse(offset, 2, 3, value, "gpu.matrix.translate(): invalid vector arg")) == -1) {
		return NULL;
	}
	if (len == 2) {
		gpuTranslate2fv(offset);
	}
	else {
		gpuTranslate3fv(offset);
	}
	Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Write State
 * \{ */

PyDoc_STRVAR(pygpu_matrix_reset_doc,
"reset()\n"
"\n"
"   Empty stack and set to identity.\n"
);
static PyObject *pygpu_matrix_reset(PyObject *UNUSED(self))
{
	gpuMatrixReset();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_load_identity_doc,
"load_identity()\n"
"\n"
"   Empty stack and set to identity.\n"
);
static PyObject *pygpu_matrix_load_identity(PyObject *UNUSED(self))
{
	gpuLoadIdentity();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_load_matrix_doc,
"load_matrix(matrix)\n"
"\n"
"   Load a matrix into the stack.\n"
"\n"
"   :param matrix: A 4x4 matrix.\n"
"   :type matrix: :class:`mathutils.Matrix`\n"
);
static PyObject *pygpu_matrix_load_matrix(PyObject *UNUSED(self), PyObject *value)
{
	MatrixObject *pymat;
	if (!Matrix_Parse4x4(value, &pymat)) {
		return NULL;
	}
	gpuLoadMatrix(pymat->matrix);
	Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Read State
 * \{ */

PyDoc_STRVAR(pygpu_matrix_get_projection_matrix_doc,
"get_projection_matrix()\n"
"\n"
"   Return a copy of the projection matrix.\n"
"\n"
"   :return: A 4x4 projection matrix.\n"
"   :rtype: :class:`mathutils.Matrix`\n"
);
static PyObject *pygpu_matrix_get_projection_matrix(PyObject *UNUSED(self))
{
	float matrix[4][4];
	gpuGetModelViewMatrix(matrix);
	return Matrix_CreatePyObject(&matrix[0][0], 4, 4, NULL);
}


PyDoc_STRVAR(pygpu_matrix_get_modal_view_matrix_doc,
"get_view_matrix()\n"
"\n"
"   Return a copy of the view matrix.\n"
"\n"
"   :return: A 4x4 view matrix.\n"
"   :rtype: :class:`mathutils.Matrix`\n"
);
static PyObject *pygpu_matrix_get_modal_view_matrix(PyObject *UNUSED(self))
{
	float matrix[4][4];
	gpuGetProjectionMatrix(matrix);
	return Matrix_CreatePyObject(&matrix[0][0], 4, 4, NULL);
}

PyDoc_STRVAR(pygpu_matrix_get_normal_matrix_doc,
"get_normal_matrix()\n"
"\n"
"   Return a copy of the normal matrix.\n"
"\n"
"   :return: A 3x3 normal matrix.\n"
"   :rtype: :class:`mathutils.Matrix`\n"
);
static PyObject *pygpu_matrix_get_normal_matrix(PyObject *UNUSED(self))
{
	float matrix[3][3];
	gpuGetNormalMatrix(matrix);
	return Matrix_CreatePyObject(&matrix[0][0], 3, 3, NULL);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Module
 * \{ */


static struct PyMethodDef BPy_GPU_matrix_methods[] = {
	/* Manage Stack */
	{"push", (PyCFunction)pygpu_matrix_push, METH_NOARGS, pygpu_matrix_push_doc},
	{"pop", (PyCFunction)pygpu_matrix_pop, METH_NOARGS, pygpu_matrix_pop_doc},

	{"push_projection", (PyCFunction)pygpu_matrix_push_projection,
	 METH_NOARGS, pygpu_matrix_push_projection_doc},
	{"pop_projection", (PyCFunction)pygpu_matrix_pop_projection,
	 METH_NOARGS, pygpu_matrix_pop_projection_doc},

	/* Manipulate State */
	{"multiply_matrix", (PyCFunction)pygpu_matrix_multiply_matrix,
	 METH_O, pygpu_matrix_multiply_matrix_doc},
	{"scale", (PyCFunction)pygpu_matrix_scale,
	 METH_O, pygpu_matrix_scale_doc},
	{"scale_uniform", (PyCFunction)pygpu_matrix_scale_uniform,
	 METH_O, pygpu_matrix_scale_uniform_doc},
	{"translate", (PyCFunction)pygpu_matrix_translate,
	 METH_O, pygpu_matrix_translate_doc},

	/* TODO */
#if 0
	{"rotate", (PyCFunction)pygpu_matrix_rotate,
	 METH_O, pygpu_matrix_rotate_doc},
	{"rotate_axis", (PyCFunction)pygpu_matrix_rotate_axis,
	 METH_O, pygpu_matrix_rotate_axis_doc},
	{"look_at", (PyCFunction)pygpu_matrix_look_at,
	 METH_O, pygpu_matrix_look_at_doc},
#endif

	/* Write State */
	{"reset", (PyCFunction)pygpu_matrix_reset,
	 METH_NOARGS, pygpu_matrix_reset_doc},
	{"load_identity", (PyCFunction)pygpu_matrix_load_identity,
	 METH_NOARGS, pygpu_matrix_load_identity_doc},
	{"load_matrix", (PyCFunction)pygpu_matrix_load_matrix,
	 METH_O, pygpu_matrix_load_matrix_doc},

	/* Read State */
	{"get_projection_matrix", (PyCFunction)pygpu_matrix_get_projection_matrix,
	 METH_NOARGS, pygpu_matrix_get_projection_matrix_doc},
	{"get_model_view_matrix", (PyCFunction)pygpu_matrix_get_modal_view_matrix,
	 METH_NOARGS, pygpu_matrix_get_modal_view_matrix_doc},
	{"get_normal_matrix", (PyCFunction)pygpu_matrix_get_normal_matrix,
	 METH_NOARGS, pygpu_matrix_get_normal_matrix_doc},

	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_GPU_matrix_doc,
"This module provides access to the matrix stack."
);
static PyModuleDef BPy_GPU_matrix_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "gpu.matrix",
	.m_doc = BPy_GPU_matrix_doc,
	.m_methods = BPy_GPU_matrix_methods,
};

PyObject *BPyInit_gpu_matrix(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_GPU_matrix_module_def);

	return submodule;
}

/** \} */
