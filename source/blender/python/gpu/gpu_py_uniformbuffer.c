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
 * This file defines the uniform buffer functionalities of the 'gpu' module
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_string.h"

#include "GPU_context.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h"
#include "gpu_py_api.h"
#include "gpu_py_buffer.h"

#include "gpu_py_uniformbuffer.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPUUniformBuf Common Utilities
 * \{ */

static int pygpu_uniformbuffer_valid_check(BPyGPUUniformBuf *bpygpu_ub)
{
  if (UNLIKELY(bpygpu_ub->ubo == NULL)) {
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
      return NULL; \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUUniformBuf Type
 * \{ */

static PyObject *pygpu_uniformbuffer__tp_new(PyTypeObject *UNUSED(self),
                                             PyObject *args,
                                             PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  GPUUniformBuf *ubo = NULL;
  BPyGPUBuffer *pybuffer_obj;
  char err_out[256] = "unknown error. See console";

  static const char *_keywords[] = {"data", NULL};
  static _PyArg_Parser _parser = {"O!:GPUUniformBuf.__new__", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &BPyGPU_BufferType, &pybuffer_obj)) {
    return NULL;
  }

  if (GPU_context_active_get()) {
    ubo = GPU_uniformbuf_create_ex(
        bpygpu_Buffer_size(pybuffer_obj), pybuffer_obj->buf.as_void, "python_uniformbuffer");
  }
  else {
    STRNCPY(err_out, "No active GPU context found");
  }

  if (ubo == NULL) {
    PyErr_Format(PyExc_RuntimeError, "GPUUniformBuf.__new__(...) failed with '%s'", err_out);
    return NULL;
  }

  return BPyGPUUniformBuf_CreatePyObject(ubo);
}

PyDoc_STRVAR(pygpu_uniformbuffer_update_doc,
             ".. method::update(data)\n"
             "\n"
             "   Update the data of the uniform buffer object.\n");
static PyObject *pygpu_uniformbuffer_update(BPyGPUUniformBuf *self, PyObject *obj)
{
  BPYGPU_UNIFORMBUF_CHECK_OBJ(self);

  if (!BPyGPU_Buffer_Check(obj)) {
    return NULL;
  }

  GPU_uniformbuf_update(self->ubo, ((BPyGPUBuffer *)obj)->buf.as_void);
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
  self->ubo = NULL;
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
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef pygpu_uniformbuffer__tp_methods[] = {
    {"update", (PyCFunction)pygpu_uniformbuffer_update, METH_O, pygpu_uniformbuffer_update_doc},
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
    {"free", (PyCFunction)pygpu_uniformbuffer_free, METH_NOARGS, pygpu_uniformbuffer_free_doc},
#endif
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(pygpu_uniformbuffer__tp_doc,
             ".. class:: GPUUniformBuf(data)\n"
             "\n"
             "   This object gives access to off uniform buffers.\n"
             "\n"
             "   :arg data: Buffer object.\n"
             "   :type data: `Buffer`\n");
PyTypeObject BPyGPUUniformBuf_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUUniformBuf",
    .tp_basicsize = sizeof(BPyGPUUniformBuf),
    .tp_dealloc = (destructor)BPyGPUUniformBuf__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = pygpu_uniformbuffer__tp_doc,
    .tp_methods = pygpu_uniformbuffer__tp_methods,
    .tp_getset = pygpu_uniformbuffer__tp_getseters,
    .tp_new = pygpu_uniformbuffer__tp_new,
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
