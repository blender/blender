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

/** \file blender/python/gawain/gwn_py_types.c
 *  \ingroup pygawain
 */

#include <Python.h>

#include "gawain/gwn_batch.h"
#include "gawain/gwn_vertex_format.h"

#include "BLI_math.h"

#include "GPU_batch.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gwn_py_types.h" /* own include */

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

static int BPy_Gwn_ParseVertCompType(PyObject *o, void *p)
{
	Py_ssize_t comp_type_id_len;
	const char *comp_type_id = _PyUnicode_AsStringAndSize(o, &comp_type_id_len);
	if (comp_type_id == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "expected a string, got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	Gwn_VertCompType comp_type;
	if (comp_type_id_len == 2) {
		switch (*((ushort *)comp_type_id)) {
			case MAKE_ID2('I', '8'): { comp_type = GWN_COMP_I8; goto success; }
			case MAKE_ID2('U', '8'): { comp_type = GWN_COMP_U8; goto success; }
		}
	}
	else if (comp_type_id_len == 3) {
		switch (*((uint *)comp_type_id)) {
			case MAKE_ID3('I', '1', '6'): { comp_type = GWN_COMP_I16; goto success; }
			case MAKE_ID3('U', '1', '6'): { comp_type = GWN_COMP_U16; goto success; }
			case MAKE_ID3('I', '3', '2'): { comp_type = GWN_COMP_I32; goto success; }
			case MAKE_ID3('U', '3', '2'): { comp_type = GWN_COMP_U32; goto success; }
			case MAKE_ID3('F', '3', '2'): { comp_type = GWN_COMP_F32; goto success; }
			case MAKE_ID3('I', '1', '0'): { comp_type = GWN_COMP_I10; goto success; }
		}
	}

	PyErr_Format(PyExc_ValueError,
	             "unknown type literal: '%s'",
	             comp_type_id);
	return 0;

success:
	*((Gwn_VertCompType *)p) = comp_type;
	return 1;
}

static int BPy_Gwn_ParseVertFetchMode(PyObject *o, void *p)
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
			mode = GWN_FETCH_##id; \
			goto success; \
		} \
	} ((void)0)

	Gwn_VertCompType mode;
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
	(*(Gwn_VertFetchMode *)p) = mode;
	return 1;
}

