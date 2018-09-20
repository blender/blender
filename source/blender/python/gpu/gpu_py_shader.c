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

/** \file blender/python/gpu/gpu_py_shader.c
 *  \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_shader.h"
#include "GPU_shader_interface.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_shader.h" /* own include */


 /* -------------------------------------------------------------------- */

 /** \name Enum Conversion.
 * \{ */

static void bpygpu_shader_add_enum_objects(PyObject *submodule)
{
	PyObject *dict = PyModule_GetDict(submodule);
	PyObject *item;

#define PY_DICT_ADD_INT(x) PyDict_SetItemString(dict, #x, item = PyLong_FromLong(x)); Py_DECREF(item)

	/* Shaders */
	PY_DICT_ADD_INT(GPU_SHADER_2D_UNIFORM_COLOR);
	PY_DICT_ADD_INT(GPU_SHADER_2D_FLAT_COLOR);
	PY_DICT_ADD_INT(GPU_SHADER_2D_SMOOTH_COLOR);
	PY_DICT_ADD_INT(GPU_SHADER_2D_IMAGE);
	PY_DICT_ADD_INT(GPU_SHADER_3D_UNIFORM_COLOR);
	PY_DICT_ADD_INT(GPU_SHADER_3D_FLAT_COLOR);
	PY_DICT_ADD_INT(GPU_SHADER_3D_SMOOTH_COLOR);

#undef PY_DICT_ADD_INT
}

static int bpygpu_pyLong_as_shader_enum(PyObject *o)
{
	uint id = (uint)PyLong_AsUnsignedLong(o);

	if (id >= GPU_NUM_BUILTIN_SHADERS) {
		PyErr_SetString(PyExc_ValueError,
		                "not a builtin shader identifier");
		return -1;
	}

	return (int)id;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Shader Type
 * \{ */

static PyObject *bpygpu_shader_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {
	        "vertexcode", "fragcode", "geocode",
	        "libcode", "defines", NULL};

	struct {
		const char *vertexcode;
		const char *fragcode;
		const char *geocode;
		const char *libcode;
		const char *defines;
	} params = {0};

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "ss|$sss:GPUShader.__new__", (char **)kwlist,
	        &params.vertexcode, &params.fragcode, &params.geocode,
	        &params.libcode, &params.defines))
	{
		return NULL;
	}

	GPUShader *shader = GPU_shader_create(
	        params.vertexcode,
	        params.fragcode,
	        params.geocode,
	        params.libcode,
	        params.defines,
	        NULL);

	if (shader == NULL) {
		PyErr_SetString(PyExc_Exception,
		                "Shader Compile Error, see console for more details");
		return NULL;
	}

	return BPyGPUShader_CreatePyObject(shader, false);
}

PyDoc_STRVAR(bpygpu_shader_bind_doc,
".. method:: bind()\n"
"\n"
"   Bind the Shader object.\n"
);
static PyObject *bpygpu_shader_bind(BPyGPUShader *self)
{
	GPU_shader_bind(self->shader);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_transform_feedback_enable_doc,
".. method:: transform_feedback_enable(vbo_id)\n"
"\n"
"   Start transform feedback operation.\n"
"\n"
"   :return: true if transform feedback was succesfully enabled.\n"
"   :rtype: `bool`\n"
);
static PyObject *bpygpu_shader_transform_feedback_enable(
        BPyGPUShader *self, PyObject *arg)
{
	uint vbo_id;
	if ((vbo_id = PyC_Long_AsU32(arg)) == (uint)-1) {
		return NULL;
	}
	return PyBool_FromLong(GPU_shader_transform_feedback_enable(self->shader, vbo_id));
}

