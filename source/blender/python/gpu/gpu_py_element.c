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

/** \file blender/python/gpu/gpu_py_element.c
 *  \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_element.h"

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_primitive.h"
#include "gpu_py_element.h" /* own include */


/* -------------------------------------------------------------------- */

/** \name IndexBuf Type
 * \{ */

static PyObject *bpygpu_IndexBuf_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	bool ok = true;

	struct {
		GPUPrimType type_id;
		PyObject *seq;
	} params;

	uint verts_per_prim;
	uint index_len;
	GPUIndexBufBuilder builder;

	static const char *_keywords[] = {"type", "seq", NULL};
	static _PyArg_Parser _parser = {"$O&O:IndexBuf.__new__", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kwds, &_parser,
	        bpygpu_ParsePrimType, &params.type_id,
	        &params.seq))
	{
		return NULL;
	}

	verts_per_prim = GPU_indexbuf_primitive_len(params.type_id);
	if (verts_per_prim == -1) {
		PyErr_Format(PyExc_ValueError,
		             "The argument 'type' must be "
		             "'POINTS', 'LINES', 'TRIS' or 'LINES_ADJ'");
		return NULL;
	}

	if (PyObject_CheckBuffer(params.seq)) {
		Py_buffer pybuffer;

		if (PyObject_GetBuffer(params.seq, &pybuffer, PyBUF_FORMAT | PyBUF_ND) == -1) {
			/* PyObject_GetBuffer already handles error messages. */
			return NULL;
		}

		if (pybuffer.ndim != 1 && pybuffer.shape[1] != verts_per_prim) {
			PyErr_Format(PyExc_ValueError,
			             "Each primitive must exactly %d indices",
			             verts_per_prim);
			return NULL;
		}

		if (pybuffer.itemsize != 4 ||
		    PyC_Formatstr_is_float(PyC_Formatstr_get(pybuffer.format)))
		{
			PyErr_Format(PyExc_ValueError,
			             "Each index must be an 4-bytes integer value");
			return NULL;
		}

		index_len = pybuffer.shape[0];
		if (pybuffer.ndim != 1) {
			index_len *= pybuffer.shape[1];
		}

		/* The `vertex_len` parameter is only used for asserts in the Debug build. */
		/* Not very useful in python since scripts are often tested in Release build. */
		/* Use `INT_MAX` instead of the actual number of vertices. */
		GPU_indexbuf_init(
		        &builder, params.type_id, index_len, INT_MAX);

#if 0
		uint *buf = pybuffer.buf;
		for (uint i = index_len; i--; buf++) {
			GPU_indexbuf_add_generic_vert(&builder, *buf);
		}
#else
		memcpy(builder.data, pybuffer.buf, index_len * sizeof(builder.data));
		builder.index_len = index_len;
#endif
		PyBuffer_Release(&pybuffer);
	}
	else {
		PyObject *seq_fast = PySequence_Fast(
		        params.seq, "Index Buffer Initialization");

		if (seq_fast == NULL) {
			return false;
		}

		const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

		PyObject **seq_items = PySequence_Fast_ITEMS(seq_fast);

		index_len = seq_len * verts_per_prim;

		/* The `vertex_len` parameter is only used for asserts in the Debug build. */
		/* Not very useful in python since scripts are often tested in Release build. */
		/* Use `INT_MAX` instead of the actual number of vertices. */
		GPU_indexbuf_init(
		        &builder, params.type_id, index_len, INT_MAX);

		if (verts_per_prim == 1) {
			for (uint i = 0; i < seq_len; i++) {
				GPU_indexbuf_add_generic_vert(
				        &builder, PyC_Long_AsU32(seq_items[i]));
			}
		}
		else {
			for (uint i = 0; i < seq_len; i++) {
				PyObject *item = seq_items[i];
				if (!PyTuple_CheckExact(item)) {
					PyErr_Format(PyExc_ValueError,
					             "expected a tuple, got %s",
					             Py_TYPE(item)->tp_name);
					ok = false;
					goto finally;
				}
				if (PyTuple_GET_SIZE(item) != verts_per_prim) {
					PyErr_Format(PyExc_ValueError,
					             "Expected a Tuple of size %d, got %d",
					             PyTuple_GET_SIZE(item));
					ok = false;
					goto finally;
				}

				for (uint j = 0; j < verts_per_prim; j++) {
					GPU_indexbuf_add_generic_vert(
					        &builder,
					        PyC_Long_AsU32(PyTuple_GET_ITEM(item, j)));
				}
			}
		}

		if (PyErr_Occurred()) {
			ok = false;
		}

finally:

		Py_DECREF(seq_fast);
	}

	if (ok == false) {
		MEM_freeN(builder.data);
		return NULL;
	}

	return BPyGPUIndexBuf_CreatePyObject(GPU_indexbuf_build(&builder));
}

static void bpygpu_IndexBuf_dealloc(BPyGPUIndexBuf *self)
{
	GPU_indexbuf_discard(self->elem);
	Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(py_gpu_element_doc,
"GPUIndexBuf(type, seq)\n"
"\n"
"Contains a VBO."
"\n"
"   :param prim_type:\n"
"      One of these primitive types: {\n"
"      'POINTS',\n"
"      'LINES',\n"
"      'TRIS',\n"
"      'LINE_STRIP_ADJ'}\n"
"   :type type: `str`\n"
"   :param seq: Sequence of integers.\n"
"   :type buf: `Any 1D or 2D Sequence`\n"
);
PyTypeObject BPyGPUIndexBuf_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUIndexBuf",
	.tp_basicsize = sizeof(BPyGPUIndexBuf),
	.tp_dealloc = (destructor)bpygpu_IndexBuf_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = py_gpu_element_doc,
	.tp_new = bpygpu_IndexBuf_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public API
 * \{ */

PyObject *BPyGPUIndexBuf_CreatePyObject(GPUIndexBuf *elem)
{
	BPyGPUIndexBuf *self;

	self = PyObject_New(BPyGPUIndexBuf, &BPyGPUIndexBuf_Type);
	self->elem = elem;

	return (PyObject *)self;
}

/** \} */
