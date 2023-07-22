/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the gpu.state API.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "GPU_texture.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h"

#include "gpu_py_buffer.h"

#define PYGPU_BUFFER_PROTOCOL
#define MAX_DIMENSIONS 64

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static Py_ssize_t pygpu_buffer_dimensions_tot_elem(const Py_ssize_t *shape, Py_ssize_t shape_len)
{
  Py_ssize_t tot = shape[0];
  for (int i = 1; i < shape_len; i++) {
    tot *= shape[i];
  }

  return tot;
}

static bool pygpu_buffer_dimensions_tot_len_compare(const Py_ssize_t *shape_a,
                                                    const Py_ssize_t shape_a_len,
                                                    const Py_ssize_t *shape_b,
                                                    const Py_ssize_t shape_b_len)
{
  if (pygpu_buffer_dimensions_tot_elem(shape_a, shape_a_len) !=
      pygpu_buffer_dimensions_tot_elem(shape_b, shape_b_len))
  {
    PyErr_Format(PyExc_BufferError, "array size does not match");
    return false;
  }

  return true;
}

static bool pygpu_buffer_pyobj_as_shape(PyObject *shape_obj,
                                        Py_ssize_t r_shape[MAX_DIMENSIONS],
                                        Py_ssize_t *r_shape_len)
{
  Py_ssize_t shape_len = 0;
  if (PyLong_Check(shape_obj)) {
    shape_len = 1;
    if ((r_shape[0] = PyLong_AsLong(shape_obj)) < 1) {
      PyErr_SetString(PyExc_AttributeError, "dimension must be greater than or equal to 1");
      return false;
    }
  }
  else if (PySequence_Check(shape_obj)) {
    shape_len = PySequence_Size(shape_obj);
    if (shape_len > MAX_DIMENSIONS) {
      PyErr_SetString(PyExc_AttributeError,
                      "too many dimensions, max is " STRINGIFY(MAX_DIMENSIONS));
      return false;
    }
    if (shape_len < 1) {
      PyErr_SetString(PyExc_AttributeError, "sequence must have at least one dimension");
      return false;
    }

    for (int i = 0; i < shape_len; i++) {
      PyObject *ob = PySequence_GetItem(shape_obj, i);
      if (!PyLong_Check(ob)) {
        PyErr_Format(PyExc_TypeError,
                     "invalid dimension %i, expected an int, not a %.200s",
                     i,
                     Py_TYPE(ob)->tp_name);
        Py_DECREF(ob);
        return false;
      }

      r_shape[i] = PyLong_AsLong(ob);
      Py_DECREF(ob);

      if (r_shape[i] < 1) {
        PyErr_SetString(PyExc_AttributeError, "dimension must be greater than or equal to 1");
        return false;
      }
    }
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "invalid second argument expected a sequence "
                 "or an int, not a %.200s",
                 Py_TYPE(shape_obj)->tp_name);
  }

  *r_shape_len = shape_len;
  return true;
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
  return nullptr;
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

  buffer->parent = nullptr;
  buffer->format = format;
  buffer->shape_len = shape_len;
  buffer->shape = static_cast<Py_ssize_t *>(
      MEM_mallocN(shape_len * sizeof(*buffer->shape), "BPyGPUBuffer shape"));
  memcpy(buffer->shape, shape, shape_len * sizeof(*buffer->shape));
  buffer->buf.as_void = buf;

  if (parent) {
    Py_INCREF(parent);
    buffer->parent = parent;
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)buffer));
    PyObject_GC_Track(buffer);
  }
  return buffer;
}

static PyObject *pygpu_buffer__sq_item(BPyGPUBuffer *self, Py_ssize_t i)
{
  if (i >= self->shape[0] || i < 0) {
    PyErr_SetString(PyExc_IndexError, "array index out of range");
    return nullptr;
  }

  const char *formatstr = pygpu_buffer_formatstr(eGPUDataFormat(self->format));

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
    int offset = i * GPU_texture_dataformat_size(eGPUDataFormat(self->format));
    for (int j = 1; j < self->shape_len; j++) {
      offset *= self->shape[j];
    }

    return (PyObject *)pygpu_buffer_make_from_data((PyObject *)self,
                                                   eGPUDataFormat(self->format),
                                                   self->shape_len - 1,
                                                   self->shape + 1,
                                                   self->buf.as_byte + offset);
  }

  return nullptr;
}