static int BPy_Gwn_ParsePrimType(PyObject *o, void *p)
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
			mode = GWN_PRIM_##id; \
			goto success; \
		} \
	} ((void)0)

	Gwn_PrimType mode;
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
	(*(Gwn_VertFetchMode *)p) = mode;
	return 1;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Utility Functions
 * \{ */

/* Use for both tuple and single item, TODO: GWN_COMP_I10 */
#define PY_AS_NATIVE_SWITCH \
	switch (attr->comp_type) { \
		case GWN_COMP_I8:  { PY_AS_NATIVE(int8_t,   PyLong_AsLong); break; } \
		case GWN_COMP_U8:  { PY_AS_NATIVE(uint8_t,  PyLong_AsLong); break; } \
		case GWN_COMP_I16: { PY_AS_NATIVE(int16_t,  PyLong_AsLong); break; } \
		case GWN_COMP_U16: { PY_AS_NATIVE(uint16_t, PyLong_AsLong); break; } \
		case GWN_COMP_I32: { PY_AS_NATIVE(int32_t,  PyLong_AsLong); break; } \
		case GWN_COMP_U32: { PY_AS_NATIVE(uint32_t, PyLong_AsLong); break; } \
		case GWN_COMP_F32: { PY_AS_NATIVE(float, PyFloat_AsDouble); break; } \
		default: \
			BLI_assert(0); \
	} ((void)0)

/* No error checking, callers must run PyErr_Occurred */
static void fill_format_elem(void *data_dst_void, PyObject *py_src, const Gwn_VertAttr *attr)
{
	/* TODO: check overflow */

#define PY_AS_NATIVE(t, py_as_native) \
	{ \
		t *data_dst = data_dst_void; \
		*data_dst = py_as_native(py_src); \
	} ((void)0)

	PY_AS_NATIVE_SWITCH;

#undef PY_AS_NATIVE
}

/* No error checking, callers must run PyErr_Occurred */
static void fill_format_tuple(void *data_dst_void, PyObject *py_src, const Gwn_VertAttr *attr)
{
	/* TODO: check overflow */
	const uint len = attr->comp_ct;

#define PY_AS_NATIVE(t, py_as_native) \
	t *data_dst = data_dst_void; \
	for (uint i = 0; i < len; i++) { \
		data_dst[i] = py_as_native(PyTuple_GET_ITEM(py_src, i)); \
	} ((void)0)

	PY_AS_NATIVE_SWITCH;

#undef PY_AS_NATIVE
}

#undef PY_AS_NATIVE_SWITCH

static bool pygwn_vertbuf_fill_impl(
        Gwn_VertBuf *vbo,
        uint data_id, PyObject *seq)
{
	bool ok = true;
	const Gwn_VertAttr *attr = &vbo->format.attribs[data_id];

	Gwn_VertBufRaw data_step;
	GWN_vertbuf_attr_get_raw_data(vbo, data_id, &data_step);

	PyObject *seq_fast = PySequence_Fast(seq, "Vertex buffer fill");
	if (seq_fast == NULL) {
		goto finally;
	}

	const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

	if (seq_len != vbo->vertex_ct) {
		PyErr_Format(PyExc_ValueError,
		             "Expected a sequence of size %d, got %d",
		             vbo->vertex_ct, seq_len);
	}

	PyObject **seq_items = PySequence_Fast_ITEMS(seq_fast);

	if (attr->comp_ct == 1) {
		for (uint i = 0; i < seq_len; i++) {
			uchar *data = (uchar *)GWN_vertbuf_raw_step(&data_step);
			PyObject *item = seq_items[i];
			fill_format_elem(data, item, attr);
		}
	}
	else {
		for (uint i = 0; i < seq_len; i++) {
			uchar *data = (uchar *)GWN_vertbuf_raw_step(&data_step);
			PyObject *item = seq_items[i];
			if (!PyTuple_CheckExact(item)) {
				PyErr_Format(PyExc_ValueError,
				             "expected a tuple, got %s",
				             Py_TYPE(item)->tp_name);
				ok = false;
				goto finally;
			}
			if (PyTuple_GET_SIZE(item) != attr->comp_ct) {
				PyErr_Format(PyExc_ValueError,
				             "expected a tuple of size %d, got %d",
				             attr->comp_ct, PyTuple_GET_SIZE(item));
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
static int pygwn_find_id(const Gwn_VertFormat *fmt, const char *id)
{
	for (int i = 0; i < fmt->attrib_ct; i++) {
		for (uint j = 0; j < fmt->name_ct; j++) {
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

/** \name VertFormat Type
 * \{ */

static PyObject *BPy_Gwn_VertFormat_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	if (PyTuple_GET_SIZE(args) || (kwds && PyDict_Size(kwds))) {
		PyErr_SetString(PyExc_TypeError,
		                "VertFormat(): takes no arguments");
		return NULL;
	}

	BPy_Gwn_VertFormat *ret = (BPy_Gwn_VertFormat *)BPy_Gwn_VertFormat_CreatePyObject(NULL);

	return (PyObject *)ret;
}

PyDoc_STRVAR(BPy_Gwn_VertFormat_attr_add_doc,
"TODO"
);
static PyObject *BPy_Gwn_VertFormat_attr_add(BPy_Gwn_VertFormat *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"id", "comp_type", "len", "fetch_mode", NULL};

	struct {
		const char *id;
		Gwn_VertCompType comp_type;
		uint len;
		Gwn_VertFetchMode fetch_mode;
	} params;

	if (self->fmt.attrib_ct == GWN_VERT_ATTR_MAX_LEN) {
		PyErr_SetString(PyExc_ValueError, "Maxumum attr reached " STRINGIFY(GWN_VERT_ATTR_MAX_LEN));
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "$sO&IO&:attr_add", (char **)kwlist,
	        &params.id,
	        BPy_Gwn_ParseVertCompType, &params.comp_type,
	        &params.len,
	        BPy_Gwn_ParseVertFetchMode, &params.fetch_mode))
	{
		return NULL;
	}

	uint attr_id = GWN_vertformat_attr_add(&self->fmt, params.id, params.comp_type, params.len, params.fetch_mode);
	return PyLong_FromLong(attr_id);
}

static struct PyMethodDef BPy_Gwn_VertFormat_methods[] = {
	{"attr_add", (PyCFunction) BPy_Gwn_VertFormat_attr_add,
	 METH_VARARGS | METH_KEYWORDS, BPy_Gwn_VertFormat_attr_add_doc},
	{NULL, NULL, 0, NULL}
};


static void BPy_Gwn_VertFormat_dealloc(BPy_Gwn_VertFormat *self)
{
	Py_TYPE(self)->tp_free(self);
}

PyTypeObject BPy_Gwn_VertFormat_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "Gwn_VertFormat",
	.tp_basicsize = sizeof(BPy_Gwn_VertFormat),
	.tp_dealloc = (destructor)BPy_Gwn_VertFormat_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = BPy_Gwn_VertFormat_methods,
	.tp_new = BPy_Gwn_VertFormat_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name VertBuf Type
 * \{ */

static PyObject *BPy_Gwn_VertBuf_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	const char * const keywords[] = {"len", "format", NULL};

	struct {
		BPy_Gwn_VertFormat *py_fmt;
		uint len;
	} params;

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds,
	        "$IO!:Gwn_VertBuf.__new__", (char **)keywords,
	        &params.len,
	        &BPy_Gwn_VertFormat_Type, &params.py_fmt))
	{
		return NULL;
	}

	struct Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&params.py_fmt->fmt);

	GWN_vertbuf_data_alloc(vbo, params.len);

	return BPy_Gwn_VertBuf_CreatePyObject(vbo);
}

PyDoc_STRVAR(BPy_Gwn_VertBuf_fill_doc,
"TODO"
);
static PyObject *BPy_Gwn_VertBuf_fill(BPy_Gwn_VertBuf *self, PyObject *args, PyObject *kwds)
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

	if (params.id >= self->buf->format.attrib_ct) {
		PyErr_Format(PyExc_ValueError,
		             "Format id '%s' out of range",
		             params.id);
	}

	if (!pygwn_vertbuf_fill_impl(self->buf, params.id, params.py_seq_data)) {
		return NULL;
	}
	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_Gwn_VertBuf_methods[] = {
	{"fill", (PyCFunction) BPy_Gwn_VertBuf_fill,
	 METH_VARARGS | METH_KEYWORDS, BPy_Gwn_VertBuf_fill_doc},
	{NULL, NULL, 0, NULL}
};

static void BPy_Gwn_VertBuf_dealloc(BPy_Gwn_VertBuf *self)
{
	GWN_vertbuf_discard(self->buf);
	Py_TYPE(self)->tp_free(self);
}

PyTypeObject BPy_Gwn_VertBuf_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "Gwn_VertBuf",
	.tp_basicsize = sizeof(BPy_Gwn_VertBuf),
	.tp_dealloc = (destructor)BPy_Gwn_VertBuf_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = BPy_Gwn_VertBuf_methods,
	.tp_new = BPy_Gwn_VertBuf_new,
};

/** \} */


/* -------------------------------------------------------------------- */

/** \name VertBatch Type
 * \{ */

static PyObject *BPy_Gwn_Batch_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
	const char * const keywords[] = {"type", "buf", NULL};

	struct {
		Gwn_PrimType type_id;
		BPy_Gwn_VertBuf *py_buf;
	} params;

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds,
	        "$O&O!:Gwn_Batch.__new__", (char **)keywords,
	        BPy_Gwn_ParsePrimType, &params.type_id,
	        &BPy_Gwn_VertBuf_Type, &params.py_buf))
	{
		return NULL;
	}

	Gwn_Batch *batch = GWN_batch_create(params.type_id, params.py_buf->buf, NULL);
	BPy_Gwn_Batch *ret = (BPy_Gwn_Batch *)BPy_Gwn_Batch_CreatePyObject(batch);

