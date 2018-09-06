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

/** \file blender/python/gpu/gpu_py_vertex_buffer.c
 *  \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_vertex_buffer.h"

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_vertex_format.h"
#include "gpu_py_vertex_buffer.h" /* own include */

/* -------------------------------------------------------------------- */

/** \name Utility Functions
 * \{ */

#define PY_AS_NATIVE_SWITCH(attr) \
	switch (attr->comp_type) { \
		case GPU_COMP_I8:  { PY_AS_NATIVE(int8_t,   PyC_Long_AsI8); break; } \
		case GPU_COMP_U8:  { PY_AS_NATIVE(uint8_t,  PyC_Long_AsU8); break; } \
		case GPU_COMP_I16: { PY_AS_NATIVE(int16_t,  PyC_Long_AsI16); break; } \
		case GPU_COMP_U16: { PY_AS_NATIVE(uint16_t, PyC_Long_AsU16); break; } \
		case GPU_COMP_I32: { PY_AS_NATIVE(int32_t,  PyC_Long_AsI32); break; } \
		case GPU_COMP_U32: { PY_AS_NATIVE(uint32_t, PyC_Long_AsU32); break; } \
		case GPU_COMP_F32: { PY_AS_NATIVE(float, PyFloat_AsDouble); break; } \
		default: \
			BLI_assert(0); \
	} ((void)0)

/* No error checking, callers must run PyErr_Occurred */
static void fill_format_elem(void *data_dst_void, PyObject *py_src, const GPUVertAttr *attr)
{
#define PY_AS_NATIVE(ty_dst, py_as_native) \
{ \
	ty_dst *data_dst = data_dst_void; \
	*data_dst = py_as_native(py_src); \
} ((void)0)

	PY_AS_NATIVE_SWITCH(attr);

#undef PY_AS_NATIVE
}

/* No error checking, callers must run PyErr_Occurred */
static void fill_format_tuple(void *data_dst_void, PyObject *py_src, const GPUVertAttr *attr)
{
	const uint len = attr->comp_len;

/**
 * Args are constants, so range checks will be optimized out if they're nop's.
 */
#define PY_AS_NATIVE(ty_dst, py_as_native) \
	ty_dst *data_dst = data_dst_void; \
	for (uint i = 0; i < len; i++) { \
		data_dst[i] = py_as_native(PyTuple_GET_ITEM(py_src, i)); \
	} ((void)0)

	PY_AS_NATIVE_SWITCH(attr);

#undef PY_AS_NATIVE
}

#undef PY_AS_NATIVE_SWITCH
#undef WARN_TYPE_LIMIT_PUSH
#undef WARN_TYPE_LIMIT_POP

static bool bpygpu_vertbuf_fill_impl(
        GPUVertBuf *vbo,
        uint data_id, PyObject *seq)
{
	bool ok = true;
	const GPUVertAttr *attr = &vbo->format.attribs[data_id];

	GPUVertBufRaw data_step;
	GPU_vertbuf_attr_get_raw_data(vbo, data_id, &data_step);

	PyObject *seq_fast = PySequence_Fast(seq, "Vertex buffer fill");
	if (seq_fast == NULL) {
		goto finally;
	}

	const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

	if (seq_len != vbo->vertex_len) {
		PyErr_Format(PyExc_ValueError,
		             "Expected a sequence of size %d, got %d",
		             vbo->vertex_len, seq_len);
	}

	PyObject **seq_items = PySequence_Fast_ITEMS(seq_fast);

	if (attr->comp_len == 1) {
		for (uint i = 0; i < seq_len; i++) {
			uchar *data = (uchar *)GPU_vertbuf_raw_step(&data_step);
			PyObject *item = seq_items[i];
			fill_format_elem(data, item, attr);
		}
	}
	else {
		for (uint i = 0; i < seq_len; i++) {
			uchar *data = (uchar *)GPU_vertbuf_raw_step(&data_step);
			PyObject *item = seq_items[i];
			if (!PyTuple_CheckExact(item)) {
				PyErr_Format(PyExc_ValueError,
				             "expected a tuple, got %s",
				             Py_TYPE(item)->tp_name);
				ok = false;
				goto finally;
			}
			if (PyTuple_GET_SIZE(item) != attr->comp_len) {
				PyErr_Format(PyExc_ValueError,
				             "expected a tuple of size %d, got %d",
				             attr->comp_len, PyTuple_GET_SIZE(item));
				ok = false;
				goto finally;
			}

			/* May trigger error, check below */
			fill_format_tuple(data, item, attr);
		}
	}

	if (PyErr_Occurred()) {
		ok = false;
	}

finally:

	Py_DECREF(seq_fast);
	return ok;
}

