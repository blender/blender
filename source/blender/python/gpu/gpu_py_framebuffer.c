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
 * This file defines the framebuffer functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_init_exit.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"
#include "../mathutils/mathutils.h"

#include "gpu_py_api.h"
#include "gpu_py_texture.h"

#include "gpu_py_framebuffer.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPUFrameBuffer Common Utilities
 * \{ */

static int py_framebuffer_valid_check(BPyGPUFrameBuffer *bpygpu_fb)
{
  if (UNLIKELY(bpygpu_fb->fb == NULL)) {
    PyErr_SetString(PyExc_ReferenceError, "GPU framebuffer was freed, no further access is valid");
    return -1;
  }
  return 0;
}

#define PY_FRAMEBUFFER_CHECK_OBJ(bpygpu) \
  { \
    if (UNLIKELY(py_framebuffer_valid_check(bpygpu) == -1)) { \
      return NULL; \
    } \
  } \
  ((void)0)

static void py_framebuffer_free_if_possible(GPUFrameBuffer *fb)
{
  if (!fb) {
    return;
  }

  if (GPU_is_init()) {
    GPU_framebuffer_free(fb);
  }
  else {
    printf("PyFramebuffer freed after the context has been destroyed.\n");
  }
}

/* Keep less than or equal to #FRAMEBUFFER_STACK_DEPTH */
#define GPU_PY_FRAMEBUFFER_STACK_LEN 16

static bool py_framebuffer_stack_push_or_error(GPUFrameBuffer *fb)
{
  if (GPU_framebuffer_stack_level_get() >= GPU_PY_FRAMEBUFFER_STACK_LEN) {
    PyErr_SetString(
        PyExc_RuntimeError,
        "Maximum framebuffer stack depth " STRINGIFY(GPU_PY_FRAMEBUFFER_STACK_LEN) " reached");
    return false;
  }
  GPU_framebuffer_push(fb);
  GPU_framebuffer_bind(fb);
  return true;
}

static bool py_framebuffer_stack_pop_or_error(void)
{
  if (GPU_framebuffer_stack_level_get() == 0) {
    PyErr_SetString(PyExc_RuntimeError, "Minimum framebuffer stack depth reached");
    return false;
  }

  GPUFrameBuffer *fb = GPU_framebuffer_pop();
  GPU_framebuffer_bind(fb);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUFramebuffer Type
 * \{ */

static PyObject *py_framebuffer_new(PyTypeObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;
  if (PyTuple_GET_SIZE(args) || (kwds && PyDict_Size(kwds))) {
    PyErr_SetString(PyExc_ValueError, "This function takes no arguments");
    return NULL;
  }

  if (!GPU_context_active_get()) {
    PyErr_SetString(PyExc_RuntimeError, "No active GPU context found");
    return NULL;
  }

  GPUFrameBuffer *fb = GPU_framebuffer_create("python_fb");
  return BPyGPUFrameBuffer_CreatePyObject(fb);
}

static PyObject *py_framebuffer_viewport_get(BPyGPUFrameBuffer *self, void *UNUSED(type))
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);
  int viewport[4];
  GPU_framebuffer_viewport_get(self->fb, viewport);

  PyObject *ret = PyTuple_New(4);
  PyTuple_SET_ITEMS(ret,
                    PyLong_FromLong(viewport[0]),
                    PyLong_FromLong(viewport[1]),
                    PyLong_FromLong(viewport[2]),
                    PyLong_FromLong(viewport[3]));
  return ret;
}

static int py_framebuffer_viewport_set(BPyGPUFrameBuffer *self,
                                       PyObject *py_values,
                                       void *UNUSED(type))
{
  int viewport[4];

  if (!PySequence_Check(py_values)) {
    return -1;
  }

  if (PySequence_Size(py_values) != 4) {
    PyErr_SetString(PyExc_AttributeError, "An array of length 4 is required");
    return -1;
  }

  for (int i = 0; i < 4; i++) {
    PyObject *ob = PySequence_GetItem(py_values, i);
    viewport[i] = PyLong_AsLong(ob);
    Py_DECREF(ob);
    if (PyErr_Occurred()) {
      return -1;
    }
  }

  GPU_framebuffer_viewport_set(self->fb, UNPACK4(viewport));
  return 0;
}

PyDoc_STRVAR(py_framebuffer_is_bound_doc,
             ".. method:: is_bound()\n"
             "\n"
             "   Checks if this is the active framebuffer in the context.\n"
             "\n");
static PyObject *py_framebuffer_is_bound(BPyGPUFrameBuffer *self, void *UNUSED(type))
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);
  return PyBool_FromLong(GPU_framebuffer_bound(self->fb));
}

PyDoc_STRVAR(py_framebuffer_bind_doc,
             ".. method:: bind()\n"
             "\n"
             "   Bind the framebuffer object.\n"
             "   To make sure that the framebuffer gets restored whether an exception occurs or "
             "not, pack it into a `with` statement.\n"
             "\n");