#ifdef USE_GWN_PY_REFERENCES
	ret->references = PyList_New(1);
	PyList_SET_ITEM(ret->references, 0, (PyObject *)params.py_buf);
	Py_INCREF(params.py_buf);
	PyObject_GC_Track(ret);
#endif

	return (PyObject *)ret;
}

PyDoc_STRVAR(BPy_Gwn_VertBatch_vertbuf_add_doc,
"TODO"
);
static PyObject *BPy_Gwn_VertBatch_vertbuf_add(BPy_Gwn_Batch *self, BPy_Gwn_VertBuf *py_buf)
{
	if (!BPy_Gwn_VertBuf_Check(py_buf)) {
		PyErr_Format(PyExc_TypeError,
		             "Expected a Gwn_VertBuf, got %s",
		             Py_TYPE(py_buf)->tp_name);
		return NULL;
	}

	if (self->batch->verts[0]->vertex_ct != py_buf->buf->vertex_ct) {
		PyErr_Format(PyExc_TypeError,
		             "Expected %d length, got %d",
		             self->batch->verts[0]->vertex_ct, py_buf->buf->vertex_ct);
		return NULL;
	}

#ifdef USE_GWN_PY_REFERENCES
	/* Hold user */
	PyList_Append(self->references, (PyObject *)py_buf);
#endif

	GWN_batch_vertbuf_add(self->batch, py_buf->buf);
	Py_RETURN_NONE;
}