PyDoc_STRVAR(bpygpu_shader_transform_feedback_disable_doc,
".. method:: transform_feedback_disable()\n"
"\n"
"   Disable transform feedback.\n"
);
static PyObject *bpygpu_transform_feedback_disable(BPyGPUShader *self)
{
	GPU_shader_transform_feedback_disable(self->shader);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_from_name_doc,
".. method:: uniform_from_name(name)\n"
"\n"
"   Get uniform location by name.\n"
"\n"
"   :param name: name of the uniform variable whose location is to be queried.\n"
"   :type name: `str`\n"
"   :return: the location of the uniform variable.\n"
"   :rtype: `int`\n"
);
static PyObject *bpygpu_shader_uniform_from_name(
        BPyGPUShader *self, PyObject *arg)
{
	const char *name = PyUnicode_AsUTF8(arg);
	if (name == NULL) {
		return NULL;
	}

	int uniform = GPU_shader_get_uniform(self->shader, name);

	if (uniform == -1) {
		PyErr_SetString(PyExc_SyntaxError,
		                "GPUShader.get_uniform: uniform not found.");
		return NULL;
	}

	return PyLong_FromLong(uniform);
}

PyDoc_STRVAR(bpygpu_shader_uniform_block_from_name_doc,
".. method:: uniform_block_from_name(name)\n"
"\n"
"   Get uniform block location by name.\n"
"\n"
"   :param name: name of the uniform block variable whose location is to be queried.\n"
"   :type name: `str`\n"
"   :return: the location of the uniform block variable.\n"
"   :rtype: `int`\n"
);
static PyObject *bpygpu_shader_uniform_block_from_name(
        BPyGPUShader *self, PyObject *arg)
{
	const char *name = PyUnicode_AsUTF8(arg);
	if (name == NULL) {
		return NULL;
	}

	int uniform = GPU_shader_get_uniform_block(self->shader, name);

	if (uniform == -1) {
		PyErr_SetString(PyExc_SyntaxError,
		                "GPUShader.get_uniform_block: uniform not found");
		return NULL;
	}

	return PyLong_FromLong(uniform);
}

static bool bpygpu_shader_uniform_vector_imp(
        PyObject *args, int elem_size,
        int *r_location, int *r_length, int *r_count, Py_buffer *r_pybuffer)
{
	PyObject *buffer;

	*r_count = 1;
	if (!PyArg_ParseTuple(
	        args, "iOi|i:GPUShader.uniform_vector_*",
	        r_location, &buffer, r_length, r_count))
	{
		return false;
	}

	if (PyObject_GetBuffer(buffer, r_pybuffer, PyBUF_SIMPLE) == -1) {
		/* PyObject_GetBuffer raise a PyExc_BufferError */
		return false;
	}

	if (r_pybuffer->len != (*r_length * *r_count * elem_size)) {
		PyErr_SetString(
		        PyExc_BufferError,
		        "GPUShader.uniform_vector_*: buffer size does not match.");
		return false;
	}

	return true;
}

