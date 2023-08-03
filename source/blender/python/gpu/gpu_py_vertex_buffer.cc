/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "GPU_vertex_buffer.h"

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_vertex_buffer.h" /* own include */
#include "gpu_py_vertex_format.h"

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

#define PYGPU_AS_NATIVE_SWITCH(attr) \
  switch (attr->comp_type) { \
    case GPU_COMP_I8: { \
      PY_AS_NATIVE(int8_t, PyC_Long_AsI8); \
      break; \
    } \
    case GPU_COMP_U8: { \
      PY_AS_NATIVE(uint8_t, PyC_Long_AsU8); \
      break; \
    } \
    case GPU_COMP_I16: { \
      PY_AS_NATIVE(int16_t, PyC_Long_AsI16); \
      break; \
    } \
    case GPU_COMP_U16: { \
      PY_AS_NATIVE(uint16_t, PyC_Long_AsU16); \
      break; \
    } \
    case GPU_COMP_I32: { \
      PY_AS_NATIVE(int32_t, PyC_Long_AsI32); \
      break; \
    } \
    case GPU_COMP_U32: { \
      PY_AS_NATIVE(uint32_t, PyC_Long_AsU32); \
      break; \
    } \
    case GPU_COMP_F32: { \
      PY_AS_NATIVE(float, PyFloat_AsDouble); \
      break; \
    } \
    default: \
      BLI_assert_unreachable(); \
      break; \
  } \
  ((void)0)

/* No error checking, callers must run PyErr_Occurred */
static void pygpu_fill_format_elem(void *data_dst_void, PyObject *py_src, const GPUVertAttr *attr)
{
#define PY_AS_NATIVE(ty_dst, py_as_native) \
  { \
    ty_dst *data_dst = static_cast<ty_dst *>(data_dst_void); \
    *data_dst = py_as_native(py_src); \
  } \
  ((void)0)

  PYGPU_AS_NATIVE_SWITCH(attr);

#undef PY_AS_NATIVE
}

/* No error checking, callers must run PyErr_Occurred */
static void pygpu_fill_format_sequence(void *data_dst_void,
                                       PyObject *py_seq_fast,
                                       const GPUVertAttr *attr)
{
  const uint len = attr->comp_len;
  PyObject **value_fast_items = PySequence_Fast_ITEMS(py_seq_fast);

/**
 * Args are constants, so range checks will be optimized out if they're no-op's.
 */
#define PY_AS_NATIVE(ty_dst, py_as_native) \
  ty_dst *data_dst = static_cast<ty_dst *>(data_dst_void); \
  for (uint i = 0; i < len; i++) { \
    data_dst[i] = py_as_native(value_fast_items[i]); \
  } \
  ((void)0)

  PYGPU_AS_NATIVE_SWITCH(attr);

#undef PY_AS_NATIVE
}

#undef PYGPU_AS_NATIVE_SWITCH
#undef WARN_TYPE_LIMIT_PUSH
#undef WARN_TYPE_LIMIT_POP