/* Currently magic number from Py perspective. */
PyDoc_STRVAR(BPy_Gwn_VertBatch_program_set_builtin_doc,
"TODO"
);
static PyObject *BPy_Gwn_VertBatch_program_set_builtin(BPy_Gwn_Batch *self, PyObject *args, PyObject *kwds)
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
	GWN_batch_program_set_builtin(self->batch, shader);
	Py_RETURN_NONE;
}

static PyObject *BPy_Gwn_VertBatch_uniform_bool(BPy_Gwn_Batch *self, PyObject *args)
{
	struct {
		const char *id;
		bool values[1];
	} params;

	if (!PyArg_ParseTuple(
	        args, "si:uniform_b",
	        &params.id,
	        &params.values[0]))
	{
		return NULL;
	}

	GWN_batch_uniform_1b(self->batch, params.id, params.values[0]);
	Py_RETURN_NONE;
}

static PyObject *BPy_Gwn_VertBatch_uniform_i32(BPy_Gwn_Batch *self, PyObject *args)
{
	struct {
		const char *id;
		int values[1];
	} params;

	if (!PyArg_ParseTuple(
	        args, "si:uniform_i",
	        &params.id,
	        &params.values[0]))
	{
		return NULL;
	}

	GWN_batch_uniform_1i(self->batch, params.id, params.values[0]);
	Py_RETURN_NONE;
}

