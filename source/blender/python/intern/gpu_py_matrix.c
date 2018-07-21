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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/gpu_py_matrix.c
 *  \ingroup pythonintern
 *
 * This file defines the gpu.matrix stack API.
 *
 * \warning While these functions attempt to ensure correct stack usage.
 * Mixing Python and C functions may still crash on invalid use.
 */

#include <Python.h>


#include "BLI_utildefines.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu.h"

#define USE_GPU_PY_MATRIX_API
#include "GPU_matrix.h"
#undef USE_GPU_PY_MATRIX_API

/* -------------------------------------------------------------------- */
/** \name Helper Functions
 * \{ */

static bool pygpu_stack_is_push_model_view_ok_or_error(void)
{
	if (GPU_matrix_stack_level_get_model_view() >= GPU_PY_MATRIX_STACK_LEN) {
		PyErr_SetString(PyExc_RuntimeError,
		                "Maximum model-view stack depth " STRINGIFY(GPU_PY_MATRIX_STACK_DEPTH) " reached");
		return false;
	}
	return true;
}

static bool pygpu_stack_is_push_projection_ok_or_error(void)
{
	if (GPU_matrix_stack_level_get_projection() >= GPU_PY_MATRIX_STACK_LEN) {
		PyErr_SetString(PyExc_RuntimeError,
		                "Maximum projection stack depth " STRINGIFY(GPU_PY_MATRIX_STACK_DEPTH) " reached");
		return false;
	}
	return true;
}

static bool pygpu_stack_is_pop_model_view_ok_or_error(void)
{
	if (GPU_matrix_stack_level_get_model_view() == 0) {
		PyErr_SetString(PyExc_RuntimeError,
		                "Minimum model-view stack depth reached");
		return false;
	}
	return true;
}

