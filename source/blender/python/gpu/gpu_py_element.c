/*
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
 */

/** \file
 * \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_index_buffer.h"

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_api.h"
#include "gpu_py_element.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name IndexBuf Type
 * \{ */

static PyObject *pygpu_IndexBuf__tp_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  const char *error_prefix = "IndexBuf.__new__";
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
          args, kwds, &_parser, bpygpu_ParsePrimType, &params.type_id, &params.seq)) {
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
      PyErr_Format(PyExc_ValueError, "Each primitive must exactly %d indices", verts_per_prim);
      return NULL;
    }

    if (pybuffer.itemsize != 4 ||
        PyC_StructFmt_type_is_float_any(PyC_StructFmt_type_from_str(pybuffer.format))) {
      PyErr_Format(PyExc_ValueError, "Each index must be an 4-bytes integer value");
      return NULL;
    }

    index_len = pybuffer.shape[0];
    if (pybuffer.ndim != 1) {
      index_len *= pybuffer.shape[1];
    }

    /* The `vertex_len` parameter is only used for asserts in the Debug build. */
    /* Not very useful in python since scripts are often tested in Release build. */
    /* Use `INT_MAX` instead of the actual number of vertices. */
    GPU_indexbuf_init(&builder, params.type_id, index_len, INT_MAX);

#if 0
    uint *buf = pybuffer.buf;
    for (uint i = index_len; i--; buf++) {
      GPU_indexbuf_add_generic_vert(&builder, *buf);
    }
#else
    memcpy(builder.data, pybuffer.buf, index_len * sizeof(*builder.data));
    builder.index_len = index_len;
#endif
    PyBuffer_Release(&pybuffer);
  }
  else {
    PyObject *seq_fast = PySequence_Fast(params.seq, error_prefix);

    if (seq_fast == NULL) {
      return false;
    }

    const uint seq_len = PySequence_Fast_GET_SIZE(seq_fast);

    PyObject **seq_items = PySequence_Fast_ITEMS(seq_fast);

    index_len = seq_len * verts_per_prim;

    /* The `vertex_len` parameter is only used for asserts in the Debug build. */
    /* Not very useful in python since scripts are often tested in Release build. */
    /* Use `INT_MAX` instead of the actual number of vertices. */
    GPU_indexbuf_init(&builder, params.type_id, index_len, INT_MAX);

    if (verts_per_prim == 1) {
      for (uint i = 0; i < seq_len; i++) {
        GPU_indexbuf_add_generic_vert(&builder, PyC_Long_AsU32(seq_items[i]));
      }
    }
    else {
      int values[4];
      for (uint i = 0; i < seq_len; i++) {
        PyObject *seq_fast_item = PySequence_Fast(seq_items[i], error_prefix);
        if (seq_fast_item == NULL) {
          PyErr_Format(PyExc_TypeError,
                       "%s: expected a sequence, got %s",
                       error_prefix,
                       Py_TYPE(seq_items[i])->tp_name);
          ok = false;
          goto finally;
        }

        ok = PyC_AsArray_FAST(
                 values, seq_fast_item, verts_per_prim, &PyLong_Type, false, error_prefix) == 0;

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
    return NULL;
  }

  return BPyGPUIndexBuf_CreatePyObject(GPU_indexbuf_build(&builder));
}

static void pygpu_IndexBuf__tp_dealloc(BPyGPUIndexBuf *self)
{
  GPU_indexbuf_discard(self->elem);
  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pygpu_IndexBuf__tp_doc,
             ".. class:: GPUIndexBuf(type, seq)\n"
             "\n"
             "   Contains an index buffer.\n"
             "\n"
             "   :param type: One of these primitive types: {\n"
             "      `POINTS`,\n"
             "      `LINES`,\n"
             "      `TRIS`,\n"
             "      `LINE_STRIP_ADJ` }\n"
             "   :type type: `str`\n"
             "   :param seq: Indices this index buffer will contain.\n"
             "      Whether a 1D or 2D sequence is required depends on the type.\n"
             "      Optionally the sequence can support the buffer protocol.\n"
             "   :type seq: 1D or 2D sequence\n");
PyTypeObject BPyGPUIndexBuf_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUIndexBuf",
    .tp_basicsize = sizeof(BPyGPUIndexBuf),
    .tp_dealloc = (destructor)pygpu_IndexBuf__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = pygpu_IndexBuf__tp_doc,
    .tp_new = pygpu_IndexBuf__tp_new,
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