static PyObject *BPy_Gwn_VertBatch_uniform_f32(BPy_Gwn_Batch *self, PyObject *args)
{
	struct {
		const char *id;
		float values[4];
	} params;

	if (!PyArg_ParseTuple(
	        args, "sf|fff:uniform_f",
	        &params.id,
	        &params.values[0], &params.values[1], &params.values[2], &params.values[3]))
	{
		return NULL;
	}

	switch(PyTuple_GET_SIZE(args)) {
		case 2: GWN_batch_uniform_1f(self->batch,  params.id,  params.values[0]); break;
		case 3: GWN_batch_uniform_2fv(self->batch, params.id, params.values);     break;
		case 4: GWN_batch_uniform_3fv(self->batch, params.id, params.values);     break;
		case 5: GWN_batch_uniform_4fv(self->batch, params.id, params.values);     break;
		default:
			BLI_assert(0);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_Gwn_VertBatch_draw_doc,
"TODO"
);
static PyObject *BPy_Gwn_VertBatch_draw(BPy_Gwn_Batch *self)
{
	if (!glIsProgram(self->batch->program)) {
		PyErr_SetString(PyExc_ValueError,
		                "batch program has not not set");
	}
	GWN_batch_draw(self->batch);
	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_Gwn_VertBatch_methods[] = {
	{"vertbuf_add", (PyCFunction) BPy_Gwn_VertBatch_vertbuf_add,
	 METH_O, BPy_Gwn_VertBatch_vertbuf_add_doc},
	{"program_set_builtin", (PyCFunction) BPy_Gwn_VertBatch_program_set_builtin,
	 METH_VARARGS | METH_KEYWORDS, BPy_Gwn_VertBatch_program_set_builtin_doc},
	{"uniform_bool", (PyCFunction) BPy_Gwn_VertBatch_uniform_bool,
	 METH_VARARGS, NULL},
	{"uniform_i32", (PyCFunction) BPy_Gwn_VertBatch_uniform_i32,
	 METH_VARARGS, NULL},
	{"uniform_f32", (PyCFunction) BPy_Gwn_VertBatch_uniform_f32,
	  METH_VARARGS, NULL},
	{"draw", (PyCFunction) BPy_Gwn_VertBatch_draw,
	 METH_NOARGS, BPy_Gwn_VertBatch_draw_doc},
	{NULL, NULL, 0, NULL}
};

#ifdef USE_GWN_PY_REFERENCES

static int BPy_Gwn_Batch_traverse(BPy_Gwn_Batch *self, visitproc visit, void *arg)
{
	Py_VISIT(self->references);
	return 0;
}

static int BPy_Gwn_Batch_clear(BPy_Gwn_Batch *self)
{
	Py_CLEAR(self->references);
	return 0;
}

#endif

static void BPy_Gwn_Batch_dealloc(BPy_Gwn_Batch *self)
{
	GWN_batch_discard(self->batch);

#ifdef USE_GWN_PY_REFERENCES
	if (self->references) {
		PyObject_GC_UnTrack(self);
		BPy_Gwn_Batch_clear(self);
		Py_XDECREF(self->references);
	}
#endif

	Py_TYPE(self)->tp_free(self);
}

PyTypeObject BPy_Gwn_Batch_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "Gwn_Batch",
	.tp_basicsize = sizeof(BPy_Gwn_Batch),
	.tp_dealloc = (destructor)BPy_Gwn_Batch_dealloc,
#ifdef USE_GWN_PY_REFERENCES
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
	.tp_traverse = (traverseproc)BPy_Gwn_Batch_traverse,
	.tp_clear = (inquiry)BPy_Gwn_Batch_clear,
#else
	.tp_flags = Py_TPFLAGS_DEFAULT,
#endif
	.tp_methods = BPy_Gwn_VertBatch_methods,
	.tp_new = BPy_Gwn_Batch_new,
};

/* -------------------------------------------------------------------- */


/** \name Gawain Types Module
 * \{ */

static struct PyModuleDef BPy_BM_types_module_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "_gawain.types",
};

PyObject *BPyInit_gawain_types(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BM_types_module_def);

	if (PyType_Ready(&BPy_Gwn_VertFormat_Type) < 0)
		return NULL;
	if (PyType_Ready(&BPy_Gwn_VertBuf_Type) < 0)
		return NULL;
	if (PyType_Ready(&BPy_Gwn_Batch_Type) < 0)
		return NULL;

#define MODULE_TYPE_ADD(s, t) \
	PyModule_AddObject(s, t.tp_name, (PyObject *)&t); Py_INCREF((PyObject *)&t)

	MODULE_TYPE_ADD(submodule, BPy_Gwn_VertFormat_Type);
	MODULE_TYPE_ADD(submodule, BPy_Gwn_VertBuf_Type);
	MODULE_TYPE_ADD(submodule, BPy_Gwn_Batch_Type);

#undef MODULE_TYPE_ADD

	return submodule;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Public API
 * \{ */

PyObject *BPy_Gwn_VertFormat_CreatePyObject(Gwn_VertFormat *fmt)
{
	BPy_Gwn_VertFormat *self;

	self = PyObject_New(BPy_Gwn_VertFormat, &BPy_Gwn_VertFormat_Type);
	if (fmt) {
		self->fmt = *fmt;
	}
	else {
		memset(&self->fmt, 0, sizeof(self->fmt));
	}

	return (PyObject *)self;
}

PyObject *BPy_Gwn_VertBuf_CreatePyObject(Gwn_VertBuf *buf)
{
	BPy_Gwn_VertBuf *self;

	self = PyObject_New(BPy_Gwn_VertBuf, &BPy_Gwn_VertBuf_Type);
	self->buf = buf;

	return (PyObject *)self;
}


PyObject *BPy_Gwn_Batch_CreatePyObject(Gwn_Batch *batch)
{
	BPy_Gwn_Batch *self;

#ifdef USE_GWN_PY_REFERENCES
	self = (BPy_Gwn_Batch *)_PyObject_GC_New(&BPy_Gwn_Batch_Type);
	self->references = NULL;
#else
	self = PyObject_New(BPy_Gwn_Batch, &BPy_Gwn_Batch_Type);
#endif

	self->batch = batch;

	return (PyObject *)self;
}

/** \} */