PyDoc_STRVAR(bpygpu_shader_uniform_vector_float_doc,
".. method:: uniform_vector_float(location, buffer, length, count)\n"
"\n"
"   Set the buffer to fill the uniform.\n"
"\n"
"   :param location: location of the uniform variable to be modified.\n"
"   :type location: `int`\n"
"   :param buffer: buffer object with format float.\n"
"   :type buffer: `buffer object`\n"
"   :param length:\n"
"      size of the uniform data type:\n"
"\n"
"      - 1: float\n"
"      - 2: vec2 or float[2]\n"
"      - 3: vec3 or float[3]\n"
"      - 4: vec4 or float[4]\n"
"      - 9: mat3\n"
"      - 16: mat4\n"
"\n"
"   :type length:    `int`\n"
"   :param count:    specifies the number of elements, vector or matrices that are to be modified.\n"
"   :type count:     `int`\n"
);
static PyObject *bpygpu_shader_uniform_vector_float(
        BPyGPUShader *self, PyObject *args)
{
	int location, length, count;

	Py_buffer pybuffer;

	if (!bpygpu_shader_uniform_vector_imp(
	        args, sizeof(float),
	        &location, &length, &count, &pybuffer))
	{
		return NULL;
	}

	GPU_shader_uniform_vector(
	        self->shader, location, length,
	        count, pybuffer.buf);

	PyBuffer_Release(&pybuffer);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_vector_int_doc,
".. method:: uniform_vector_int(location, buffer, length, count)\n"
"\n"
"   See GPUShader.uniform_vector_float(...) description.\n"
);
static PyObject *bpygpu_shader_uniform_vector_int(
        BPyGPUShader *self, PyObject *args)
{
	int location, length, count;

	Py_buffer pybuffer;

	if (!bpygpu_shader_uniform_vector_imp(
	        args, sizeof(int),
	        &location, &length, &count, &pybuffer))
	{
		return NULL;
	}

	GPU_shader_uniform_vector_int(
	        self->shader, location, length,
	        count, pybuffer.buf);

	PyBuffer_Release(&pybuffer);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_float_doc,
	".. method:: uniform_float(location, value)\n"
	"\n"
	"   Set uniform value.\n"
	"\n"
	"   :param location: builtin identifier.\n"
	"   :type location: `int`\n"
	"   :param value: uniform value.\n"
	"   :type value: `float`\n"
);
static PyObject *bpygpu_shader_uniform_float(
	BPyGPUShader *self, PyObject *args)
{
	int location;
	float value;

	if (!PyArg_ParseTuple(
	            args, "if:GPUShader.uniform_float",
	            &location, &value))
	{
		return NULL;
	}

	GPU_shader_uniform_float(self->shader, location, value);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_uniform_int_doc,
".. method:: uniform_int(location, value)\n"
"\n"
"   Set uniform value.\n"
"\n"
"   :param location: builtin identifier.\n"
"   :type location: `int`\n"
"   :param value: uniform value.\n"
"   :type value: `int`\n"
);
static PyObject *bpygpu_shader_uniform_int(
        BPyGPUShader *self, PyObject *args)
{
	int location, value;

	if (!PyArg_ParseTuple(
	        args, "ii:GPUShader.uniform_int",
	        &location, &value))
	{
		return NULL;
	}

	GPU_shader_uniform_int(self->shader, location, value);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_attr_from_name_doc,
".. method:: attr_from_name(name)\n"
"\n"
"   Get attribute location by name.\n"
"\n"
"   :param name: the name of the attribute variable whose location is to be queried.\n"
"   :type name: `str`\n"
"   :return: the location of an attribute variable.\n"
"   :rtype: `int`\n"
);
static PyObject *bpygpu_shader_attr_from_name(
        BPyGPUShader *self, PyObject *arg)
{
	const char *name = PyUnicode_AsUTF8(arg);
	if (name == NULL) {
		return NULL;
	}

	int attrib = GPU_shader_get_attribute(self->shader, name);

	if (attrib == -1) {
		PyErr_SetString(PyExc_SyntaxError,
		                "GPUShader.attr_from_name: attribute not found.");
		return NULL;
	}

	return PyLong_FromLong(attrib);
}

PyDoc_STRVAR(bpygpu_shader_program_doc,
"The name of the program object for use by the OpenGL API (read-only).\n\n:type: int"
);
static PyObject *bpygpu_shader_program_get(BPyGPUShader *self, void *UNUSED(closure))
{
	return PyLong_FromLong(GPU_shader_get_program(self->shader));
}