static bool pygpu_vertbuf_fill_impl(GPUVertBuf *vbo,
                                    uint data_id,
                                    PyObject *seq,
                                    const char *error_prefix)
{
  const char *exc_str_size_mismatch = "Expected a %s of size %d, got %u";

  bool ok = true;
  const GPUVertAttr *attr = &GPU_vertbuf_get_format(vbo)->attrs[data_id];
  uint vert_len = GPU_vertbuf_get_vertex_len(vbo);

  if (PyObject_CheckBuffer(seq)) {
    Py_buffer pybuffer;

    if (PyObject_GetBuffer(seq, &pybuffer, PyBUF_STRIDES | PyBUF_ND) == -1) {
      /* PyObject_GetBuffer raise a PyExc_BufferError */
      return false;
    }

    const uint comp_len = pybuffer.ndim == 1 ? 1 : uint(pybuffer.shape[1]);

    if (pybuffer.shape[0] != vert_len) {
      PyErr_Format(
          PyExc_ValueError, exc_str_size_mismatch, "sequence", vert_len, pybuffer.shape[0]);
      ok = false;
    }
    else if (comp_len != attr->comp_len) {
      PyErr_Format(PyExc_ValueError, exc_str_size_mismatch, "component", attr->comp_len, comp_len);
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
    if (seq_fast == nullptr) {
      return false;
    }

    const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

    if (seq_len != vert_len) {
      PyErr_Format(PyExc_ValueError, exc_str_size_mismatch, "sequence", vert_len, seq_len);
    }

    PyObject **seq_items = PySequence_Fast_ITEMS(seq_fast);

    if (attr->comp_len == 1) {
      for (uint i = 0; i < seq_len; i++) {
        uchar *data = (uchar *)GPU_vertbuf_raw_step(&data_step);
        PyObject *item = seq_items[i];
        pygpu_fill_format_elem(data, item, attr);
      }
    }
    else {
      for (uint i = 0; i < seq_len; i++) {
        uchar *data = (uchar *)GPU_vertbuf_raw_step(&data_step);
        PyObject *seq_fast_item = PySequence_Fast(seq_items[i], error_prefix);

        if (seq_fast_item == nullptr) {
          ok = false;
          goto finally;
        }
        if (PySequence_Fast_GET_SIZE(seq_fast_item) != attr->comp_len) {
          PyErr_Format(PyExc_ValueError,
                       exc_str_size_mismatch,
                       "sequence",
                       attr->comp_len,
                       PySequence_Fast_GET_SIZE(seq_fast_item));
          ok = false;
          Py_DECREF(seq_fast_item);
          goto finally;
        }

        /* May trigger error, check below */
        pygpu_fill_format_sequence(data, seq_fast_item, attr);
        Py_DECREF(seq_fast_item);
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

static int pygpu_vertbuf_fill(GPUVertBuf *buf,
                              int id,
                              PyObject *py_seq_data,
                              const char *error_prefix)
{
  if (id < 0 || id >= GPU_vertbuf_get_format(buf)->attr_len) {
    PyErr_Format(PyExc_ValueError, "Format id %d out of range", id);
    return 0;
  }

  if (GPU_vertbuf_get_data(buf) == nullptr) {
    PyErr_SetString(PyExc_ValueError, "Can't fill, static buffer already in use");
    return 0;
  }

  if (!pygpu_vertbuf_fill_impl(buf, uint(id), py_seq_data, error_prefix)) {
    return 0;
  }

  return 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VertBuf Type
 * \{ */

static PyObject *pygpu_vertbuf__tp_new(PyTypeObject * /*type*/, PyObject *args, PyObject *kwds)
{
  struct {
    PyObject *py_fmt;
    uint len;
  } params;

  static const char *_keywords[] = {"format", "len", nullptr};
  static _PyArg_Parser _parser = {
      "O!" /* `format` */
      "I"  /* `len` */
      ":GPUVertBuf.__new__",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &BPyGPUVertFormat_Type, &params.py_fmt, &params.len))
  {
    return nullptr;
  }

  const GPUVertFormat *fmt = &((BPyGPUVertFormat *)params.py_fmt)->fmt;
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(fmt);

  GPU_vertbuf_data_alloc(vbo, params.len);

  return BPyGPUVertBuf_CreatePyObject(vbo);
}

PyDoc_STRVAR(pygpu_vertbuf_attr_fill_doc,
             ".. method:: attr_fill(id, data)\n"
             "\n"
             "   Insert data into the buffer for a single attribute.\n"
             "\n"
             "   :arg id: Either the name or the id of the attribute.\n"
             "   :type id: int or str\n"
             "   :arg data: Sequence of data that should be stored in the buffer\n"
             "   :type data: sequence of floats, ints, vectors or matrices\n");
static PyObject *pygpu_vertbuf_attr_fill(BPyGPUVertBuf *self, PyObject *args, PyObject *kwds)
{
  PyObject *data;
  PyObject *identifier;

  static const char *_keywords[] = {"id", "data", nullptr};
  static _PyArg_Parser _parser = {
      "O" /* `id` */
      "O" /* `data` */
      ":attr_fill",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &identifier, &data)) {
    return nullptr;
  }

  int id;

  if (PyLong_Check(identifier)) {
    id = PyLong_AsLong(identifier);
  }
  else if (PyUnicode_Check(identifier)) {
    const GPUVertFormat *format = GPU_vertbuf_get_format(self->buf);
    const char *name = PyUnicode_AsUTF8(identifier);
    id = GPU_vertformat_attr_id_get(format, name);
    if (id == -1) {
      PyErr_Format(PyExc_ValueError, "Unknown attribute '%s'", name);
      return nullptr;
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "expected int or str type as identifier");
    return nullptr;
  }

  if (!pygpu_vertbuf_fill(self->buf, id, data, "GPUVertBuf.attr_fill")) {
    return nullptr;
  }

  Py_RETURN_NONE;
}

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef pygpu_vertbuf__tp_methods[] = {
    {"attr_fill",
     (PyCFunction)pygpu_vertbuf_attr_fill,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_vertbuf_attr_fill_doc},
    {nullptr, nullptr, 0, nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

static void pygpu_vertbuf__tp_dealloc(BPyGPUVertBuf *self)
{
  GPU_vertbuf_discard(self->buf);
  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pygpu_vertbuf__tp_doc,
             ".. class:: GPUVertBuf(format, len)\n"
             "\n"
             "   Contains a VBO.\n"
             "\n"
             "   :arg format: Vertex format.\n"
             "   :type format: :class:`gpu.types.GPUVertFormat`\n"
             "   :arg len: Amount of vertices that will fit into this buffer.\n"
             "   :type len: int\n");
PyTypeObject BPyGPUVertBuf_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUVertBuf",
    /*tp_basicsize*/ sizeof(BPyGPUVertBuf),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_vertbuf__tp_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ pygpu_vertbuf__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_vertbuf__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_vertbuf__tp_new,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
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