static bool pygpu_stack_is_pop_projection_ok_or_error(void)
{
	if (GPU_matrix_stack_level_get_projection() == 0) {
		PyErr_SetString(PyExc_RuntimeError,
		                "Minimum projection stack depth reached");
		return false;
	}
	return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manage Stack
 * \{ */

PyDoc_STRVAR(pygpu_matrix_push_doc,
"push()\n"
"\n"
"   Add to the model-view matrix stack.\n"
);
static PyObject *pygpu_matrix_push(PyObject *UNUSED(self))
{
	if (!pygpu_stack_is_push_model_view_ok_or_error()) {
		return NULL;
	}
	GPU_matrix_push();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_pop_doc,
"pop()\n"
"\n"
"   Remove the last model-view matrix from the stack.\n"
);
static PyObject *pygpu_matrix_pop(PyObject *UNUSED(self))
{
	if (!pygpu_stack_is_pop_model_view_ok_or_error()) {
		return NULL;
	}
	GPU_matrix_pop();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_push_projection_doc,
"push_projection()\n"
"\n"
"   Add to the projection matrix stack.\n"
);
static PyObject *pygpu_matrix_push_projection(PyObject *UNUSED(self))
{
	if (!pygpu_stack_is_push_projection_ok_or_error()) {
		return NULL;
	}
	GPU_matrix_push_projection();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_pop_projection_doc,
"pop_projection()\n"
"\n"
"   Remove the last projection matrix from the stack.\n"
);
static PyObject *pygpu_matrix_pop_projection(PyObject *UNUSED(self))
{
	if (!pygpu_stack_is_pop_projection_ok_or_error()) {
		return NULL;
	}
	GPU_matrix_pop_projection();
	Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stack (Context Manager)
 *
 * Safer alternative to ensure balanced push/pop calls.
 *
 * \{ */

typedef struct {
	PyObject_HEAD /* required python macro */
	int type;
	int level;
} BPy_GPU_MatrixStackContext;

enum {
	PYGPU_MATRIX_TYPE_MODEL_VIEW = 1,
	PYGPU_MATRIX_TYPE_PROJECTION = 2,
};

static PyObject *pygpu_matrix_stack_context_enter(BPy_GPU_MatrixStackContext *self);
static PyObject *pygpu_matrix_stack_context_exit(BPy_GPU_MatrixStackContext *self, PyObject *args);

static PyMethodDef pygpu_matrix_stack_context_methods[] = {
	{"__enter__", (PyCFunction)pygpu_matrix_stack_context_enter, METH_NOARGS},
	{"__exit__",  (PyCFunction)pygpu_matrix_stack_context_exit,  METH_VARARGS},
	{NULL}
};

static PyTypeObject pygpu_matrix_stack_context_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUMatrixStackContext",
	.tp_basicsize = sizeof(BPy_GPU_MatrixStackContext),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = pygpu_matrix_stack_context_methods,
};

static PyObject *pygpu_matrix_stack_context_enter(BPy_GPU_MatrixStackContext *self)
{
	/* sanity - should never happen */
	if (self->level != -1) {
		PyErr_SetString(PyExc_RuntimeError, "Already in use");
		return NULL;
	}

	if (self->type == PYGPU_MATRIX_TYPE_MODEL_VIEW) {
		if (!pygpu_stack_is_push_model_view_ok_or_error()) {
			return NULL;
		}
		GPU_matrix_push();
		self->level = GPU_matrix_stack_level_get_model_view();
	}
	else if (self->type == PYGPU_MATRIX_TYPE_PROJECTION) {
		if (!pygpu_stack_is_push_projection_ok_or_error()) {
			return NULL;
		}
		GPU_matrix_push_projection();
		self->level = GPU_matrix_stack_level_get_projection();
	}
	else {
		BLI_assert(0);
	}
	Py_RETURN_NONE;
}

static PyObject *pygpu_matrix_stack_context_exit(BPy_GPU_MatrixStackContext *self, PyObject *UNUSED(args))
{
	/* sanity - should never happen */
	if (self->level == -1) {
		fprintf(stderr, "Not yet in use\n");
		goto finally;
	}

	if (self->type == PYGPU_MATRIX_TYPE_MODEL_VIEW) {
		const int level = GPU_matrix_stack_level_get_model_view();
		if (level != self->level) {
			fprintf(stderr, "Level push/pop mismatch, expected %d, got %d\n", self->level, level);
		}
		if (level != 0) {
			GPU_matrix_pop();
		}
	}
	else if (self->type == PYGPU_MATRIX_TYPE_PROJECTION) {
		const int level = GPU_matrix_stack_level_get_projection();
		if (level != self->level) {
			fprintf(stderr, "Level push/pop mismatch, expected %d, got %d", self->level, level);
		}
		if (level != 0) {
			GPU_matrix_pop_projection();
		}
	}
	else {
		BLI_assert(0);
	}
finally:
	Py_RETURN_NONE;
}

static PyObject *pygpu_matrix_push_pop_impl(int type)
{
	BPy_GPU_MatrixStackContext *ret = PyObject_New(BPy_GPU_MatrixStackContext, &pygpu_matrix_stack_context_Type);
	ret->type = type;
	ret->level = -1;
	return (PyObject *)ret;
}

PyDoc_STRVAR(pygpu_matrix_push_pop_doc,
"push_pop()\n"
"\n"
"   Context manager to ensure balanced push/pop calls, even in the case of an error.\n"
);
static PyObject *pygpu_matrix_push_pop(PyObject *UNUSED(self))
{
	return pygpu_matrix_push_pop_impl(PYGPU_MATRIX_TYPE_MODEL_VIEW);
}

PyDoc_STRVAR(pygpu_matrix_push_pop_projection_doc,
"push_pop_projection()\n"
"\n"
"   Context manager to ensure balanced push/pop calls, even in the case of an error.\n"
);
static PyObject *pygpu_matrix_push_pop_projection(PyObject *UNUSED(self))
{
	return pygpu_matrix_push_pop_impl(PYGPU_MATRIX_TYPE_PROJECTION);
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
	GPU_matrix_mul(pymat->matrix);
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
		GPU_matrix_scale_2fv(scale);
	}
	else {
		GPU_matrix_scale_3fv(scale);
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
	GPU_matrix_scale_1f(scalar);
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
		GPU_matrix_translate_2fv(offset);
	}
	else {
		GPU_matrix_translate_3fv(offset);
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
	GPU_matrix_reset();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_matrix_load_identity_doc,
"load_identity()\n"
"\n"
"   Empty stack and set to identity.\n"
);
static PyObject *pygpu_matrix_load_identity(PyObject *UNUSED(self))
{
	GPU_matrix_identity_set();
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
	GPU_matrix_set(pymat->matrix);
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
	GPU_matrix_model_view_get(matrix);
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
	GPU_matrix_projection_get(matrix);
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
	GPU_matrix_normal_get(matrix);
	return Matrix_CreatePyObject(&matrix[0][0], 3, 3, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static struct PyMethodDef BPy_GPU_matrix_methods[] = {
	/* Manage Stack */
	{"push", (PyCFunction)pygpu_matrix_push,
	 METH_NOARGS, pygpu_matrix_push_doc},
	{"pop", (PyCFunction)pygpu_matrix_pop,
	 METH_NOARGS, pygpu_matrix_pop_doc},

	{"push_projection", (PyCFunction)pygpu_matrix_push_projection,
	 METH_NOARGS, pygpu_matrix_push_projection_doc},
	{"pop_projection", (PyCFunction)pygpu_matrix_pop_projection,
	 METH_NOARGS, pygpu_matrix_pop_projection_doc},

	/* Stack (Context Manager) */
	{"push_pop", (PyCFunction)pygpu_matrix_push_pop,
	 METH_NOARGS, pygpu_matrix_push_pop_doc},
	{"push_pop_projection", (PyCFunction)pygpu_matrix_push_pop_projection,
	 METH_NOARGS, pygpu_matrix_push_pop_projection_doc},

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

	if (PyType_Ready(&pygpu_matrix_stack_context_Type) < 0) {
		return NULL;
	}

	return submodule;
}

/** \} */