static PyObject *py_framebuffer_bind(BPyGPUFrameBuffer *self)
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);

  GPU_framebuffer_bind(self->fb);
  Py_INCREF(self);

  return (PyObject *)self;
}

PyDoc_STRVAR(py_framebuffer_free_doc,
             ".. method:: free()\n"
             "\n"
             "   Free the framebuffer object.\n"
             "   The framebuffer will no longer be accessible.\n");
static PyObject *py_framebuffer_free(BPyGPUFrameBuffer *self)
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);
  py_framebuffer_free_if_possible(self->fb);
  self->fb = NULL;
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_framebuffer_texture_attach_doc,
             ".. method:: texture_attach(texture, slot=0, layer=-1, mip=0)\n"
             "\n"
             "   Attach a texture to slot.\n"
             "\n"
             "   :arg texture: Texture to attach.\n"
             "   :type texture: :class:`gpu.types.GPUTexture`\n"
             "   :arg slot: Framebuffer color slot to attach the texture.\n"
             "              For depth or stencil textures this value is not used.\n"
             "   :type slot: `int`\n"
             "   :arg layer: When specified, attach a single layer of a 3D or array texture.\n"
             "               For cube map textures, layer is translated into a cube map face.\n"
             "   :type layer: `int`\n"
             "   :arg mip: Mipmap level of the texture image to be attached.\n"
             "   :type mip: `int`\n");
static PyObject *py_framebuffer_texture_attach(BPyGPUFrameBuffer *self,
                                               PyObject *args,
                                               PyObject *kwds)
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);
  BPyGPUTexture *py_texture;
  int slot = 0;
  int layer = -1;
  int mip = 0;

  static const char *_keywords[] = {"texture", "slot", "layer", "mip", NULL};
  static _PyArg_Parser _parser = {"O!|ii:texture_attach", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &BPyGPUTexture_Type, &py_texture, &slot, &layer, &mip)) {
    return NULL;
  }

  if (py_texture == NULL) {
    PyErr_SetString(PyExc_TypeError,
                    "GPUFrameBuffer.texture_attach() missing required argument texture (pos 1)");
    return NULL;
  }

  if (slot && GPU_texture_depth(py_texture->tex) || GPU_texture_stencil(py_texture->tex)) {
    PyErr_SetString(
        PyExc_TypeError,
        "GPUFrameBuffer.texture_attach() this slot is not intended for depth or stencil textures");
    return NULL;
  }

  GPU_framebuffer_texture_layer_attach(self->fb, py_texture->tex, slot, layer, mip);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_framebuffer_texture_detach_doc,
             ".. method:: texture_detach(texture)\n"
             "\n"
             "   Dettach texture.\n"
             "\n"
             "   :arg texture: Texture to detach.\n");
static PyObject *py_framebuffer_texture_detach(BPyGPUFrameBuffer *self, PyObject *py_texture)
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);
  if (!BPyGPUTexture_Check(py_texture)) {
    return NULL;
  }

  GPU_framebuffer_texture_detach(self->fb, ((BPyGPUTexture *)py_texture)->tex);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_framebuffer_clear_doc,
             ".. method:: clear(color=(0.0, 0.0, 0.0, 1.0), depth=None, stencil=None,)\n"
             "\n"
             "   Fill color, depth and stencil textures with specific value.\n"
             "\n"
             "   :arg color: float sequence each representing ``(r, g, b, a)``.\n"
             "   :type color: sequence of 3 or 4 floats\n"
             "   :arg depth: depth value.\n"
             "   :type depth: `float`\n"
             "   :arg stencil: stencil value.\n"
             "   :type stencil: `int`\n");
static PyObject *py_framebuffer_clear(BPyGPUFrameBuffer *self, PyObject *args, PyObject *kwds)
{
  PY_FRAMEBUFFER_CHECK_OBJ(self);

  if (!GPU_framebuffer_bound(self->fb)) {
    return NULL;
  }

  PyObject *py_col = NULL;
  PyObject *py_depth = NULL;
  PyObject *py_stencil = NULL;

  static const char *_keywords[] = {"color", "depth", "stencil", NULL};
  static _PyArg_Parser _parser = {"|OOO:texture_attach", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &py_col, &py_depth, &py_stencil)) {
    return NULL;
  }

  eGPUFrameBufferBits buffers = 0;
  float col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float depth = 1.0f;
  uint stencil = 0;

  if (py_col && py_col != Py_None) {
    if (mathutils_array_parse(col, 3, 4, py_col, "GPUFrameBuffer.clear(), invalid 'color' arg") ==
        -1) {
      return NULL;
    }
    buffers |= GPU_COLOR_BIT;
  }

  if (py_depth && py_depth != Py_None) {
    depth = PyFloat_AsDouble(py_depth);
    if (PyErr_Occurred()) {
      return NULL;
    }
    buffers |= GPU_DEPTH_BIT;
  }

  if (py_stencil && py_stencil != Py_None) {
    if ((stencil = PyC_Long_AsU32(py_stencil)) == (uint)-1) {
      return NULL;
    }
    buffers |= GPU_STENCIL_BIT;
  }

  GPU_framebuffer_clear(self->fb, buffers, col, depth, stencil);
  Py_RETURN_NONE;
}

