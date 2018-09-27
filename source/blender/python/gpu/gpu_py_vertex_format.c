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

/** \file blender/python/gpu/gpu_py_vertex_format.c
 *  \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_vertex_format.h" /* own include */

#ifdef __BIG_ENDIAN__
   /* big endian */
#  define MAKE_ID2(c, d)  ((c) << 8 | (d))
#  define MAKE_ID3(a, b, c) ( (int)(a) << 24 | (int)(b) << 16 | (c) << 8 )
#  define MAKE_ID4(a, b, c, d) ( (int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d) )
#else
   /* little endian  */
#  define MAKE_ID2(c, d)  ((d) << 8 | (c))
#  define MAKE_ID3(a, b, c) ( (int)(c) << 16 | (b) << 8 | (a) )
#  define MAKE_ID4(a, b, c, d) ( (int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a) )
#endif

/* -------------------------------------------------------------------- */

/** \name Enum Conversion
 *
 * Use with PyArg_ParseTuple's "O&" formatting.
 * \{ */

static int bpygpu_ParseVertCompType(PyObject *o, void *p)
{
	Py_ssize_t comp_type_id_len;
	const char *comp_type_id = _PyUnicode_AsStringAndSize(o, &comp_type_id_len);
	if (comp_type_id == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "expected a string, got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	GPUVertCompType comp_type;
	if (comp_type_id_len == 2) {
		switch (*((ushort *)comp_type_id)) {
			case MAKE_ID2('I', '8'): { comp_type = GPU_COMP_I8; goto success; }
			case MAKE_ID2('U', '8'): { comp_type = GPU_COMP_U8; goto success; }
		}
	}
	else if (comp_type_id_len == 3) {
		switch (*((uint *)comp_type_id)) {
			case MAKE_ID3('I', '1', '6'): { comp_type = GPU_COMP_I16; goto success; }
			case MAKE_ID3('U', '1', '6'): { comp_type = GPU_COMP_U16; goto success; }
			case MAKE_ID3('I', '3', '2'): { comp_type = GPU_COMP_I32; goto success; }
			case MAKE_ID3('U', '3', '2'): { comp_type = GPU_COMP_U32; goto success; }
			case MAKE_ID3('F', '3', '2'): { comp_type = GPU_COMP_F32; goto success; }
			case MAKE_ID3('I', '1', '0'): { comp_type = GPU_COMP_I10; goto success; }
		}
	}

	PyErr_Format(PyExc_ValueError,
	             "unknown type literal: '%s'",
	             comp_type_id);
	return 0;

success:
	*((GPUVertCompType *)p) = comp_type;
	return 1;
}

static int bpygpu_ParseVertFetchMode(PyObject *o, void *p)
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
			mode = GPU_FETCH_##id; \
			goto success; \
		} \
	} ((void)0)

	GPUVertFetchMode mode;
	MATCH_ID(FLOAT);
	MATCH_ID(INT);
	MATCH_ID(INT_TO_FLOAT_UNIT);
	MATCH_ID(INT_TO_FLOAT);
#undef MATCH_ID
	PyErr_Format(PyExc_ValueError,
	             "unknown type literal: '%s'",
	             mode_id);
	return 0;

success:
	(*(GPUVertFetchMode *)p) = mode;
	return 1;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name VertFormat Type
 * \{ */

static PyObject *bpygpu_VertFormat_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	if (PyTuple_GET_SIZE(args) || (kwds && PyDict_Size(kwds))) {
		PyErr_SetString(PyExc_TypeError,
		                "VertFormat(): takes no arguments");
		return NULL;
	}

	BPyGPUVertFormat *ret = (BPyGPUVertFormat *)BPyGPUVertFormat_CreatePyObject(NULL);

	return (PyObject *)ret;
}

PyDoc_STRVAR(bpygpu_VertFormat_attr_add_doc,
"TODO"
);
static PyObject *bpygpu_VertFormat_attr_add(BPyGPUVertFormat *self, PyObject *args, PyObject *kwds)
{
	struct {
		const char *id;
		GPUVertCompType comp_type;
		uint len;
		GPUVertFetchMode fetch_mode;
	} params;

	if (self->fmt.attr_len == GPU_VERT_ATTR_MAX_LEN) {
		PyErr_SetString(PyExc_ValueError, "Maxumum attr reached " STRINGIFY(GPU_VERT_ATTR_MAX_LEN));
		return NULL;
	}

	static const char *_keywords[] = {"id", "comp_type", "len", "fetch_mode", NULL};
	static _PyArg_Parser _parser = {"$sO&IO&:attr_add", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kwds, &_parser,
	        &params.id,
	        bpygpu_ParseVertCompType, &params.comp_type,
	        &params.len,
	        bpygpu_ParseVertFetchMode, &params.fetch_mode))
	{
		return NULL;
	}

	uint attr_id = GPU_vertformat_attr_add(&self->fmt, params.id, params.comp_type, params.len, params.fetch_mode);
	return PyLong_FromLong(attr_id);
}

static struct PyMethodDef bpygpu_VertFormat_methods[] = {
	{"attr_add", (PyCFunction)bpygpu_VertFormat_attr_add,
	 METH_VARARGS | METH_KEYWORDS, bpygpu_VertFormat_attr_add_doc},
	{NULL, NULL, 0, NULL}
};


static void bpygpu_VertFormat_dealloc(BPyGPUVertFormat *self)
{
	Py_TYPE(self)->tp_free(self);
}

PyTypeObject BPyGPUVertFormat_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUVertFormat",
	.tp_basicsize = sizeof(BPyGPUVertFormat),
	.tp_dealloc = (destructor)bpygpu_VertFormat_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = bpygpu_VertFormat_methods,
	.tp_new = bpygpu_VertFormat_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public API
 * \{ */

PyObject *BPyGPUVertFormat_CreatePyObject(GPUVertFormat *fmt)
{
	BPyGPUVertFormat *self;

	self = PyObject_New(BPyGPUVertFormat, &BPyGPUVertFormat_Type);
	if (fmt) {
		self->fmt = *fmt;
	}
	else {
		memset(&self->fmt, 0, sizeof(self->fmt));
	}

	return (PyObject *)self;
}

/** \} */
