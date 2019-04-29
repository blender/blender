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

#include "gpu_py_api.h"
#include "gpu_py_shader.h"
#include "gpu_py_vertex_buffer.h"
#include "gpu_py_element.h"
#include "gpu_py_batch.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static bool bpygpu_batch_is_program_or_error(BPyGPUBatch *self)
{
  if (!glIsProgram(self->batch->program)) {
    PyErr_SetString(PyExc_RuntimeError, "batch does not have any program assigned to it");
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUBatch Type
 * \{ */

static PyObject *bpygpu_Batch_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  const char *exc_str_missing_arg = "GPUBatch.__new__() missing required argument '%s' (pos %d)";

  struct {
    GPUPrimType type_id;
    BPyGPUVertBuf *py_vertbuf;
    BPyGPUIndexBuf *py_indexbuf;
  } params = {GPU_PRIM_NONE, NULL, NULL};

  static const char *_keywords[] = {"type", "buf", "elem", NULL};
  static _PyArg_Parser _parser = {"|$O&O!O!:GPUBatch.__new__", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        bpygpu_ParsePrimType,
                                        &params.type_id,
                                        &BPyGPUVertBuf_Type,
                                        &params.py_vertbuf,
                                        &BPyGPUIndexBuf_Type,
                                        &params.py_indexbuf)) {
    return NULL;
  }

  if (params.type_id == GPU_PRIM_NONE) {
    PyErr_Format(PyExc_TypeError, exc_str_missing_arg, _keywords[0], 1);
    return NULL;
  }

  if (params.py_vertbuf == NULL) {
    PyErr_Format(PyExc_TypeError, exc_str_missing_arg, _keywords[1], 2);
    return NULL;
  }

  GPUBatch *batch = GPU_batch_create(params.type_id,
                                     params.py_vertbuf->buf,
                                     params.py_indexbuf ? params.py_indexbuf->elem : NULL);

  BPyGPUBatch *ret = (BPyGPUBatch *)BPyGPUBatch_CreatePyObject(batch);

#ifdef USE_GPU_PY_REFERENCES
  ret->references = PyList_New(params.py_indexbuf ? 2 : 1);
  PyList_SET_ITEM(ret->references, 0, (PyObject *)params.py_vertbuf);
  Py_INCREF(params.py_vertbuf);

  if (params.py_indexbuf != NULL) {
    PyList_SET_ITEM(ret->references, 1, (PyObject *)params.py_indexbuf);
    Py_INCREF(params.py_indexbuf);
  }

  PyObject_GC_Track(ret);
#endif

  return (PyObject *)ret;
}

PyDoc_STRVAR(bpygpu_Batch_vertbuf_add_doc,
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
static PyObject *bpygpu_Batch_vertbuf_add(BPyGPUBatch *self, BPyGPUVertBuf *py_buf)
{
  if (!BPyGPUVertBuf_Check(py_buf)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUVertBuf, got %s", Py_TYPE(py_buf)->tp_name);
    return NULL;
  }

  if (self->batch->verts[0]->vertex_len != py_buf->buf->vertex_len) {
    PyErr_Format(PyExc_TypeError,
                 "Expected %d length, got %d",
                 self->batch->verts[0]->vertex_len,
                 py_buf->buf->vertex_len);
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
    bpygpu_Batch_program_set_doc,
    ".. method:: program_set(program)\n"
    "\n"
    "   Assign a shader to this batch that will be used for drawing when not overwritten later.\n"
    "   Note: This method has to be called in the draw context that the batch will be drawn in.\n"
    "   This function does not need to be called when you always set the shader when calling "
    "`batch.draw`.\n"
    "\n"
    "   :param program: The program/shader the batch will use in future draw calls.\n"
    "   :type program: :class:`gpu.types.GPUShader`\n");
static PyObject *bpygpu_Batch_program_set(BPyGPUBatch *self, BPyGPUShader *py_shader)
{
  if (!BPyGPUShader_Check(py_shader)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUShader, got %s", Py_TYPE(py_shader)->tp_name);
    return NULL;
  }

  GPUShader *shader = py_shader->shader;
  GPU_batch_program_set(
      self->batch, GPU_shader_get_program(shader), GPU_shader_get_interface(shader));

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

PyDoc_STRVAR(bpygpu_Batch_draw_doc,
             ".. method:: draw(program=None)\n"
             "\n"
             "   Run the drawing program with the parameters assigned to the batch.\n"
             "\n"
             "   :param program: Program that performs the drawing operations.\n"
             "      If ``None`` is passed, the last program setted to this batch will run.\n"
             "   :type program: :class:`gpu.types.GPUShader`\n");
static PyObject *bpygpu_Batch_draw(BPyGPUBatch *self, PyObject *args)
{
  BPyGPUShader *py_program = NULL;

  if (!PyArg_ParseTuple(args, "|O!:GPUBatch.draw", &BPyGPUShader_Type, &py_program)) {
    return NULL;
  }
  else if (py_program == NULL) {
    if (!bpygpu_batch_is_program_or_error(self)) {
      return NULL;
    }
  }
  else if (self->batch->program != GPU_shader_get_program(py_program->shader)) {
    GPU_batch_program_set(self->batch,
                          GPU_shader_get_program(py_program->shader),
                          GPU_shader_get_interface(py_program->shader));
  }

  GPU_batch_draw(self->batch);
  Py_RETURN_NONE;
}

static PyObject *bpygpu_Batch_program_use_begin(BPyGPUBatch *self)
{
  if (!bpygpu_batch_is_program_or_error(self)) {
    return NULL;
  }
  GPU_batch_program_use_begin(self->batch);
  Py_RETURN_NONE;
}

static PyObject *bpygpu_Batch_program_use_end(BPyGPUBatch *self)
{
  if (!bpygpu_batch_is_program_or_error(self)) {
    return NULL;
  }
  GPU_batch_program_use_end(self->batch);
  Py_RETURN_NONE;
}

static struct PyMethodDef bpygpu_Batch_methods[] = {
    {"vertbuf_add", (PyCFunction)bpygpu_Batch_vertbuf_add, METH_O, bpygpu_Batch_vertbuf_add_doc},
    {"program_set", (PyCFunction)bpygpu_Batch_program_set, METH_O, bpygpu_Batch_program_set_doc},
    {"draw", (PyCFunction)bpygpu_Batch_draw, METH_VARARGS, bpygpu_Batch_draw_doc},
    {"_program_use_begin", (PyCFunction)bpygpu_Batch_program_use_begin, METH_NOARGS, ""},
    {"_program_use_end", (PyCFunction)bpygpu_Batch_program_use_end, METH_NOARGS, ""},
    {NULL, NULL, 0, NULL},
};

#ifdef USE_GPU_PY_REFERENCES

static int bpygpu_Batch_traverse(BPyGPUBatch *self, visitproc visit, void *arg)
{
  Py_VISIT(self->references);
  return 0;
}

static int bpygpu_Batch_clear(BPyGPUBatch *self)
{
  Py_CLEAR(self->references);
  return 0;
}

#endif

static void bpygpu_Batch_dealloc(BPyGPUBatch *self)
{
  GPU_batch_discard(self->batch);

#ifdef USE_GPU_PY_REFERENCES
  if (self->references) {
    PyObject_GC_UnTrack(self);
    bpygpu_Batch_clear(self);
    Py_XDECREF(self->references);
  }
#endif

  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(
    py_gpu_batch_doc,
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
    .tp_dealloc = (destructor)bpygpu_Batch_dealloc,
#ifdef USE_GPU_PY_REFERENCES
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = py_gpu_batch_doc,
    .tp_traverse = (traverseproc)bpygpu_Batch_traverse,
    .tp_clear = (inquiry)bpygpu_Batch_clear,
#else
    .tp_flags = Py_TPFLAGS_DEFAULT,
#endif
    .tp_methods = bpygpu_Batch_methods,
    .tp_new = bpygpu_Batch_new,
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
