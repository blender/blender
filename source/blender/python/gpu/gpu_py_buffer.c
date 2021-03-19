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
 * This file defines the gpu.state API.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "GPU_texture.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h"

#include "gpu_py_buffer.h"

// #define PYGPU_BUFFER_PROTOCOL

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static bool pygpu_buffer_dimensions_compare(int ndim,
                                            const Py_ssize_t *shape_a,
                                            const Py_ssize_t *shape_b)
{
  return (bool)memcmp(shape_a, shape_b, ndim * sizeof(Py_ssize_t));
}

static const char *pygpu_buffer_formatstr(eGPUDataFormat data_format)
{
  switch (data_format) {
    case GPU_DATA_FLOAT:
      return "f";
    case GPU_DATA_INT:
      return "i";
    case GPU_DATA_UINT:
      return "I";
    case GPU_DATA_UBYTE:
      return "B";
    case GPU_DATA_UINT_24_8:
    case GPU_DATA_10_11_11_REV:
      return "I";
    default:
      break;
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BPyGPUBuffer API
 * \{ */

static BPyGPUBuffer *pygpu_buffer_make_from_data(PyObject *parent,
                                                 const eGPUDataFormat format,
                                                 const int shape_len,
                                                 const Py_ssize_t *shape,
                                                 void *buf)
{
  BPyGPUBuffer *buffer = (BPyGPUBuffer *)_PyObject_GC_New(&BPyGPU_BufferType);

  buffer->parent = NULL;
  buffer->format = format;
  buffer->shape_len = shape_len;
  buffer->shape = MEM_mallocN(shape_len * sizeof(*buffer->shape), "BPyGPUBuffer shape");
  memcpy(buffer->shape, shape, shape_len * sizeof(*buffer->shape));
  buffer->buf.as_void = buf;

  if (parent) {
    Py_INCREF(parent);
    buffer->parent = parent;
    PyObject_GC_Track(buffer);
  }
  return buffer;
}

static PyObject *pygpu_buffer__sq_item(BPyGPUBuffer *self, int i)
{
  if (i >= self->shape[0] || i < 0) {
    PyErr_SetString(PyExc_IndexError, "array index out of range");
    return NULL;
  }

  const char *formatstr = pygpu_buffer_formatstr(self->format);

  if (self->shape_len == 1) {
    switch (self->format) {
      case GPU_DATA_FLOAT:
        return Py_BuildValue(formatstr, self->buf.as_float[i]);
      case GPU_DATA_INT:
        return Py_BuildValue(formatstr, self->buf.as_int[i]);
      case GPU_DATA_UBYTE:
        return Py_BuildValue(formatstr, self->buf.as_byte[i]);
      case GPU_DATA_UINT:
      case GPU_DATA_UINT_24_8:
      case GPU_DATA_10_11_11_REV:
        return Py_BuildValue(formatstr, self->buf.as_uint[i]);
    }
  }
  else {
    int offset = i * GPU_texture_dataformat_size(self->format);
    for (int j = 1; j < self->shape_len; j++) {
      offset *= self->shape[j];
    }

    return (PyObject *)pygpu_buffer_make_from_data((PyObject *)self,
                                                   self->format,
                                                   self->shape_len - 1,
                                                   self->shape + 1,
                                                   self->buf.as_byte + offset);
  }

  return NULL;
}

static PyObject *pygpu_buffer_to_list(BPyGPUBuffer *self)
{
  int i, len = self->shape[0];
  PyObject *list = PyList_New(len);

  for (i = 0; i < len; i++) {
    PyList_SET_ITEM(list, i, pygpu_buffer__sq_item(self, i));
  }

  return list;
}

static PyObject *pygpu_buffer_to_list_recursive(BPyGPUBuffer *self)
{
  PyObject *list;

  if (self->shape_len > 1) {
    int i, len = self->shape[0];
    list = PyList_New(len);

    for (i = 0; i < len; i++) {
      /* "BPyGPUBuffer *sub_tmp" is a temporary object created just to be read for nested lists.
       * That is why it is decremented/freed soon after.
       * TODO: For efficiency, avoid creating #BPyGPUBuffer when creating nested lists. */
      BPyGPUBuffer *sub_tmp = (BPyGPUBuffer *)pygpu_buffer__sq_item(self, i);
      PyList_SET_ITEM(list, i, pygpu_buffer_to_list_recursive(sub_tmp));
      Py_DECREF(sub_tmp);
    }
  }
  else {
    list = pygpu_buffer_to_list(self);
  }

  return list;
}

static PyObject *pygpu_buffer_dimensions(BPyGPUBuffer *self, void *UNUSED(arg))
{
  PyObject *list = PyList_New(self->shape_len);
  int i;

  for (i = 0; i < self->shape_len; i++) {
    PyList_SET_ITEM(list, i, PyLong_FromLong(self->shape[i]));
  }

  return list;
}

static int pygpu_buffer__tp_traverse(BPyGPUBuffer *self, visitproc visit, void *arg)
{
  Py_VISIT(self->parent);
  return 0;
}

static int pygpu_buffer__tp_clear(BPyGPUBuffer *self)
{
  Py_CLEAR(self->parent);
  return 0;
}

static void pygpu_buffer__tp_dealloc(BPyGPUBuffer *self)
{
  if (self->parent) {
    PyObject_GC_UnTrack(self);
    Py_CLEAR(self->parent);
  }
  else {
    MEM_freeN(self->buf.as_void);
  }

  MEM_freeN(self->shape);

  PyObject_GC_Del(self);
}

static PyObject *pygpu_buffer__tp_repr(BPyGPUBuffer *self)
{
  PyObject *repr;

  PyObject *list = pygpu_buffer_to_list_recursive(self);
  const char *typestr = PyC_StringEnum_FindIDFromValue(bpygpu_dataformat_items, self->format);

  repr = PyUnicode_FromFormat("Buffer(%s, %R)", typestr, list);
  Py_DECREF(list);

  return repr;
}

static int pygpu_buffer__sq_ass_item(BPyGPUBuffer *self, int i, PyObject *v);

static int pygpu_buffer_ass_slice(BPyGPUBuffer *self,
                                  Py_ssize_t begin,
                                  Py_ssize_t end,
                                  PyObject *seq)
{
  PyObject *item;
  int count, err = 0;

  if (begin < 0) {
    begin = 0;
  }
  if (end > self->shape[0]) {
    end = self->shape[0];
  }
  if (begin > end) {
    begin = end;
  }

  if (!PySequence_Check(seq)) {
    PyErr_Format(PyExc_TypeError,
                 "buffer[:] = value, invalid assignment. "
                 "Expected a sequence, not an %.200s type",
                 Py_TYPE(seq)->tp_name);
    return -1;
  }

  /* re-use count var */
  if ((count = PySequence_Size(seq)) != (end - begin)) {
    PyErr_Format(PyExc_TypeError,
                 "buffer[:] = value, size mismatch in assignment. "
                 "Expected: %d (given: %d)",
                 count,
                 end - begin);
    return -1;
  }

  for (count = begin; count < end; count++) {
    item = PySequence_GetItem(seq, count - begin);
    if (item) {
      err = pygpu_buffer__sq_ass_item(self, count, item);
      Py_DECREF(item);
    }
    else {
      err = -1;
    }
    if (err) {
      break;
    }
  }
  return err;
}

#define MAX_DIMENSIONS 64
static PyObject *pygpu_buffer__tp_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
  PyObject *length_ob, *init = NULL;
  BPyGPUBuffer *buffer = NULL;
  Py_ssize_t shape[MAX_DIMENSIONS];

  Py_ssize_t i, shape_len = 0;

  if (kwds && PyDict_Size(kwds)) {
    PyErr_SetString(PyExc_TypeError, "Buffer(): takes no keyword args");
    return NULL;
  }

  const struct PyC_StringEnum pygpu_dataformat = {bpygpu_dataformat_items, GPU_DATA_FLOAT};
  if (!PyArg_ParseTuple(
          args, "O&O|O: Buffer", PyC_ParseStringEnum, &pygpu_dataformat, &length_ob, &init)) {
    return NULL;
  }

  if (PyLong_Check(length_ob)) {
    shape_len = 1;
    if (((shape[0] = PyLong_AsLong(length_ob)) < 1)) {
      PyErr_SetString(PyExc_AttributeError, "dimension must be greater than or equal to 1");
      return NULL;
    }
  }
  else if (PySequence_Check(length_ob)) {
    shape_len = PySequence_Size(length_ob);
    if (shape_len > MAX_DIMENSIONS) {
      PyErr_SetString(PyExc_AttributeError,
                      "too many dimensions, max is " STRINGIFY(MAX_DIMENSIONS));
      return NULL;
    }
    if (shape_len < 1) {
      PyErr_SetString(PyExc_AttributeError, "sequence must have at least one dimension");
      return NULL;
    }

    for (i = 0; i < shape_len; i++) {
      PyObject *ob = PySequence_GetItem(length_ob, i);
      if (!PyLong_Check(ob)) {
        PyErr_Format(PyExc_TypeError,
                     "invalid dimension %i, expected an int, not a %.200s",
                     i,
                     Py_TYPE(ob)->tp_name);
        Py_DECREF(ob);
        return NULL;
      }
      shape[i] = PyLong_AsLong(ob);
      Py_DECREF(ob);

      if (shape[i] < 1) {
        PyErr_SetString(PyExc_AttributeError, "dimension must be greater than or equal to 1");
        return NULL;
      }
    }
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "invalid second argument argument expected a sequence "
                 "or an int, not a %.200s",
                 Py_TYPE(length_ob)->tp_name);
    return NULL;
  }

  if (init && PyObject_CheckBuffer(init)) {
    Py_buffer pybuffer;

    if (PyObject_GetBuffer(init, &pybuffer, PyBUF_ND | PyBUF_FORMAT) == -1) {
      /* PyObject_GetBuffer raise a PyExc_BufferError */
      return NULL;
    }

    if (shape_len != pybuffer.ndim ||
        !pygpu_buffer_dimensions_compare(shape_len, shape, pybuffer.shape)) {
      PyErr_Format(PyExc_TypeError, "array size does not match");
    }
    else {
      buffer = pygpu_buffer_make_from_data(
          init, pygpu_dataformat.value_found, pybuffer.ndim, shape, pybuffer.buf);
    }

    PyBuffer_Release(&pybuffer);
  }
  else {
    buffer = BPyGPU_Buffer_CreatePyObject(pygpu_dataformat.value_found, shape, shape_len, NULL);
    if (init && pygpu_buffer_ass_slice(buffer, 0, shape[0], init)) {
      Py_DECREF(buffer);
      return NULL;
    }
  }

  return (PyObject *)buffer;
}

/* BPyGPUBuffer sequence methods */

static int pygpu_buffer__sq_length(BPyGPUBuffer *self)
{
  return self->shape[0];
}

static PyObject *pygpu_buffer_slice(BPyGPUBuffer *self, Py_ssize_t begin, Py_ssize_t end)
{
  PyObject *list;
  Py_ssize_t count;

  if (begin < 0) {
    begin = 0;
  }
  if (end > self->shape[0]) {
    end = self->shape[0];
  }
  if (begin > end) {
    begin = end;
  }

  list = PyList_New(end - begin);

  for (count = begin; count < end; count++) {
    PyList_SET_ITEM(list, count - begin, pygpu_buffer__sq_item(self, count));
  }
  return list;
}

static int pygpu_buffer__sq_ass_item(BPyGPUBuffer *self, int i, PyObject *v)
{
  if (i >= self->shape[0] || i < 0) {
    PyErr_SetString(PyExc_IndexError, "array assignment index out of range");
    return -1;
  }

  if (self->shape_len != 1) {
    BPyGPUBuffer *row = (BPyGPUBuffer *)pygpu_buffer__sq_item(self, i);

    if (row) {
      const int ret = pygpu_buffer_ass_slice(row, 0, self->shape[1], v);
      Py_DECREF(row);
      return ret;
    }

    return -1;
  }

  switch (self->format) {
    case GPU_DATA_FLOAT:
      return PyArg_Parse(v, "f:Expected floats", &self->buf.as_float[i]) ? 0 : -1;
    case GPU_DATA_INT:
      return PyArg_Parse(v, "i:Expected ints", &self->buf.as_int[i]) ? 0 : -1;
    case GPU_DATA_UBYTE:
      return PyArg_Parse(v, "b:Expected ints", &self->buf.as_byte[i]) ? 0 : -1;
    case GPU_DATA_UINT:
    case GPU_DATA_UINT_24_8:
    case GPU_DATA_10_11_11_REV:
      return PyArg_Parse(v, "b:Expected ints", &self->buf.as_uint[i]) ? 0 : -1;
    default:
      return 0; /* should never happen */
  }
}

static PyObject *pygpu_buffer__mp_subscript(BPyGPUBuffer *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return NULL;
    }
    if (i < 0) {
      i += self->shape[0];
    }
    return pygpu_buffer__sq_item(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->shape[0], &start, &stop, &step, &slicelength) < 0) {
      return NULL;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return pygpu_buffer_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with vectors");
    return NULL;
  }

  PyErr_Format(
      PyExc_TypeError, "buffer indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return NULL;
}

