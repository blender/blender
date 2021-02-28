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
 *
 * Copyright 2015, Blender Foundation.
 */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the offscreen functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "GPU_batch.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h"
#include "gpu_py_element.h"
#include "gpu_py_shader.h"
#include "gpu_py_vertex_buffer.h"

#include "gpu_py_batch.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static bool pygpu_batch_is_program_or_error(BPyGPUBatch *self)
{
  if (!self->batch->shader) {
    PyErr_SetString(PyExc_RuntimeError, "batch does not have any program assigned to it");
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUBatch Type
 * \{ */

static PyObject *pygpu_batch__tp_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  const char *exc_str_missing_arg = "GPUBatch.__new__() missing required argument '%s' (pos %d)";

  struct PyC_StringEnum prim_type = {bpygpu_primtype_items, GPU_PRIM_NONE};
  BPyGPUVertBuf *py_vertbuf = NULL;
  BPyGPUIndexBuf *py_indexbuf = NULL;

  static const char *_keywords[] = {"type", "buf", "elem", NULL};
  static _PyArg_Parser _parser = {"|$O&O!O!:GPUBatch.__new__", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        PyC_ParseStringEnum,
                                        &prim_type,
                                        &BPyGPUVertBuf_Type,
                                        &py_vertbuf,
                                        &BPyGPUIndexBuf_Type,
                                        &py_indexbuf)) {
    return NULL;
  }

  BLI_assert(prim_type.value_found != GPU_PRIM_NONE);

  if (py_vertbuf == NULL) {
    PyErr_Format(PyExc_TypeError, exc_str_missing_arg, _keywords[1], 2);
    return NULL;
  }

  GPUBatch *batch = GPU_batch_create(
      prim_type.value_found, py_vertbuf->buf, py_indexbuf ? py_indexbuf->elem : NULL);

  BPyGPUBatch *ret = (BPyGPUBatch *)BPyGPUBatch_CreatePyObject(batch);

#ifdef USE_GPU_PY_REFERENCES
  ret->references = PyList_New(py_indexbuf ? 2 : 1);
  PyList_SET_ITEM(ret->references, 0, (PyObject *)py_vertbuf);
  Py_INCREF(py_vertbuf);

  if (py_indexbuf != NULL) {
    PyList_SET_ITEM(ret->references, 1, (PyObject *)py_indexbuf);
    Py_INCREF(py_indexbuf);
  }

  PyObject_GC_Track(ret);
#endif

  return (PyObject *)ret;
}

PyDoc_STRVAR(pygpu_batch_vertbuf_add_doc,
".. method:: vertbuf_add(buf)\n"
"\n"
"   Add another vertex buffer to the Batch.\n"
"   It is not possible to add more vertices to the batch using this method.\n"
"   Instead it can be used to add more attributes to the existing vertices.\n"
"   A good use case would be when you have a separate\n"
"   vertex buffer for vertex positions and vertex normals.\n"
"   Current a batch can have at most " STRINGIFY(GPU_BATCH_VBO_MAX_LEN) " vertex buffers.\n"
"\n"
"   :param buf: The vertex buffer that will be added to the batch.\n"
"   :type buf: :class:`gpu.types.GPUVertBuf`\n"
);
static PyObject *pygpu_batch_vertbuf_add(BPyGPUBatch *self, BPyGPUVertBuf *py_buf)
{
  if (!BPyGPUVertBuf_Check(py_buf)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUVertBuf, got %s", Py_TYPE(py_buf)->tp_name);
    return NULL;
  }

  if (GPU_vertbuf_get_vertex_len(self->batch->verts[0]) !=
      GPU_vertbuf_get_vertex_len(py_buf->buf)) {
    PyErr_Format(PyExc_TypeError,
                 "Expected %d length, got %d",
                 GPU_vertbuf_get_vertex_len(self->batch->verts[0]),
                 GPU_vertbuf_get_vertex_len(py_buf->buf));
    return NULL;
  }

  if (self->batch->verts[GPU_BATCH_VBO_MAX_LEN - 1] != NULL) {
    PyErr_SetString(
        PyExc_RuntimeError,
        "Maximum number of vertex buffers exceeded: " STRINGIFY(GPU_BATCH_VBO_MAX_LEN));
    return NULL;
  }

#ifdef USE_GPU_PY_REFERENCES
  /* Hold user */
  PyList_Append(self->references, (PyObject *)py_buf);
#endif

  GPU_batch_vertbuf_add(self->batch, py_buf->buf);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    pygpu_batch_program_set_doc,
    ".. method:: program_set(program)\n"
    "\n"
    "   Assign a shader to this batch that will be used for drawing when not overwritten later.\n"
    "   Note: This method has to be called in the draw context that the batch will be drawn in.\n"
    "   This function does not need to be called when you always\n"
    "   set the shader when calling :meth:`gpu.types.GPUBatch.draw`.\n"
    "\n"
    "   :param program: The program/shader the batch will use in future draw calls.\n"
    "   :type program: :class:`gpu.types.GPUShader`\n");
static PyObject *pygpu_batch_program_set(BPyGPUBatch *self, BPyGPUShader *py_shader)
{
  if (!BPyGPUShader_Check(py_shader)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUShader, got %s", Py_TYPE(py_shader)->tp_name);
    return NULL;
  }

  GPUShader *shader = py_shader->shader;
  GPU_batch_set_shader(self->batch, shader);

#ifdef USE_GPU_PY_REFERENCES
  /* Remove existing user (if any), hold new user. */
  int i = PyList_GET_SIZE(self->references);
  while (--i != -1) {
    PyObject *py_shader_test = PyList_GET_ITEM(self->references, i);
    if (BPyGPUShader_Check(py_shader_test)) {
      PyList_SET_ITEM(self->references, i, (PyObject *)py_shader);
      Py_INCREF(py_shader);
      Py_DECREF(py_shader_test);
      /* Only ever reference one shader. */
      break;
    }
  }
  if (i != -1) {
    PyList_Append(self->references, (PyObject *)py_shader);
  }
#endif

  Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_batch_draw_doc,
             ".. method:: draw(program=None)\n"
             "\n"
             "   Run the drawing program with the parameters assigned to the batch.\n"
             "\n"
             "   :param program: Program that performs the drawing operations.\n"
             "      If ``None`` is passed, the last program set to this batch will run.\n"
             "   :type program: :class:`gpu.types.GPUShader`\n");
static PyObject *pygpu_batch_draw(BPyGPUBatch *self, PyObject *args)
{
  BPyGPUShader *py_program = NULL;

  if (!PyArg_ParseTuple(args, "|O!:GPUBatch.draw", &BPyGPUShader_Type, &py_program)) {
    return NULL;
  }
  if (py_program == NULL) {
    if (!pygpu_batch_is_program_or_error(self)) {
      return NULL;
    }
  }
  else if (self->batch->shader != py_program->shader) {
    GPU_batch_set_shader(self->batch, py_program->shader);
  }

  GPU_batch_draw(self->batch);
  Py_RETURN_NONE;
}

static PyObject *pygpu_batch_program_use_begin(BPyGPUBatch *self)
{
  if (!pygpu_batch_is_program_or_error(self)) {
    return NULL;
  }
  GPU_shader_bind(self->batch->shader);
  Py_RETURN_NONE;
}

static PyObject *pygpu_batch_program_use_end(BPyGPUBatch *self)
{
  if (!pygpu_batch_is_program_or_error(self)) {
    return NULL;
  }
  GPU_shader_unbind();
  Py_RETURN_NONE;
}

static struct PyMethodDef pygpu_batch__tp_methods[] = {
    {"vertbuf_add", (PyCFunction)pygpu_batch_vertbuf_add, METH_O, pygpu_batch_vertbuf_add_doc},
    {"program_set", (PyCFunction)pygpu_batch_program_set, METH_O, pygpu_batch_program_set_doc},
    {"draw", (PyCFunction)pygpu_batch_draw, METH_VARARGS, pygpu_batch_draw_doc},
    {"_program_use_begin", (PyCFunction)pygpu_batch_program_use_begin, METH_NOARGS, ""},
    {"_program_use_end", (PyCFunction)pygpu_batch_program_use_end, METH_NOARGS, ""},
    {NULL, NULL, 0, NULL},
};

#ifdef USE_GPU_PY_REFERENCES

static int pygpu_batch__tp_traverse(BPyGPUBatch *self, visitproc visit, void *arg)
{
  Py_VISIT(self->references);
  return 0;
}

static int pygpu_batch__tp_clear(BPyGPUBatch *self)
{
  Py_CLEAR(self->references);
  return 0;
}

#endif

static void pygpu_batch__tp_dealloc(BPyGPUBatch *self)
{
  GPU_batch_discard(self->batch);

#ifdef USE_GPU_PY_REFERENCES
  if (self->references) {
    PyObject_GC_UnTrack(self);
    pygpu_batch__tp_clear(self);
    Py_XDECREF(self->references);
  }
#endif

  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(
    pygpu_batch__tp_doc,
    ".. class:: GPUBatch(type, buf, elem=None)\n"
    "\n"
    "   Reusable container for drawable geometry.\n"
    "\n"
    "   :arg type: One of these primitive types: {\n"
    "       `POINTS`,\n"
    "       `LINES`,\n"
    "       `TRIS`,\n"
    "       `LINE_STRIP`,\n"
    "       `LINE_LOOP`,\n"
    "       `TRI_STRIP`,\n"
    "       `TRI_FAN`,\n"
    "       `LINES_ADJ`,\n"
    "       `TRIS_ADJ`,\n"
    "       `LINE_STRIP_ADJ` }\n"
    "   :type type: `str`\n"
    "   :arg buf: Vertex buffer containing all or some of the attributes required for drawing.\n"
    "   :type buf: :class:`gpu.types.GPUVertBuf`\n"
    "   :arg elem: An optional index buffer.\n"
    "   :type elem: :class:`gpu.types.GPUIndexBuf`\n");
PyTypeObject BPyGPUBatch_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUBatch",
    .tp_basicsize = sizeof(BPyGPUBatch),
    .tp_dealloc = (destructor)pygpu_batch__tp_dealloc,
#ifdef USE_GPU_PY_REFERENCES
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = pygpu_batch__tp_doc,
    .tp_traverse = (traverseproc)pygpu_batch__tp_traverse,
    .tp_clear = (inquiry)pygpu_batch__tp_clear,
#else
    .tp_flags = Py_TPFLAGS_DEFAULT,
#endif
    .tp_methods = pygpu_batch__tp_methods,
    .tp_new = pygpu_batch__tp_new,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUBatch_CreatePyObject(GPUBatch *batch)
{
  BPyGPUBatch *self;

#ifdef USE_GPU_PY_REFERENCES
  self = (BPyGPUBatch *)_PyObject_GC_New(&BPyGPUBatch_Type);
  self->references = NULL;
#else
  self = PyObject_New(BPyGPUBatch, &BPyGPUBatch_Type);
#endif

  self->batch = batch;

  return (PyObject *)self;
}

/** \} */

#undef BPY_GPU_BATCH_CHECK_OBJ