static PyObject *py_framebuffer_bind_context_enter(BPyGPUFrameBuffer *self)
{
  if (!py_framebuffer_stack_push_or_error(self->fb)) {
    return NULL;
  }
  Py_RETURN_NONE;
}

static PyObject *py_framebuffer_bind_context_exit(BPyGPUFrameBuffer *UNUSED(self),
                                                  PyObject *UNUSED(args))
{
  if (!py_framebuffer_stack_pop_or_error()) {
    return NULL;
  }
  Py_RETURN_NONE;
}

static void BPyGPUFrameBuffer__tp_dealloc(BPyGPUFrameBuffer *self)
{
  py_framebuffer_free_if_possible(self->fb);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef py_framebuffer_getseters[] = {
    {"viewport",
     (getter)py_framebuffer_viewport_get,
     (setter)py_framebuffer_viewport_set,
     NULL,
     NULL},
    {"is_bound", (getter)py_framebuffer_is_bound, (setter)NULL, py_framebuffer_is_bound_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef py_framebuffer_methods[] = {
    {"bind", (PyCFunction)py_framebuffer_bind, METH_NOARGS, py_framebuffer_bind_doc},
    {"free", (PyCFunction)py_framebuffer_free, METH_NOARGS, py_framebuffer_free_doc},
    {"texture_attach",
     (PyCFunction)py_framebuffer_texture_attach,
     METH_VARARGS | METH_KEYWORDS,
     py_framebuffer_texture_attach_doc},
    {"texture_detach",
     (PyCFunction)py_framebuffer_texture_detach,
     METH_O,
     py_framebuffer_texture_detach_doc},
    {"clear",
     (PyCFunction)py_framebuffer_clear,
     METH_VARARGS | METH_KEYWORDS,
     py_framebuffer_clear_doc},
    {"__enter__", (PyCFunction)py_framebuffer_bind_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)py_framebuffer_bind_context_exit, METH_VARARGS},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(py_framebuffer_doc,
             ".. class:: GPUFrameBuffer()\n"
             "\n"
             "   This object gives access to framebuffer functionallities.\n");
PyTypeObject BPyGPUFrameBuffer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUFrameBuffer",
    .tp_basicsize = sizeof(BPyGPUFrameBuffer),
    .tp_dealloc = (destructor)BPyGPUFrameBuffer__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = py_framebuffer_doc,
    .tp_methods = py_framebuffer_methods,
    .tp_getset = py_framebuffer_getseters,
    .tp_new = py_framebuffer_new,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name gpu.framebuffer Module API
 * \{ */

PyDoc_STRVAR(py_framebuffer_push_doc,
             ".. function:: push()\n"
             "\n"
             "   Bind and add the framebuffer to the stack so that the previous one can be "
             "restored with the pop method.\n");
static PyObject *py_framebuffer_push(BPyGPUFrameBuffer *self)
{
  if (!py_framebuffer_stack_push_or_error(self->fb)) {
    return NULL;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_framebuffer_pop_doc,
             ".. function:: pop()\n"
             "\n"
             "   Remove the last framebuffer from the stack.\n");
static PyObject *py_framebuffer_pop(PyObject *UNUSED(self))
{
  if (!py_framebuffer_stack_pop_or_error()) {
    return NULL;
  }
  Py_RETURN_NONE;
}

static struct PyMethodDef py_framebuffer_module_methods[] = {
    {"push", (PyCFunction)py_framebuffer_push, METH_NOARGS, py_framebuffer_push_doc},
    {"pop", (PyCFunction)py_framebuffer_pop, METH_NOARGS, py_framebuffer_pop_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(bpygpu_framebuffeer_module_doc,
             "This module provides access to GPUFrameBuffer internal functions.");
static PyModuleDef BPyGPU_framebuffer_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.framebuffer",
    .m_doc = bpygpu_framebuffeer_module_doc,
    .m_methods = py_framebuffer_module_methods,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUFrameBuffer_CreatePyObject(GPUFrameBuffer *fb)
{
  BPyGPUFrameBuffer *self;

  self = PyObject_New(BPyGPUFrameBuffer, &BPyGPUFrameBuffer_Type);
  self->fb = fb;

  return (PyObject *)self;
}

PyObject *BPyInit_gpu_framebuffer(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPyGPU_framebuffer_module_def);

  return submodule;
}

/** \} */

#undef PY_FRAMEBUFFER_CHECK_OBJ
