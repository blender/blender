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

/** \file blender/python/gpu/gpu_py_batch.c
 *  \ingroup bpygpu
 *
 * This file defines the offscreen functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_library.h"

#include "GPU_batch.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py_vertex_buffer.h"
#include "gpu_py_batch.h" /* own include */


/* -------------------------------------------------------------------- */

/** \name VertBatch Type
 * \{ */

static int bpygpu_ParsePrimType(PyObject *o, void *p)
{
	Py_ssize_t mode_id_len;
	const char *mode_id = _PyUnicode_AsStringAndSize(o, &mode_id_len);
	if (mode_id == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "expected a string, got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}
#define MATCH_ID(id) \
	if (mode_id_len == strlen(STRINGIFY(id))) { \
		if (STREQ(mode_id, STRINGIFY(id))) { \
			mode = GPU_PRIM_##id; \
			goto success; \
		} \
	} ((void)0)

	GPUPrimType mode;
	MATCH_ID(POINTS);
	MATCH_ID(LINES);
	MATCH_ID(TRIS);
	MATCH_ID(LINE_STRIP);
	MATCH_ID(LINE_LOOP);
	MATCH_ID(TRI_STRIP);
	MATCH_ID(TRI_FAN);
	MATCH_ID(LINE_STRIP_ADJ);

#undef MATCH_ID
	PyErr_Format(PyExc_ValueError,
	             "unknown type literal: '%s'",
	             mode_id);
	return 0;

success:
	(*(GPUPrimType *)p) = mode;
	return 1;
}

static PyObject *bpygpu_Batch_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	const char * const keywords[] = {"type", "buf", NULL};

	struct {
		GPUPrimType type_id;
		BPyGPUVertBuf *py_buf;
	} params;

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds,
	        "$O&O!:GPUBatch.__new__", (char **)keywords,
	        bpygpu_ParsePrimType, &params.type_id,
	        &BPyGPUVertBuf_Type, &params.py_buf))
	{
		return NULL;
	}

	GPUBatch *batch = GPU_batch_create(params.type_id, params.py_buf->buf, NULL);
	BPyGPUBatch *ret = (BPyGPUBatch *)BPyGPUBatch_CreatePyObject(batch);

#ifdef USE_GPU_PY_REFERENCES
	ret->references = PyList_New(1);
	PyList_SET_ITEM(ret->references, 0, (PyObject *)params.py_buf);
	Py_INCREF(params.py_buf);
	PyObject_GC_Track(ret);
#endif

	return (PyObject *)ret;
}

PyDoc_STRVAR(bpygpu_VertBatch_vertbuf_add_doc,
"TODO"
);
static PyObject *bpygpu_VertBatch_vertbuf_add(BPyGPUBatch *self, BPyGPUVertBuf *py_buf)
{
	if (!BPyGPUVertBuf_Check(py_buf)) {
		PyErr_Format(PyExc_TypeError,
		             "Expected a GPUVertBuf, got %s",
		             Py_TYPE(py_buf)->tp_name);
		return NULL;
	}

	if (self->batch->verts[0]->vertex_len != py_buf->buf->vertex_len) {
		PyErr_Format(PyExc_TypeError,
		             "Expected %d length, got %d",
		             self->batch->verts[0]->vertex_len, py_buf->buf->vertex_len);
		return NULL;
	}

#ifdef USE_GPU_PY_REFERENCES
	/* Hold user */
	PyList_Append(self->references, (PyObject *)py_buf);
#endif

	GPU_batch_vertbuf_add(self->batch, py_buf->buf);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_VertBatch_program_set_builtin_doc,
"TODO"
);
static PyObject *bpygpu_VertBatch_program_set_builtin(BPyGPUBatch *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"id", NULL};

	struct {
		const char *shader;
	} params;

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "s:program_set_builtin", (char **)kwlist,
	        &params.shader))
	{
		return NULL;
	}

	GPUBuiltinShader shader;