/* handy, but not used just now */
#if 0
static int bpygpu_find_id(const GPUVertFormat *fmt, const char *id)
{
	for (int i = 0; i < fmt->attr_len; i++) {
		for (uint j = 0; j < fmt->name_len; j++) {
			if (STREQ(fmt->attribs[i].name[j], id)) {
				return i;
			}
		}
	}
	return -1;
}
#endif

/** \} */


/* -------------------------------------------------------------------- */

/** \name VertBuf Type
 * \{ */

static PyObject *bpygpu_VertBuf_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	const char * const keywords[] = {"len", "format", NULL};

	struct {
		BPyGPUVertFormat *py_fmt;
		uint len;
	} params;

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds,
	        "$IO!:GPUVertBuf.__new__", (char **)keywords,
	        &params.len,
	        &BPyGPUVertFormat_Type, &params.py_fmt))
	{
		return NULL;
	}

	struct GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&params.py_fmt->fmt);

	GPU_vertbuf_data_alloc(vbo, params.len);

	return BPyGPUVertBuf_CreatePyObject(vbo);
}

PyDoc_STRVAR(bpygpu_VertBuf_fill_doc,
"TODO"
);
static PyObject *bpygpu_VertBuf_fill(BPyGPUVertBuf *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"id", "data", NULL};

	struct {
		uint id;
		PyObject *py_seq_data;
	} params;

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "$IO:fill", (char **)kwlist,
	        &params.id,
	        &params.py_seq_data))
	{
		return NULL;
	}

	if (params.id >= self->buf->format.attr_len) {
		PyErr_Format(PyExc_ValueError,
		             "Format id %d out of range",
		             params.id);
		return NULL;
	}

	if (self->buf->data == NULL) {
		PyErr_SetString(PyExc_ValueError,
		                "Can't fill, static buffer already in use");
		return NULL;
	}

	if (!bpygpu_vertbuf_fill_impl(self->buf, params.id, params.py_seq_data)) {
		return NULL;
	}
	Py_RETURN_NONE;
}

static struct PyMethodDef bpygpu_VertBuf_methods[] = {
	{"fill", (PyCFunction) bpygpu_VertBuf_fill,
	 METH_VARARGS | METH_KEYWORDS, bpygpu_VertBuf_fill_doc},
	{NULL, NULL, 0, NULL}
};

static void bpygpu_VertBuf_dealloc(BPyGPUVertBuf *self)
{
	GPU_vertbuf_discard(self->buf);
	Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(py_gpu_vertex_buffer_doc,
"GPUVertBuf(len, format)\n"
"\n"
"Contains a VBO."
"\n"
"   :param len: number of elements to allocate\n"
"   :type type: int`\n"
"   :param format: Vertex format.\n"
"   :type buf: GPUVertFormat`\n"
);
PyTypeObject BPyGPUVertBuf_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUVertBuf",
	.tp_basicsize = sizeof(BPyGPUVertBuf),
	.tp_dealloc = (destructor)bpygpu_VertBuf_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = py_gpu_vertex_buffer_doc,
	.tp_methods = bpygpu_VertBuf_methods,
	.tp_new = bpygpu_VertBuf_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public API
 * \{ */

PyObject *BPyGPUVertBuf_CreatePyObject(GPUVertBuf *buf)
{
	BPyGPUVertBuf *self;

	self = PyObject_New(BPyGPUVertBuf, &BPyGPUVertBuf_Type);
	self->buf = buf;

	return (PyObject *)self;
}

/** \} */
