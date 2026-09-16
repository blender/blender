/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the storage buffer functionalities of the 'gpu' module
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_string_utf8.hh"

#include "MEM_guardedalloc.h"

#include "GPU_context.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_texture.hh"

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "gpu_py.hh"
#include "gpu_py_buffer.hh"
#include "gpu_py_storagebuffer.hh" /* own include */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name gpu::StorageBuf Common Utilities
 * \{ */

static int pygpu_storagebuffer_valid_check(BPyGPUStorageBuf *bpygpu_ssbo)
{
  if (bpygpu_ssbo->ssbo == nullptr) [[unlikely]] {
    PyErr_SetString(PyExc_ReferenceError,
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
                    "GPU storage buffer was freed, no further access is valid");
#else

                    "GPU storage buffer: internal error");
#endif
    return -1;
  }
  return 0;
}

#define BPYGPU_STORAGEBUF_CHECK_OBJ(bpygpu) \
  { \
    if (pygpu_storagebuffer_valid_check(bpygpu) == -1) [[unlikely]] { \
      return nullptr; \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name gpu::StorageBuf Type
 * \{ */

static PyObject *pygpu_storagebuffer__tp_new(PyTypeObject * /*self*/,
                                             PyObject *args,
                                             PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  gpu::StorageBuf *ssbo = nullptr;
  PyObject *pybuffer_obj;
  Py_ssize_t size = 0;
  char err_out[256] = "unknown error. See console";

  static const char *_keywords[] = {"data", nullptr};
  static _PyArg_Parser _parser = {
      "O" /* `data` */
      ":GPUStorageBuf.__new__",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &pybuffer_obj)) {
    return nullptr;
  }

  if (!GPU_context_active_get()) {
    STRNCPY_UTF8(err_out, "No active GPU context found");
  }
  else {
    Py_buffer pybuffer;
    if (PyObject_GetBuffer(pybuffer_obj, &pybuffer, PyBUF_SIMPLE) == -1) {
      /* PyObject_GetBuffer raise a PyExc_BufferError */
      return nullptr;
    }

    size = pybuffer.len;
    ssbo = GPU_storagebuf_create_ex(
        pybuffer.len, pybuffer.buf, GPU_USAGE_DYNAMIC, "python_storagebuffer");
    PyBuffer_Release(&pybuffer);
  }

  if (ssbo == nullptr) {
    PyErr_Format(PyExc_RuntimeError, "GPUStorageBuf.__new__(...) failed with '%s'", err_out);
    return nullptr;
  }

  return BPyGPUStorageBuf_CreatePyObject(ssbo, size_t(size));
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_storagebuffer_update_doc,
    ".. method:: update(data)\n"
    "\n"
    "   Update the data of the storage buffer object.\n"
    "\n"
    "   :param data: Data to fill the buffer.\n"
    "   :type data: Buffer\n");
static PyObject *pygpu_storagebuffer_update(BPyGPUStorageBuf *self, PyObject *obj)
{
  BPYGPU_STORAGEBUF_CHECK_OBJ(self);

  Py_buffer pybuffer;
  if (PyObject_GetBuffer(obj, &pybuffer, PyBUF_SIMPLE) == -1) {
    /* PyObject_GetBuffer raise a PyExc_BufferError */
    return nullptr;
  }

  /* The backends copy exactly the size the buffer was created with, regardless of how much
   * data is actually passed in. Providing less would read past the end of `pybuffer`. */
  if (size_t(pybuffer.len) != self->size) {
    PyErr_Format(PyExc_ValueError,
                 "GPUStorageBuf.update(): expected a buffer of size %zu, got %zu",
                 self->size,
                 size_t(pybuffer.len));
    PyBuffer_Release(&pybuffer);
    return nullptr;
  }

  GPU_storagebuf_update(self->ssbo, pybuffer.buf);
  PyBuffer_Release(&pybuffer);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_storagebuffer_clear_to_zero_doc,
    ".. method:: clear_to_zero()\n"
    "\n"
    "   Clear the storage buffer data to zero.\n");
static PyObject *pygpu_storagebuffer_clear_to_zero(BPyGPUStorageBuf *self)
{
  BPYGPU_STORAGEBUF_CHECK_OBJ(self);

  GPU_storagebuf_clear_to_zero(self->ssbo);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_storagebuffer_read_doc,
    ".. method:: read()\n"
    "\n"
    "   Read back the contents of the storage buffer.\n"
    "   This waits until all GPU operations are finished, performing the necessary "
    "synchronization.\n"
    "\n"
    "   :return: The Buffer with the read data.\n"
    "   :rtype: :class:`gpu.types.Buffer`\n");
static PyObject *pygpu_storagebuffer_read(BPyGPUStorageBuf *self)
{
  BPYGPU_STORAGEBUF_CHECK_OBJ(self);

  void *buf = MEM_new_uninitialized(self->size, "python_storagebuffer_read");
  GPU_storagebuf_read(self->ssbo, buf);

  const Py_ssize_t shape = Py_ssize_t(self->size);
  return reinterpret_cast<PyObject *>(
      BPyGPU_Buffer_CreatePyObject(GPU_DATA_UBYTE, &shape, 1, buf));
}

#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
PyDoc_STRVAR(
    /* Wrap. */
    pygpu_storagebuffer_free_doc,
    ".. method:: free()\n"
    "\n"
    "   Free the storage buffer object.\n"
    "   The storage buffer object will no longer be accessible.\n");
static PyObject *pygpu_storagebuffer_free(BPyGPUStorageBuf *self)
{
  BPYGPU_STORAGEBUF_CHECK_OBJ(self);

  GPU_storagebuf_free(self->ssbo);
  self->ssbo = nullptr;
  Py_RETURN_NONE;
}
#endif

static void BPyGPUStorageBuf__tp_dealloc(BPyGPUStorageBuf *self)
{
  if (self->ssbo) {
    GPU_storagebuf_free(self->ssbo);
  }
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject *>(self));
}

static PyGetSetDef pygpu_storagebuffer__tp_getseters[] = {
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyMethodDef pygpu_storagebuffer__tp_methods[] = {
    {"update",
     reinterpret_cast<PyCFunction>(pygpu_storagebuffer_update),
     METH_O,
     pygpu_storagebuffer_update_doc},
    {"clear_to_zero",
     reinterpret_cast<PyCFunction>(pygpu_storagebuffer_clear_to_zero),
     METH_NOARGS,
     pygpu_storagebuffer_clear_to_zero_doc},
    {"read",
     reinterpret_cast<PyCFunction>(pygpu_storagebuffer_read),
     METH_NOARGS,
     pygpu_storagebuffer_read_doc},
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
    {"free", (PyCFunction)pygpu_storagebuffer_free, METH_NOARGS, pygpu_storagebuffer_free_doc},
#endif
    {nullptr, nullptr, 0, nullptr},
};

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_storagebuffer__tp_doc,
    ".. class:: GPUStorageBuf\n"
    "\n"
    "   This object gives access to storage buffers.\n"
    "\n"
    "   .. method:: __init__(data)\n"
    "\n"
    "      :param data: Data to fill the buffer.\n"
    "      :type data: Buffer\n");
PyTypeObject BPyGPUStorageBuf_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUStorageBuf",
    /*tp_basicsize*/ sizeof(BPyGPUStorageBuf),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(BPyGPUStorageBuf__tp_dealloc),
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
    /*tp_doc*/ pygpu_storagebuffer__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_storagebuffer__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pygpu_storagebuffer__tp_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_storagebuffer__tp_new,
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

PyObject *BPyGPUStorageBuf_CreatePyObject(gpu::StorageBuf *ssbo, size_t size)
{
  BPyGPUStorageBuf *self;

  self = PyObject_New(BPyGPUStorageBuf, &BPyGPUStorageBuf_Type);
  self->ssbo = ssbo;
  self->size = size;

  return reinterpret_cast<PyObject *>(self);
}

/** \} */

#undef BPYGPU_STORAGEBUF_CHECK_OBJ

}  // namespace blender
