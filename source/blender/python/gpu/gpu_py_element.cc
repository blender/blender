/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "GPU_index_buffer.hh"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "gpu_py.hh"
#include "gpu_py_element.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name IndexBuf Type
 * \{ */

static PyObject *pygpu_IndexBuf__tp_new(PyTypeObject * /*type*/, PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  const char *error_prefix = "IndexBuf.__new__";
  bool ok = true;

  PyC_StringEnum prim_type = {bpygpu_primtype_items, GPU_PRIM_NONE};
  PyObject *seq;

  uint verts_per_prim;
  uint index_len;
  GPUIndexBufBuilder builder;

  static const char *_keywords[] = {"type", "seq", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "$"  /* Keyword only arguments. */
      "O&" /* `type` */
      "O"  /* `seq` */
      ":IndexBuf.__new__",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, PyC_ParseStringEnum, &prim_type, &seq))
  {
    return nullptr;
  }

  verts_per_prim = GPU_indexbuf_primitive_len(GPUPrimType(prim_type.value_found));
  if (verts_per_prim == -1) {
    PyErr_SetString(PyExc_ValueError,
                    "The argument 'type' must be "
                    "'POINTS', 'LINES', 'TRIS', 'LINES_ADJ' or 'TRIS_ADJ'");
    return nullptr;
  }

  if (PyObject_CheckBuffer(seq)) {
    Py_buffer pybuffer;

    if (PyObject_GetBuffer(seq, &pybuffer, PyBUF_FORMAT | PyBUF_ND) == -1) {
      /* PyObject_GetBuffer already handles error messages. */
      return nullptr;
    }

    if (pybuffer.ndim != 1 && pybuffer.shape[1] != verts_per_prim) {
      PyErr_Format(PyExc_ValueError, "Each primitive must exactly %d indices", verts_per_prim);
      PyBuffer_Release(&pybuffer);
      return nullptr;
    }

    if (pybuffer.itemsize != 4 ||
        PyC_StructFmt_type_is_float_any(PyC_StructFmt_type_from_str(pybuffer.format)))
    {
      PyErr_SetString(PyExc_ValueError, "Each index must be an 4-bytes integer value");
      PyBuffer_Release(&pybuffer);
      return nullptr;
    }

    index_len = pybuffer.shape[0];
    if (pybuffer.ndim != 1) {
      index_len *= pybuffer.shape[1];
    }

    /* The `vertex_len` parameter is only used for asserts in the Debug build. */
    /* Not very useful in python since scripts are often tested in Release build. */
    /* Use `INT_MAX` instead of the actual number of vertices. */
    GPU_indexbuf_init(&builder, GPUPrimType(prim_type.value_found), index_len, INT_MAX);

    uint *buf = static_cast<uint *>(pybuffer.buf);
    for (uint i = index_len; i--; buf++) {
      GPU_indexbuf_add_generic_vert(&builder, *buf);
    }

    PyBuffer_Release(&pybuffer);
  }
  else {
    PyObject *seq_fast = PySequence_Fast(seq, error_prefix);

    if (seq_fast == nullptr) {
      return nullptr;
    }

    const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

    PyObject **seq_items = PySequence_Fast_ITEMS(seq_fast);

    index_len = seq_len * verts_per_prim;

    /* The `vertex_len` parameter is only used for asserts in the Debug build. */
    /* Not very useful in python since scripts are often tested in Release build. */
    /* Use `INT_MAX` instead of the actual number of vertices. */
    GPU_indexbuf_init(&builder, GPUPrimType(prim_type.value_found), index_len, INT_MAX);

    if (verts_per_prim == 1) {
      for (uint i = 0; i < seq_len; i++) {
        GPU_indexbuf_add_generic_vert(&builder, PyC_Long_AsU32(seq_items[i]));
      }
    }
    else {
      int values[4];
      for (uint i = 0; i < seq_len; i++) {
        PyObject *seq_fast_item = PySequence_Fast(seq_items[i], error_prefix);
        if (seq_fast_item == nullptr) {
          PyErr_Format(PyExc_TypeError,
                       "%s: expected a sequence, got %s",
                       error_prefix,
                       Py_TYPE(seq_items[i])->tp_name);
          ok = false;
          goto finally;
        }

        ok = PyC_AsArray_FAST(values,
                              sizeof(*values),
                              seq_fast_item,
                              verts_per_prim,
                              &PyLong_Type,
                              error_prefix) == 0;

        if (ok) {
          for (uint j = 0; j < verts_per_prim; j++) {
            GPU_indexbuf_add_generic_vert(&builder, values[j]);
          }
        }
        Py_DECREF(seq_fast_item);
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
    return nullptr;
  }

  return BPyGPUIndexBuf_CreatePyObject(GPU_indexbuf_build(&builder));
}

static void pygpu_IndexBuf__tp_dealloc(BPyGPUIndexBuf *self)
{
  GPU_indexbuf_discard(self->elem);
  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_IndexBuf__tp_doc,
    ".. class:: GPUIndexBuf(type, seq)\n"
    "\n"
    "   Contains an index buffer.\n"
    "\n"
    "   :arg type: The primitive type this index buffer is composed of.\n"
    "      Possible values are [``POINTS``, ``LINES``, ``TRIS``, ``LINES_ADJ``, ``TRIS_ADJ``].\n"
    "   :type type: str\n"
    "   :arg seq: Indices this index buffer will contain.\n"
    "      Whether a 1D or 2D sequence is required depends on the type.\n"
    "      Optionally the sequence can support the buffer protocol.\n"
    "   :type seq: Buffer | Sequence[int] | Sequence[Sequence[int]]\n");
PyTypeObject BPyGPUIndexBuf_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUIndexBuf",
    /*tp_basicsize*/ sizeof(BPyGPUIndexBuf),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_IndexBuf__tp_dealloc,
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
    /*tp_doc*/ pygpu_IndexBuf__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_IndexBuf__tp_new,
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

PyObject *BPyGPUIndexBuf_CreatePyObject(blender::gpu::IndexBuf *elem)
{
  BPyGPUIndexBuf *self;

  self = PyObject_New(BPyGPUIndexBuf, &BPyGPUIndexBuf_Type);
  self->elem = elem;

  return (PyObject *)self;
}

/** \} */