static struct PyMethodDef bpygpu_shader_methods[] = {
	{"bind", (PyCFunction)bpygpu_shader_bind,
	 METH_NOARGS, bpygpu_shader_bind_doc},
	{"transform_feedback_enable",
	 (PyCFunction)bpygpu_shader_transform_feedback_enable,
	 METH_O, bpygpu_shader_transform_feedback_enable_doc},
	{"transform_feedback_disable",
	 (PyCFunction)bpygpu_transform_feedback_disable,
	 METH_NOARGS, bpygpu_shader_transform_feedback_disable_doc},
	{"uniform_from_name",
	 (PyCFunction)bpygpu_shader_uniform_from_name,
	 METH_O, bpygpu_shader_uniform_from_name_doc},
	{"uniform_block_from_name",
	 (PyCFunction)bpygpu_shader_uniform_block_from_name,
	 METH_O, bpygpu_shader_uniform_block_from_name_doc},
	{"uniform_vector_float",
	 (PyCFunction)bpygpu_shader_uniform_vector_float,
	 METH_VARARGS, bpygpu_shader_uniform_vector_float_doc},
	{"uniform_vector_int",
	 (PyCFunction)bpygpu_shader_uniform_vector_int,
	 METH_VARARGS, bpygpu_shader_uniform_vector_int_doc},
	{"uniform_float",
	 (PyCFunction)bpygpu_shader_uniform_float,
	 METH_VARARGS, bpygpu_shader_uniform_float_doc},
	{"uniform_int",
	 (PyCFunction)bpygpu_shader_uniform_int,
	 METH_VARARGS, bpygpu_shader_uniform_int_doc},
	{"attr_from_name",
	 (PyCFunction)bpygpu_shader_attr_from_name,
	 METH_O, bpygpu_shader_attr_from_name_doc},
	{NULL, NULL, 0, NULL}
};

