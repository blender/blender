/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the uniform buffer functionalities of the 'gpu' module
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_string.h"

#include "GPU_context.h"
#include "GPU_uniform_buffer.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h"

#include "gpu_py_uniformbuffer.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPUUniformBuf Common Utilities
 * \{ */

static int pygpu_uniformbuffer_valid_check(BPyGPUUniformBuf *bpygpu_ub)
{
  if (UNLIKELY(bpygpu_ub->ubo == nullptr)) {
    PyErr_SetString(PyExc_ReferenceError,
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
                    "GPU uniform buffer was freed, no further access is valid");
#else

                    "GPU uniform buffer: internal error");
#endif
    return -1;
  }
  return 0;
}

#define BPYGPU_UNIFORMBUF_CHECK_OBJ(bpygpu) \
  { \
    if (UNLIKELY(pygpu_uniformbuffer_valid_check(bpygpu) == -1)) { \
      return nullptr; \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUUniformBuf Type
 * \{ */

static PyObject *pygpu_uniformbuffer__tp_new(PyTypeObject * /*self*/,
                                             PyObject *args,
                                             PyObject *kwds)
{
  GPUUniformBuf *ubo = nullptr;
  PyObject *pybuffer_obj;
  char err_out[256] = "unknown error. See console";

  static const char *_keywords[] = {"data", nullptr};
  static _PyArg_Parser _parser = {
      "O" /* `data` */
      ":GPUUniformBuf.__new__",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &pybuffer_obj)) {
    return nullptr;
  }

  if (!GPU_context_active_get()) {
    STRNCPY(err_out, "No active GPU context found");
  }
  else {
    Py_buffer pybuffer;
    if (PyObject_GetBuffer(pybuffer_obj, &pybuffer, PyBUF_SIMPLE) == -1) {
      /* PyObject_GetBuffer raise a PyExc_BufferError */
      return nullptr;
    }

    if ((pybuffer.len % 16) != 0) {
      STRNCPY(err_out, "UBO is not padded to size of vec4");
    }
    else {
      ubo = GPU_uniformbuf_create_ex(pybuffer.len, pybuffer.buf, "python_uniformbuffer");
    }
    PyBuffer_Release(&pybuffer);
  }

  if (ubo == nullptr) {
    PyErr_Format(PyExc_RuntimeError, "GPUUniformBuf.__new__(...) failed with '%s'", err_out);
    return nullptr;
  }

  return BPyGPUUniformBuf_CreatePyObject(ubo);
}

PyDoc_STRVAR(pygpu_uniformbuffer_update_doc,
             ".. method:: update(data)\n"
             "\n"
             "   Update the data of the uniform buffer object.\n");
static PyObject *pygpu_uniformbuffer_update(BPyGPUUniformBuf *self, PyObject *obj)
{
  BPYGPU_UNIFORMBUF_CHECK_OBJ(self);

  Py_buffer pybuffer;
  if (PyObject_GetBuffer(obj, &pybuffer, PyBUF_SIMPLE) == -1) {
    /* PyObject_GetBuffer raise a PyExc_BufferError */
    return nullptr;
  }

  GPU_uniformbuf_update(self->ubo, pybuffer.buf);
  PyBuffer_Release(&pybuffer);
  Py_RETURN_NONE;
}

#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
PyDoc_STRVAR(pygpu_uniformbuffer_free_doc,
             ".. method::free()\n"
             "\n"
             "   Free the uniform buffer object.\n"
             "   The uniform buffer object will no longer be accessible.\n");
static PyObject *pygpu_uniformbuffer_free(BPyGPUUniformBuf *self)
{
  BPYGPU_UNIFORMBUF_CHECK_OBJ(self);

  GPU_uniformbuf_free(self->ubo);
  self->ubo = nullptr;
  Py_RETURN_NONE;
}
#endif

static void BPyGPUUniformBuf__tp_dealloc(BPyGPUUniformBuf *self)
{
  if (self->ubo) {
    GPU_uniformbuf_free(self->ubo);
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef pygpu_uniformbuffer__tp_getseters[] = {
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyMethodDef pygpu_uniformbuffer__tp_methods[] = {
    {"update", (PyCFunction)pygpu_uniformbuffer_update, METH_O, pygpu_uniformbuffer_update_doc},
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
    {"free", (PyCFunction)pygpu_uniformbuffer_free, METH_NOARGS, pygpu_uniformbuffer_free_doc},
#endif
    {nullptr, nullptr, 0, nullptr},
};

PyDoc_STRVAR(pygpu_uniformbuffer__tp_doc,
             ".. class:: GPUUniformBuf(data)\n"
             "\n"
             "   This object gives access to off uniform buffers.\n"
             "\n"
             "   :arg data: Data to fill the buffer.\n"
             "   :type data: object exposing buffer interface\n");
PyTypeObject BPyGPUUniformBuf_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUUniformBuf",
    /*tp_basicsize*/ sizeof(BPyGPUUniformBuf),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BPyGPUUniformBuf__tp_dealloc,
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
    /*tp_doc*/ pygpu_uniformbuffer__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_uniformbuffer__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pygpu_uniformbuffer__tp_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_uniformbuffer__tp_new,
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

PyObject *BPyGPUUniformBuf_CreatePyObject(GPUUniformBuf *ubo)
{
  BPyGPUUniformBuf *self;

  self = PyObject_New(BPyGPUUniformBuf, &BPyGPUUniformBuf_Type);
  self->ubo = ubo;

  return (PyObject *)self;
}

/** \} */

#undef BPYGPU_UNIFORMBUF_CHECK_OBJ
