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

static int bpygpu_parse_component_type(const char *str, int length)
{
	if (length == 2) {
		switch (*((ushort *)str)) {
			case MAKE_ID2('I', '8'): return GPU_COMP_I8;
			case MAKE_ID2('U', '8'): return GPU_COMP_U8;
			default: break;
		}
	}
	else if (length == 3) {
		switch (*((uint *)str)) {
			case MAKE_ID3('I', '1', '6'): return GPU_COMP_I16;
			case MAKE_ID3('U', '1', '6'): return GPU_COMP_U16;
			case MAKE_ID3('I', '3', '2'): return GPU_COMP_I32;
			case MAKE_ID3('U', '3', '2'): return GPU_COMP_U32;
			case MAKE_ID3('F', '3', '2'): return GPU_COMP_F32;
			case MAKE_ID3('I', '1', '0'): return GPU_COMP_I10;
			default: break;
		}
	}
	return -1;
}

static int bpygpu_parse_fetch_mode(const char *str, int length)
{
#define MATCH_ID(id) \
	if (length == strlen(STRINGIFY(id))) { \
		if (STREQ(str, STRINGIFY(id))) { \
			return GPU_FETCH_##id; \
		} \
	} ((void)0)

	MATCH_ID(FLOAT);
	MATCH_ID(INT);
	MATCH_ID(INT_TO_FLOAT_UNIT);
	MATCH_ID(INT_TO_FLOAT);
#undef MATCH_ID

	return -1;
}

static int bpygpu_ParseVertCompType(PyObject *o, void *p)
{
	Py_ssize_t length;
	const char *str = _PyUnicode_AsStringAndSize(o, &length);

	if (str == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "expected a string, got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	int comp_type = bpygpu_parse_component_type(str, length);
	if (comp_type == -1) {
		PyErr_Format(PyExc_ValueError,
		             "unkown component type: '%s",
		             str);
		return 0;
	}

	*((GPUVertCompType *)p) = comp_type;
	return 1;
}

static int bpygpu_ParseVertFetchMode(PyObject *o, void *p)
{
	Py_ssize_t length;
	const char *str = _PyUnicode_AsStringAndSize(o, &length);

	if (str == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "expected a string, got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	int fetch_mode = bpygpu_parse_fetch_mode(str, length);
	if (fetch_mode == -1) {
		PyErr_Format(PyExc_ValueError,
		             "unknown type literal: '%s'",
		             str);
		return 0;
	}

	(*(GPUVertFetchMode *)p) = fetch_mode;
	return 1;
}

static int get_default_fetch_mode(GPUVertCompType type)
{
	switch (type) {
		case GPU_COMP_F32: return GPU_FETCH_FLOAT;
		default: return -1;
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name VertFormat Type
 * \{ */

static bool bpygpu_vertformat_attr_add_simple(
        GPUVertFormat *format, const char *name, GPUVertCompType comp_type, int length)
{
	if (length <= 0) {
		PyErr_SetString(PyExc_ValueError,
		                "length of an attribute must greater than 0");
		return false;
	}

	int fetch_mode = get_default_fetch_mode(comp_type);
	if (fetch_mode == -1) {
		PyErr_SetString(PyExc_ValueError,
		                "no default fetch mode found");
		return false;
	}

	GPU_vertformat_attr_add(format, name, comp_type, length, fetch_mode);
	return true;
}

static bool bpygpu_vertformat_attr_add_from_tuple(
        GPUVertFormat *format, PyObject *data)
{
	const char *name;
	GPUVertCompType comp_type;
	int length;

	if (!PyArg_ParseTuple(data, "sO&i", &name, bpygpu_ParseVertCompType, &comp_type, &length)) {
		return false;
	}

	return bpygpu_vertformat_attr_add_simple(format, name, comp_type, length);
}

static PyObject *bpygpu_VertFormat_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	const char *error_prefix = "GPUVertFormat.__new__";
	PyListObject *format_list = NULL;

	static const char *_keywords[] = {"format", NULL};
	static _PyArg_Parser _parser = {"|O!:GPUVertFormat.__new__", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kwds, &_parser,
	        &PyList_Type, &format_list))
	{
		return NULL;
	}

	BPyGPUVertFormat *ret = (BPyGPUVertFormat *)BPyGPUVertFormat_CreatePyObject(NULL);

	if (format_list && !bpygpu_vertformat_from_PyList(format_list, error_prefix, &ret->fmt)) {
		Py_DECREF(ret);
		return NULL;
	}

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

bool bpygpu_vertformat_from_PyList(
        const PyListObject *list, const char *error_prefix, GPUVertFormat *r_fmt)
{
	BLI_assert(PyList_Check(list));

	Py_ssize_t amount = Py_SIZE(list);

	for (Py_ssize_t i = 0; i < amount; i++) {
		PyObject *element = PyList_GET_ITEM(list, i);
		if (!PyTuple_Check(element)) {
			PyErr_Format(PyExc_TypeError,
			             "%.200s expected a list of tuples", error_prefix);

			return false;
		}
		if (!bpygpu_vertformat_attr_add_from_tuple(r_fmt, element)) {
			return false;
		}
	}

	return true;
}

/** \} */