static PyObject *pygpu_buffer_to_list(BPyGPUBuffer *self)
{
  const Py_ssize_t len = self->shape[0];
  PyObject *list = PyList_New(len);

  for (Py_ssize_t i = 0; i < len; i++) {
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

static PyObject *pygpu_buffer_dimensions_get(BPyGPUBuffer *self, void * /*arg*/)
{
  PyObject *list = PyList_New(self->shape_len);
  int i;

  for (i = 0; i < self->shape_len; i++) {
    PyList_SET_ITEM(list, i, PyLong_FromLong(self->shape[i]));
  }

  return list;
}

static int pygpu_buffer_dimensions_set(BPyGPUBuffer *self, PyObject *value, void * /*type*/)
{
  Py_ssize_t shape[MAX_DIMENSIONS];
  Py_ssize_t shape_len = 0;

  if (!pygpu_buffer_pyobj_as_shape(value, shape, &shape_len)) {
    return -1;
  }

  if (!pygpu_buffer_dimensions_tot_len_compare(shape, shape_len, self->shape, self->shape_len)) {
    return -1;
  }

  size_t size = shape_len * sizeof(*self->shape);
  if (shape_len != self->shape_len) {
    MEM_freeN(self->shape);
    self->shape = static_cast<Py_ssize_t *>(MEM_mallocN(size, __func__));
  }

  self->shape_len = shape_len;
  memcpy(self->shape, shape, size);
  return 0;
}

static int pygpu_buffer__tp_traverse(BPyGPUBuffer *self, visitproc visit, void *arg)
{
  Py_VISIT(self->parent);
  return 0;
}

static int pygpu_buffer__tp_clear(BPyGPUBuffer *self)
{
  if (self->parent) {
    Py_CLEAR(self->parent);
    self->buf.as_void = nullptr;
  }
  return 0;
}

static void pygpu_buffer__tp_dealloc(BPyGPUBuffer *self)
{
  if (self->parent) {
    PyObject_GC_UnTrack(self);
    Py_CLEAR(self->parent);
  }
  else if (self->buf.as_void) {
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

static int pygpu_buffer__sq_ass_item(BPyGPUBuffer *self, Py_ssize_t i, PyObject *v);

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

static PyObject *pygpu_buffer__tp_new(PyTypeObject * /*type*/, PyObject *args, PyObject *kwds)
{
  PyObject *length_ob, *init = nullptr;
  BPyGPUBuffer *buffer = nullptr;
  Py_ssize_t shape[MAX_DIMENSIONS];

  Py_ssize_t shape_len = 0;

  if (kwds && PyDict_Size(kwds)) {
    PyErr_SetString(PyExc_TypeError, "Buffer(): takes no keyword args");
    return nullptr;
  }

  const struct PyC_StringEnum pygpu_dataformat = {bpygpu_dataformat_items, GPU_DATA_FLOAT};
  if (!PyArg_ParseTuple(
          args, "O&O|O: Buffer", PyC_ParseStringEnum, &pygpu_dataformat, &length_ob, &init))
  {
    return nullptr;
  }

  if (!pygpu_buffer_pyobj_as_shape(length_ob, shape, &shape_len)) {
    return nullptr;
  }

  if (init && PyObject_CheckBuffer(init)) {
    Py_buffer pybuffer;

    if (PyObject_GetBuffer(init, &pybuffer, PyBUF_ND | PyBUF_FORMAT) == -1) {
      /* PyObject_GetBuffer raise a PyExc_BufferError */
      return nullptr;
    }

    Py_ssize_t *pybuffer_shape = pybuffer.shape;
    Py_ssize_t pybuffer_ndim = pybuffer.ndim;
    if (!pybuffer_shape) {
      pybuffer_shape = &pybuffer.len;
      pybuffer_ndim = 1;
    }

    if (pygpu_buffer_dimensions_tot_len_compare(shape, shape_len, pybuffer_shape, pybuffer_ndim)) {
      buffer = pygpu_buffer_make_from_data(
          init, eGPUDataFormat(pygpu_dataformat.value_found), shape_len, shape, pybuffer.buf);
    }

    PyBuffer_Release(&pybuffer);
  }
  else {
    buffer = BPyGPU_Buffer_CreatePyObject(pygpu_dataformat.value_found, shape, shape_len, nullptr);
    if (init && pygpu_buffer_ass_slice(buffer, 0, shape[0], init)) {
      Py_DECREF(buffer);
      return nullptr;
    }
  }

  return (PyObject *)buffer;
}

static int pygpu_buffer__tp_is_gc(BPyGPUBuffer *self)
{
  return self->parent != nullptr;
}

/* BPyGPUBuffer sequence methods */

static Py_ssize_t pygpu_buffer__sq_length(BPyGPUBuffer *self)
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

static int pygpu_buffer__sq_ass_item(BPyGPUBuffer *self, Py_ssize_t i, PyObject *v)
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
      return PyArg_Parse(v, "I:Expected unsigned ints", &self->buf.as_uint[i]) ? 0 : -1;
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
      return nullptr;
    }
    if (i < 0) {
      i += self->shape[0];
    }
    return pygpu_buffer__sq_item(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->shape[0], &start, &stop, &step, &slicelength) < 0) {
      return nullptr;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return pygpu_buffer_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with vectors");
    return nullptr;
  }

  PyErr_Format(
      PyExc_TypeError, "buffer indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return nullptr;
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

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef pygpu_buffer__tp_methods[] = {
    {"to_list",
     (PyCFunction)pygpu_buffer_to_list_recursive,
     METH_NOARGS,
     "return the buffer as a list"},
    {nullptr, nullptr, 0, nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

static PyGetSetDef pygpu_buffer_getseters[] = {
    {"dimensions",
     (getter)pygpu_buffer_dimensions_get,
     (setter)pygpu_buffer_dimensions_set,
     nullptr,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

static PySequenceMethods pygpu_buffer__tp_as_sequence = {
    /*sq_length*/ (lenfunc)pygpu_buffer__sq_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)pygpu_buffer__sq_item,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. Handled by #pygpu_buffer__sq_item. */
    /*sq_ass_item*/ (ssizeobjargproc)pygpu_buffer__sq_ass_item,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. Handled by #pygpu_buffer__sq_ass_item. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods pygpu_buffer__tp_as_mapping = {
    /*mp_length*/ (lenfunc)pygpu_buffer__sq_length,
    /*mp_subscript*/ (binaryfunc)pygpu_buffer__mp_subscript,
    /*mp_ass_subscript*/ (objobjargproc)pygpu_buffer__mp_ass_subscript,
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
  if (view == nullptr) {
    PyErr_SetString(PyExc_ValueError, "nullptr view in getbuffer");
    return -1;
  }

  memset(view, 0, sizeof(*view));

  view->obj = (PyObject *)self;
  view->buf = (void *)self->buf.as_void;
  view->len = bpygpu_Buffer_size(self);
  view->readonly = 0;
  view->itemsize = GPU_texture_dataformat_size(eGPUDataFormat(self->format));
  if (flags & PyBUF_FORMAT) {
    view->format = (char *)pygpu_buffer_formatstr(eGPUDataFormat(self->format));
  }
  if (flags & PyBUF_ND) {
    view->ndim = self->shape_len;
    view->shape = self->shape;
  }
  if (flags & PyBUF_STRIDES) {
    view->strides = static_cast<Py_ssize_t *>(
        MEM_mallocN(view->ndim * sizeof(*view->strides), "BPyGPUBuffer strides"));
    pygpu_buffer_strides_calc(
        eGPUDataFormat(self->format), view->ndim, view->shape, view->strides);
  }
  view->suboffsets = nullptr;
  view->internal = nullptr;

  Py_INCREF(self);
  return 0;
}

static void pygpu_buffer__bf_releasebuffer(PyObject * /*exporter*/, Py_buffer *view)
{
  MEM_SAFE_FREE(view->strides);
}

static PyBufferProcs pygpu_buffer__tp_as_buffer = {
    /*bf_getbuffer*/ (getbufferproc)pygpu_buffer__bf_getbuffer,
    /*bf_releasebuffer*/ (releasebufferproc)pygpu_buffer__bf_releasebuffer,
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
    "   :type format: str\n"
    "   :arg dimensions: Array describing the dimensions.\n"
    "   :type dimensions: int\n"
    "   :arg data: Optional data array.\n"
    "   :type data: sequence\n");
PyTypeObject BPyGPU_BufferType = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Buffer",
    /*tp_basicsize*/ sizeof(BPyGPUBuffer),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_buffer__tp_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_compare*/ nullptr,
    /*tp_repr*/ (reprfunc)pygpu_buffer__tp_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &pygpu_buffer__tp_as_sequence,
    /*tp_as_mapping*/ &pygpu_buffer__tp_as_mapping,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
#ifdef PYGPU_BUFFER_PROTOCOL
    /*tp_as_buffer*/ &pygpu_buffer__tp_as_buffer,
#else
    /*tp_as_buffer*/ nullptr,
#endif
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ pygpu_buffer__tp_doc,
    /*tp_traverse*/ (traverseproc)pygpu_buffer__tp_traverse,
    /*tp_clear*/ (inquiry)pygpu_buffer__tp_clear,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_buffer__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pygpu_buffer_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_buffer__tp_new,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ (inquiry)pygpu_buffer__tp_is_gc,
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

static size_t pygpu_buffer_calc_size(const int format,
                                     const int shape_len,
                                     const Py_ssize_t *shape)
{
  return pygpu_buffer_dimensions_tot_elem(shape, shape_len) *
         GPU_texture_dataformat_size(eGPUDataFormat(format));
}

size_t bpygpu_Buffer_size(BPyGPUBuffer *buffer)
{
  return pygpu_buffer_calc_size(buffer->format, buffer->shape_len, buffer->shape);
}

BPyGPUBuffer *BPyGPU_Buffer_CreatePyObject(const int format,
                                           const Py_ssize_t *shape,
                                           const int shape_len,
                                           void *buffer)
{
  if (buffer == nullptr) {
    size_t size = pygpu_buffer_calc_size(format, shape_len, shape);
    buffer = MEM_callocN(size, "BPyGPUBuffer buffer");
  }

  return pygpu_buffer_make_from_data(nullptr, eGPUDataFormat(format), shape_len, shape, buffer);
}

/** \} */