#define MATCH_ID(id) \
	if (STREQ(params.shader, STRINGIFY(id))) { \
		shader = GPU_SHADER_##id; \
		goto success; \
	} ((void)0)

	MATCH_ID(2D_FLAT_COLOR);
	MATCH_ID(2D_SMOOTH_COLOR);
	MATCH_ID(2D_UNIFORM_COLOR);

	MATCH_ID(3D_FLAT_COLOR);
	MATCH_ID(3D_SMOOTH_COLOR);
	MATCH_ID(3D_UNIFORM_COLOR);

#undef MATCH_ID

	PyErr_SetString(PyExc_ValueError,
	                "shader name not known");
	return NULL;

success:
	GPU_batch_program_set_builtin(self->batch, shader);
	Py_RETURN_NONE;
}

static PyObject *bpygpu_VertBatch_uniform_bool(BPyGPUBatch *self, PyObject *args)
{
	struct {
		const char *id;
		bool values[1];
	} params;

	if (!PyArg_ParseTuple(
	        args, "sO&:uniform_bool",
	        &params.id,
	        PyC_ParseBool, &params.values[0]))
	{
		return NULL;
	}

	GPU_batch_uniform_1b(self->batch, params.id, params.values[0]);
	Py_RETURN_NONE;
}

static PyObject *bpygpu_VertBatch_uniform_i32(BPyGPUBatch *self, PyObject *args)
{
	struct {
		const char *id;
		int values[1];
	} params;

	if (!PyArg_ParseTuple(
	        args, "si:uniform_i32",
	        &params.id,
	        &params.values[0]))
	{
		return NULL;
	}

	GPU_batch_uniform_1i(self->batch, params.id, params.values[0]);
	Py_RETURN_NONE;
}