static PyGetSetDef bpygpu_shader_getseters[] = {
	{"program",
	 (getter)bpygpu_shader_program_get, (setter)NULL,
	 bpygpu_shader_program_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};


static void bpygpu_shader_dealloc(BPyGPUShader *self)
{
	if (self->is_builtin == false) {
		GPU_shader_free(self->shader);
	}
	Py_TYPE(self)->tp_free((PyObject *)self);
}


PyDoc_STRVAR(bpygpu_shader_doc,
"GPUShader(vertexcode, fragcode, geocode=None, libcode=None, defines=None)\n"
"\n"
"GPUShader combines multiple GLSL shaders into a program used for drawing.\n"
"It must contain a vertex and fragment shaders, with an optional geometry shader.\n"
"\n"
"The GLSL #version directive is automatically included at the top of shaders, and set to 330.\n"
"\n"
"Some preprocessor directives are automatically added according to the Operating System or availability.\n"
"\n"
"These are::\n"
"\n"
"   \"#define GPU_ATI\\n\"\n"
"   \"#define GPU_NVIDIA\\n\"\n"
"   \"#define GPU_INTEL\\n\"\n"
"\n"
"The following extensions are enabled by default if supported by the GPU::\n"
"\n"
"   \"#extension GL_ARB_texture_gather: enable\\n\"\n"
"   \"#extension GL_ARB_texture_query_lod: enable\\n\"\n"
"\n"
"To debug shaders, use the --debug-gpu-shaders command line option"
" to see full GLSL shader compilation and linking errors.\n"
"\n"
"   :param vertexcode: vertex Shader Code.\n"
"   :type vertexcode: `str`\n"
"   :param fragcode: fragment Shader Code.\n"
"   :type value: `str`\n"
"   :param geocode: geometry Shader Code.\n"
"   :type value: `str`\n"
"   :param libcode: code with functions and presets to be shared between shaders.\n"
"   :type value: `str`\n"
"   :param defines: preprocessor directives.\n"
"   :type value: `str`\n"
);
PyTypeObject BPyGPUShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUShader",
	.tp_basicsize = sizeof(BPyGPUShader),
	.tp_dealloc = (destructor)bpygpu_shader_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = bpygpu_shader_doc,
	.tp_methods = bpygpu_shader_methods,
	.tp_getset = bpygpu_shader_getseters,
	.tp_new = bpygpu_shader_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name gpu.shader Module API
 * \{ */

PyDoc_STRVAR(bpygpu_shader_unbind_doc,
".. function:: unbind()\n"
"\n"
"   Unbind the bound shader object.\n"
);
static PyObject *bpygpu_shader_unbind(BPyGPUShader *UNUSED(self))
{
	GPU_shader_unbind();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_shader_from_builtin_doc,
".. function:: shader_from_builtin(shader_id)\n"
"\n"
"   :param shader_id: shader identifier.\n"
"   :type shader_id: `int`\n"
);
static PyObject *bpygpu_shader_from_builtin(PyObject *UNUSED(self), PyObject *arg)
{
	int shader_id = bpygpu_pyLong_as_shader_enum(arg);
	if (shader_id == -1) {
		return NULL;
	}

	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);

	return BPyGPUShader_CreatePyObject(shader, true);
}

PyDoc_STRVAR(bpygpu_shader_code_from_builtin_doc,
".. function:: shader_code_from_builtin(shader_id)\n"
"\n"
"   :param shader_id: shader identifier.\n"
"   :type shader_id: `int`\n"
"   :return: vertex, fragment and geometry shader codes.\n"
"   :rtype: `dict`\n"
);
static PyObject *bpygpu_shader_code_from_builtin(BPyGPUShader *UNUSED(self), PyObject *arg)
{
	const char *vert;
	const char *frag;
	const char *geom;
	const char *defines;

	PyObject *item, *r_dict;

	int shader_id = bpygpu_pyLong_as_shader_enum(arg);
	if (shader_id == -1) {
		return NULL;
	}

	GPU_shader_get_builtin_shader_code(
	        shader_id, &vert, &frag, &geom, &defines);

	r_dict = PyDict_New();

	PyDict_SetItemString(r_dict, "vertex_shader", item = PyUnicode_FromString(vert));
	Py_DECREF(item);

	PyDict_SetItemString(r_dict, "fragment_shader", item = PyUnicode_FromString(frag));
	Py_DECREF(item);

	if (geom) {
		PyDict_SetItemString(r_dict, "geometry_shader", item = PyUnicode_FromString(geom));
		Py_DECREF(item);
	}
	if (defines) {
		PyDict_SetItemString(r_dict, "defines", item = PyUnicode_FromString(defines));
		Py_DECREF(item);
	}
	return r_dict;
}

static struct PyMethodDef bpygpu_shader_module_methods[] = {
	{"unbind",
	 (PyCFunction)bpygpu_shader_unbind,
	 METH_NOARGS, bpygpu_shader_unbind_doc},
	{"shader_from_builtin",
	 (PyCFunction)bpygpu_shader_from_builtin,
	 METH_O, bpygpu_shader_from_builtin_doc},
	{"shader_code_from_builtin",
	 (PyCFunction)bpygpu_shader_code_from_builtin,
	 METH_O, bpygpu_shader_code_from_builtin_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(bpygpu_shader_module_doc,
"This module provides access to GPUShader internal functions."
);
static PyModuleDef BPyGPU_shader_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "gpu.shader",
	.m_doc = bpygpu_shader_module_doc,
	.m_methods = bpygpu_shader_module_methods,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name gpu.shader.buitin Module API
 * \{ */

PyDoc_STRVAR(bpygpu_shader_builtin_module_doc,
"This module contains integers that identify the built-in shader ids."
);
static PyModuleDef BPyGPU_shader_builtin_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "gpu.shader.builtin",
	.m_doc = bpygpu_shader_builtin_module_doc,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public API
 * \{ */

PyObject *BPyGPUShader_CreatePyObject(GPUShader *shader, bool is_builtin)
{
	BPyGPUShader *self;

	self = PyObject_New(BPyGPUShader, &BPyGPUShader_Type);
	self->shader = shader;
	self->is_builtin = is_builtin;

	return (PyObject *)self;
}

PyObject *BPyInit_gpu_shader(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPyGPU_shader_module_def);

	return submodule;
}

PyObject *BPyInit_gpu_shader_builtin(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPyGPU_shader_builtin_module_def);
	bpygpu_shader_add_enum_objects(submodule);

	return submodule;
}

/** \} */
