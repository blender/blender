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
	const char *exc_str_size_mismatch = "Expected a %s of size %d, got %d";

	bool ok = true;
	const GPUVertAttr *attr = &vbo->format.attribs[data_id];

	if (PyObject_CheckBuffer(seq)) {
		Py_buffer pybuffer;

		if (PyObject_GetBuffer(seq, &pybuffer, PyBUF_STRIDES | PyBUF_ND) == -1) {
			/* PyObject_GetBuffer raise a PyExc_BufferError */
			return false;
		}

		int comp_len = pybuffer.ndim == 1 ? 1 : pybuffer.shape[1];

		if (pybuffer.shape[0] != vbo->vertex_len) {
			PyErr_Format(PyExc_ValueError, exc_str_size_mismatch,
			             "sequence", vbo->vertex_len, pybuffer.shape[0]);
			ok = false;
		}
		else if (comp_len != attr->comp_len) {
			PyErr_Format(PyExc_ValueError, exc_str_size_mismatch,
			            "component", attr->comp_len, comp_len);
			ok = false;
		}
		else {
			GPU_vertbuf_attr_fill_stride(vbo, data_id, pybuffer.strides[0], pybuffer.buf);
		}

		PyBuffer_Release(&pybuffer);
	}
	else {
		GPUVertBufRaw data_step;
		GPU_vertbuf_attr_get_raw_data(vbo, data_id, &data_step);

		PyObject *seq_fast = PySequence_Fast(seq, "Vertex buffer fill");
		if (seq_fast == NULL) {
			return false;
		}

		const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

		if (seq_len != vbo->vertex_len) {
			PyErr_Format(PyExc_ValueError, exc_str_size_mismatch,
			             "sequence", vbo->vertex_len, seq_len);
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
					PyErr_Format(PyExc_ValueError, exc_str_size_mismatch,
					             "tuple", attr->comp_len, PyTuple_GET_SIZE(item));
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
	}
	return ok;
}

static int bpygpu_fill_attribute(GPUVertBuf *buf, int id, PyObject *py_seq_data)
{
	if (id < 0 || id >= buf->format.attr_len) {
		PyErr_Format(PyExc_ValueError,
		             "Format id %d out of range",
		             id);
		return 0;
	}

	if (buf->data == NULL) {
		PyErr_SetString(PyExc_ValueError,
		                "Can't fill, static buffer already in use");
		return 0;
	}

	if (!bpygpu_vertbuf_fill_impl(buf, (uint)id, py_seq_data)) {
		return 0;
	}

	return 1;
}


/** \} */


/* -------------------------------------------------------------------- */

/** \name VertBuf Type
 * \{ */

static PyObject *bpygpu_VertBuf_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	struct {
		BPyGPUVertFormat *py_fmt;
		uint len;
	} params;

	static const char *_keywords[] = {"format", "len", NULL};
	static _PyArg_Parser _parser = {"O!I:GPUVertBuf.__new__", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kwds, &_parser,
	        &BPyGPUVertFormat_Type, &params.py_fmt,
	        &params.len))
	{
		return NULL;
	}

	struct GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&params.py_fmt->fmt);

	GPU_vertbuf_data_alloc(vbo, params.len);

	return BPyGPUVertBuf_CreatePyObject(vbo);
}

PyDoc_STRVAR(bpygpu_VertBuf_fill_attribute_doc,
"fill_attribute(identifier, data)\n"
"\n"
"   Insert data into the buffer for a single attribute.\n"
"\n"
"   :param identifier: Either the name or the id of the attribute.\n"
"   :type identifier: int or str\n"
"   :param data: Sequence of data that should be stored in the buffer\n"
"   :type data: sequence of individual values or tuples\n"
);
static PyObject *bpygpu_VertBuf_fill_attribute(BPyGPUVertBuf *self, PyObject *args, PyObject *kwds)
{
	PyObject *data;
	PyObject *identifier;

	static const char *_keywords[] = {"identifier", "data", NULL};
	static _PyArg_Parser _parser = {"OO:fill_attribute", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kwds, &_parser,
	        &identifier, &data))
	{
		return NULL;
	}

	int id;

	if (PyLong_Check(identifier)) {
		id = PyLong_AsLong(identifier);
	}
	else if (PyUnicode_Check(identifier)) {
		const char *name = PyUnicode_AsUTF8(identifier);
		id = GPU_vertformat_attr_id_get(&self->buf->format, name);
		if (id == -1) {
			PyErr_SetString(PyExc_ValueError,
			                "Unknown attribute name");
			return NULL;
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError,
		                "expected int or str type as identifier");
		return NULL;
	}


	if (!bpygpu_fill_attribute(self->buf, id, data)) {
		return NULL;
	}

	Py_RETURN_NONE;
}


static struct PyMethodDef bpygpu_VertBuf_methods[] = {
	{"fill_attribute", (PyCFunction) bpygpu_VertBuf_fill_attribute,
	 METH_VARARGS | METH_KEYWORDS, bpygpu_VertBuf_fill_attribute_doc},
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
"   :type type: `int`\n"
"   :param format: Vertex format.\n"
"   :type buf: `gpu.types.GPUVertFormat`\n"
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