static int pygpu_buffer__mp_ass_subscript(BPyGPUBuffer *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += self->shape[0];
    }
    return pygpu_buffer__sq_ass_item(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->shape[0], &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return pygpu_buffer_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with vectors");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "buffer indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

static PyMethodDef pygpu_buffer__tp_methods[] = {
    {"to_list",
     (PyCFunction)pygpu_buffer_to_list_recursive,
     METH_NOARGS,
     "return the buffer as a list"},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef pygpu_buffer_getseters[] = {
    {"dimensions", (getter)pygpu_buffer_dimensions, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

static PySequenceMethods pygpu_buffer__tp_as_sequence = {
    (lenfunc)pygpu_buffer__sq_length,    /*sq_length */
    (binaryfunc)NULL,                    /*sq_concat */
    (ssizeargfunc)NULL,                  /*sq_repeat */
    (ssizeargfunc)pygpu_buffer__sq_item, /*sq_item */
    (ssizessizeargfunc)NULL, /*sq_slice, deprecated, handled in pygpu_buffer__sq_item */
    (ssizeobjargproc)pygpu_buffer__sq_ass_item, /*sq_ass_item */
    (ssizessizeobjargproc)NULL, /* sq_ass_slice, deprecated handled in pygpu_buffer__sq_ass_item */
    (objobjproc)NULL,           /* sq_contains */
    (binaryfunc)NULL,           /* sq_inplace_concat */
    (ssizeargfunc)NULL,         /* sq_inplace_repeat */
};

static PyMappingMethods pygpu_buffer__tp_as_mapping = {
    (lenfunc)pygpu_buffer__sq_length,
    (binaryfunc)pygpu_buffer__mp_subscript,
    (objobjargproc)pygpu_buffer__mp_ass_subscript,
};

#ifdef PYGPU_BUFFER_PROTOCOL
static void pygpu_buffer_strides_calc(const eGPUDataFormat format,
                                      const int shape_len,
                                      const Py_ssize_t *shape,
                                      Py_ssize_t *r_strides)
{
  r_strides[0] = GPU_texture_dataformat_size(format);
  for (int i = 1; i < shape_len; i++) {
    r_strides[i] = r_strides[i - 1] * shape[i - 1];
  }
}

/* Here is the buffer interface function */
static int pygpu_buffer__bf_getbuffer(BPyGPUBuffer *self, Py_buffer *view, int flags)
{
  if (view == NULL) {
    PyErr_SetString(PyExc_ValueError, "NULL view in getbuffer");
    return -1;
  }

  view->obj = (PyObject *)self;
  view->buf = (void *)self->buf.as_void;
  view->len = bpygpu_Buffer_size(self);
  view->readonly = 0;
  view->itemsize = GPU_texture_dataformat_size(self->format);
  view->format = pygpu_buffer_formatstr(self->format);
  view->ndim = self->shape_len;
  view->shape = self->shape;
  view->strides = MEM_mallocN(view->ndim * sizeof(*view->strides), "BPyGPUBuffer strides");
  pygpu_buffer_strides_calc(self->format, view->ndim, view->shape, view->strides);
  view->suboffsets = NULL;
  view->internal = NULL;

  Py_INCREF(self);
  return 0;
}

static void pygpu_buffer__bf_releasebuffer(PyObject *UNUSED(exporter), Py_buffer *view)
{
  MEM_SAFE_FREE(view->strides);
}

static PyBufferProcs pygpu_buffer__tp_as_buffer = {
    (getbufferproc)pygpu_buffer__bf_getbuffer,
    (releasebufferproc)pygpu_buffer__bf_releasebuffer,
};
#endif

PyDoc_STRVAR(
    pygpu_buffer__tp_doc,
    ".. class:: Buffer(format, dimensions, data)\n"
    "\n"
    "   For Python access to GPU functions requiring a pointer.\n"
    "\n"
    "   :arg format: Format type to interpret the buffer.\n"
    "      Possible values are `FLOAT`, `INT`, `UINT`, `UBYTE`, `UINT_24_8` and `10_11_11_REV`.\n"
    "   :type type: str\n"
    "   :arg dimensions: Array describing the dimensions.\n"
    "   :type dimensions: int\n"
    "   :arg data: Optional data array.\n"
    "   :type data: sequence\n");
PyTypeObject BPyGPU_BufferType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "Buffer",
    .tp_basicsize = sizeof(BPyGPUBuffer),
    .tp_dealloc = (destructor)pygpu_buffer__tp_dealloc,
    .tp_repr = (reprfunc)pygpu_buffer__tp_repr,
    .tp_as_sequence = &pygpu_buffer__tp_as_sequence,
    .tp_as_mapping = &pygpu_buffer__tp_as_mapping,
#ifdef PYGPU_BUFFER_PROTOCOL
    .tp_as_buffer = &pygpu_buffer__tp_as_buffer,
#endif
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = pygpu_buffer__tp_doc,
    .tp_traverse = (traverseproc)pygpu_buffer__tp_traverse,
    .tp_clear = (inquiry)pygpu_buffer__tp_clear,
    .tp_methods = pygpu_buffer__tp_methods,
    .tp_getset = pygpu_buffer_getseters,
    .tp_new = pygpu_buffer__tp_new,
};

static size_t pygpu_buffer_calc_size(const int format,
                                     const int shape_len,
                                     const Py_ssize_t *shape)
{
  size_t r_size = GPU_texture_dataformat_size(format);

  for (int i = 0; i < shape_len; i++) {
    r_size *= shape[i];
  }

  return r_size;
}

size_t bpygpu_Buffer_size(BPyGPUBuffer *buffer)
{
  return pygpu_buffer_calc_size(buffer->format, buffer->shape_len, buffer->shape);
}

/**
 * Create a buffer object
 *
 * \param shape: An array of `shape_len` integers representing the size of each dimension.
 * \param buffer: When not NULL holds a contiguous buffer
 * with the correct format from which the buffer will be initialized
 */
BPyGPUBuffer *BPyGPU_Buffer_CreatePyObject(const int format,
                                           const Py_ssize_t *shape,
                                           const int shape_len,
                                           void *buffer)
{
  if (buffer == NULL) {
    size_t size = pygpu_buffer_calc_size(format, shape_len, shape);
    buffer = MEM_callocN(size, "BPyGPUBuffer buffer");
  }

  return pygpu_buffer_make_from_data(NULL, format, shape_len, shape, buffer);
}

/** \} */