static PyObject *bpygpu_VertBatch_uniform_f32(BPyGPUBatch *self, PyObject *args)
{
	struct {
		const char *id;
		float values[4];
	} params;

	if (!PyArg_ParseTuple(
	        args, "sf|fff:uniform_f32",
	        &params.id,
	        &params.values[0], &params.values[1], &params.values[2], &params.values[3]))
	{
		return NULL;
	}

	switch (PyTuple_GET_SIZE(args)) {
		case 2: GPU_batch_uniform_1f(self->batch, params.id, params.values[0]); break;
		case 3: GPU_batch_uniform_2f(self->batch, params.id, UNPACK2(params.values)); break;
		case 4: GPU_batch_uniform_3f(self->batch, params.id, UNPACK3(params.values)); break;
		case 5: GPU_batch_uniform_4f(self->batch, params.id, UNPACK4(params.values)); break;
		default:
			BLI_assert(0);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_VertBatch_draw_doc,
"TODO"
);
static PyObject *bpygpu_VertBatch_draw(BPyGPUBatch *self)
{
	if (!glIsProgram(self->batch->program)) {
		PyErr_SetString(PyExc_ValueError,
		                "batch program has not not set");
	}
	GPU_batch_draw(self->batch);
	Py_RETURN_NONE;
}

static PyObject *bpygpu_VertBatch_program_use_begin(BPyGPUBatch *self)
{
	if (!glIsProgram(self->batch->program)) {
		PyErr_SetString(PyExc_ValueError,
		                "batch program has not not set");
	}
	GPU_batch_program_use_begin(self->batch);
	Py_RETURN_NONE;
}

static PyObject *bpygpu_VertBatch_program_use_end(BPyGPUBatch *self)
{
	if (!glIsProgram(self->batch->program)) {
		PyErr_SetString(PyExc_ValueError,
		                "batch program has not not set");
	}
	GPU_batch_program_use_end(self->batch);
	Py_RETURN_NONE;
}

static struct PyMethodDef bpygpu_VertBatch_methods[] = {
	{"vertbuf_add", (PyCFunction)bpygpu_VertBatch_vertbuf_add,
	 METH_O, bpygpu_VertBatch_vertbuf_add_doc},
	{"program_set_builtin", (PyCFunction)bpygpu_VertBatch_program_set_builtin,
	 METH_VARARGS | METH_KEYWORDS, bpygpu_VertBatch_program_set_builtin_doc},
	{"uniform_bool", (PyCFunction)bpygpu_VertBatch_uniform_bool,
	 METH_VARARGS, NULL},
	{"uniform_i32", (PyCFunction)bpygpu_VertBatch_uniform_i32,
	 METH_VARARGS, NULL},
	{"uniform_f32", (PyCFunction)bpygpu_VertBatch_uniform_f32,
	  METH_VARARGS, NULL},
	{"draw", (PyCFunction) bpygpu_VertBatch_draw,
	 METH_NOARGS, bpygpu_VertBatch_draw_doc},
	{"program_use_begin", (PyCFunction)bpygpu_VertBatch_program_use_begin,
	 METH_NOARGS, ""},
	{"program_use_end", (PyCFunction)bpygpu_VertBatch_program_use_end,
	 METH_NOARGS, ""},
	{NULL, NULL, 0, NULL}
};

#ifdef USE_GPU_PY_REFERENCES

static int bpygpu_Batch_traverse(BPyGPUBatch *self, visitproc visit, void *arg)
{
	Py_VISIT(self->references);
	return 0;
}

static int bpygpu_Batch_clear(BPyGPUBatch *self)
{
	Py_CLEAR(self->references);
	return 0;
}

#endif

static void bpygpu_Batch_dealloc(BPyGPUBatch *self)
{
	GPU_batch_discard(self->batch);

#ifdef USE_GPU_PY_REFERENCES
	if (self->references) {
		PyObject_GC_UnTrack(self);
		bpygpu_Batch_clear(self);
		Py_XDECREF(self->references);
	}
#endif

	Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(py_gpu_batch_doc,
"GPUBatch(type, buf)\n"
"\n"
"Contains VAOs + VBOs + Shader representing a drawable entity."
"\n"
"   :param type: One of these primitive types: {\n"
"       \"POINTS\",\n"
"       \"LINES\",\n"
"       \"TRIS\",\n"
"       \"LINE_STRIP\",\n"
"       \"LINE_LOOP\",\n"
"       \"TRI_STRIP\",\n"
"       \"TRI_FAN\",\n"
"       \"LINES_ADJ\",\n"
"       \"TRIS_ADJ\",\n"
"       \"LINE_STRIP_ADJ\",\n"
"       \"NONE\"}\n"
"   :type type: `str`\n"
"   :param buf: Vertex buffer.\n"
"   :type buf: :class: `gpu.types.GPUVertBuf`\n"
);
PyTypeObject BPyGPUBatch_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUBatch",
	.tp_basicsize = sizeof(BPyGPUBatch),
	.tp_dealloc = (destructor)bpygpu_Batch_dealloc,
#ifdef USE_GPU_PY_REFERENCES
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
	.tp_doc = py_gpu_batch_doc,
	.tp_traverse = (traverseproc)bpygpu_Batch_traverse,
	.tp_clear = (inquiry)bpygpu_Batch_clear,
#else
	.tp_flags = Py_TPFLAGS_DEFAULT,
#endif
	.tp_methods = bpygpu_VertBatch_methods,
	.tp_new = bpygpu_Batch_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public API
* \{ */

PyObject *BPyGPUBatch_CreatePyObject(GPUBatch *batch)
{
	BPyGPUBatch *self;

#ifdef USE_GPU_PY_REFERENCES
	self = (BPyGPUBatch *)_PyObject_GC_New(&BPyGPUBatch_Type);
	self->references = NULL;
#else
	self = PyObject_New(BPyGPUBatch, &BPyGPUBatch_Type);
#endif

	self->batch = batch;

	return (PyObject *)self;
}

/** \} */

#undef BPY_GPU_BATCH_CHECK_OBJ
