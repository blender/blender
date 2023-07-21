/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the off-screen functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_batch.h"

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

static PyObject *pygpu_batch__tp_new(PyTypeObject * /*type*/, PyObject *args, PyObject *kwds)
{
  const char *exc_str_missing_arg = "GPUBatch.__new__() missing required argument '%s' (pos %d)";

  struct PyC_StringEnum prim_type = {bpygpu_primtype_items, GPU_PRIM_NONE};
  BPyGPUVertBuf *py_vertbuf = nullptr;
  BPyGPUIndexBuf *py_indexbuf = nullptr;

  static const char *_keywords[] = {"type", "buf", "elem", nullptr};
  static _PyArg_Parser _parser = {
      "|$" /* Optional keyword only arguments. */
      "O&" /* `type` */
      "O!" /* `buf` */
      "O!" /* `elem` */
      ":GPUBatch.__new__",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        PyC_ParseStringEnum,
                                        &prim_type,
                                        &BPyGPUVertBuf_Type,
                                        &py_vertbuf,
                                        &BPyGPUIndexBuf_Type,
                                        &py_indexbuf))
  {
    return nullptr;
  }

  BLI_assert(prim_type.value_found != GPU_PRIM_NONE);
  if (prim_type.value_found == GPU_PRIM_LINE_LOOP) {
    PyErr_WarnEx(PyExc_DeprecationWarning,
                 "'LINE_LOOP' is deprecated. Please use 'LINE_STRIP' and close the segment.",
                 1);
  }
  else if (prim_type.value_found == GPU_PRIM_TRI_FAN) {
    PyErr_WarnEx(
        PyExc_DeprecationWarning,
        "'TRI_FAN' is deprecated. Please use 'TRI_STRIP' or 'TRIS' and try modifying your "
        "vertices or indices to match the topology.",
        1);
  }

  if (py_vertbuf == nullptr) {
    PyErr_Format(PyExc_TypeError, exc_str_missing_arg, _keywords[1], 2);
    return nullptr;
  }

  GPUBatch *batch = GPU_batch_create(GPUPrimType(prim_type.value_found),
                                     py_vertbuf->buf,
                                     py_indexbuf ? py_indexbuf->elem : nullptr);

  BPyGPUBatch *ret = (BPyGPUBatch *)BPyGPUBatch_CreatePyObject(batch);

#ifdef USE_GPU_PY_REFERENCES
  ret->references = PyList_New(py_indexbuf ? 2 : 1);
  PyList_SET_ITEM(ret->references, 0, (PyObject *)py_vertbuf);
  Py_INCREF(py_vertbuf);

  if (py_indexbuf != nullptr) {
    PyList_SET_ITEM(ret->references, 1, (PyObject *)py_indexbuf);
    Py_INCREF(py_indexbuf);
  }

  BLI_assert(!PyObject_GC_IsTracked((PyObject *)ret));
  PyObject_GC_Track(ret);
#endif

  return (PyObject *)ret;
}

PyDoc_STRVAR(pygpu_batch_vertbuf_add_doc, ".. method:: vertbuf_add(buf)\n"
"\n"
"   Add another vertex buffer to the Batch.\n"
"   It is not possible to add more vertices to the batch using this method.\n"
"   Instead it can be used to add more attributes to the existing vertices.\n"
"   A good use case would be when you have a separate\n"
"   vertex buffer for vertex positions and vertex normals.\n"
"   Current a batch can have at most " STRINGIFY(GPU_BATCH_VBO_MAX_LEN) " vertex buffers.\n"
"\n"
"   :arg buf: The vertex buffer that will be added to the batch.\n"
"   :type buf: :class:`gpu.types.GPUVertBuf`\n"
);
static PyObject *pygpu_batch_vertbuf_add(BPyGPUBatch *self, BPyGPUVertBuf *py_buf)
{
  if (!BPyGPUVertBuf_Check(py_buf)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUVertBuf, got %s", Py_TYPE(py_buf)->tp_name);
    return nullptr;
  }

  if (GPU_vertbuf_get_vertex_len(self->batch->verts[0]) != GPU_vertbuf_get_vertex_len(py_buf->buf))
  {
    PyErr_Format(PyExc_TypeError,
                 "Expected %d length, got %d",
                 GPU_vertbuf_get_vertex_len(self->batch->verts[0]),
                 GPU_vertbuf_get_vertex_len(py_buf->buf));
    return nullptr;
  }

  if (self->batch->verts[GPU_BATCH_VBO_MAX_LEN - 1] != nullptr) {
    PyErr_SetString(
        PyExc_RuntimeError,
        "Maximum number of vertex buffers exceeded: " STRINGIFY(GPU_BATCH_VBO_MAX_LEN));
    return nullptr;
  }

#ifdef USE_GPU_PY_REFERENCES
  /* Hold user */
  PyList_Append(self->references, (PyObject *)py_buf);
#endif

  GPU_batch_vertbuf_add(self->batch, py_buf->buf, false);
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
    "   :arg program: The program/shader the batch will use in future draw calls.\n"
    "   :type program: :class:`gpu.types.GPUShader`\n");
static PyObject *pygpu_batch_program_set(BPyGPUBatch *self, BPyGPUShader *py_shader)
{
  if (!BPyGPUShader_Check(py_shader)) {
    PyErr_Format(PyExc_TypeError, "Expected a GPUShader, got %s", Py_TYPE(py_shader)->tp_name);
    return nullptr;
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
             "   :arg program: Program that performs the drawing operations.\n"
             "      If ``None`` is passed, the last program set to this batch will run.\n"
             "   :type program: :class:`gpu.types.GPUShader`\n");
static PyObject *pygpu_batch_draw(BPyGPUBatch *self, PyObject *args)
{
  BPyGPUShader *py_program = nullptr;

  if (!PyArg_ParseTuple(args, "|O!:GPUBatch.draw", &BPyGPUShader_Type, &py_program)) {
    return nullptr;
  }
  if (py_program == nullptr) {
    if (!pygpu_batch_is_program_or_error(self)) {
      return nullptr;
    }
  }
  else if (self->batch->shader != py_program->shader) {
    GPU_batch_set_shader(self->batch, py_program->shader);
  }

  GPU_batch_draw(self->batch);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    pygpu_batch_draw_instanced_doc,
    ".. method:: draw_instanced(program, *, instance_start=0, instance_count=0)\n"
    "\n"
    "   Draw multiple instances of the drawing program with the parameters assigned\n"
    "   to the batch. In the vertex shader, `gl_InstanceID` will contain the instance\n"
    "   number being drawn.\n"
    "\n"
    "   :arg program: Program that performs the drawing operations.\n"
    "   :type program: :class:`gpu.types.GPUShader`\n"
    "   :arg instance_start: Number of the first instance to draw.\n"
    "   :type instance_start: int\n"
    "   :arg instance_count: Number of instances to draw. When not provided or set to 0\n"
    "      the number of instances will be determined by the number of rows in the first\n"
    "      vertex buffer.\n"
    "   :type instance_count: int\n");
static PyObject *pygpu_batch_draw_instanced(BPyGPUBatch *self, PyObject *args, PyObject *kw)
{
  BPyGPUShader *py_program = nullptr;
  int instance_start = 0;
  int instance_count = 0;

  static const char *_keywords[] = {"program", "instance_start", "instance_count", nullptr};
  static _PyArg_Parser _parser = {
      "O!" /* `program` */
      "|$" /* Optional keyword only arguments. */
      "i"  /* `instance_start` */
      "i"  /* `instance_count' */
      ":GPUBatch.draw_instanced",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &BPyGPUShader_Type, &py_program, &instance_start, &instance_count))
  {
    return nullptr;
  }

  GPU_batch_set_shader(self->batch, py_program->shader);
  GPU_batch_draw_instance_range(self->batch, instance_start, instance_count);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_batch_draw_range_doc,
             ".. method:: draw_range(program, *, elem_start=0, elem_count=0)\n"
             "\n"
             "   Run the drawing program with the parameters assigned to the batch. Only draw\n"
             "   the `elem_count` elements of the index buffer starting at `elem_start` \n"
             "\n"
             "   :arg program: Program that performs the drawing operations.\n"
             "   :type program: :class:`gpu.types.GPUShader`\n"
             "   :arg elem_start: First index to draw. When not provided or set to 0 drawing\n"
             "      will start from the first element of the index buffer.\n"
             "   :type elem_start: int\n"
             "   :arg elem_count: Number of elements of the index buffer to draw. When not\n"
             "      provided or set to 0 all elements from `elem_start` to the end of the\n"
             "      index buffer will be drawn.\n"
             "   :type elem_count: int\n");
static PyObject *pygpu_batch_draw_range(BPyGPUBatch *self, PyObject *args, PyObject *kw)
{
  BPyGPUShader *py_program = nullptr;
  int elem_start = 0;
  int elem_count = 0;

  static const char *_keywords[] = {"program", "elem_start", "elem_count", nullptr};
  static _PyArg_Parser _parser = {
      "O!" /* `program` */
      "|$" /* Optional keyword only arguments. */
      "i"  /* `elem_start' */
      "i"  /* `elem_count' */
      ":GPUBatch.draw_range",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &BPyGPUShader_Type, &py_program, &elem_start, &elem_count))
  {
    return nullptr;
  }

  GPU_batch_set_shader(self->batch, py_program->shader);
  GPU_batch_draw_range(self->batch, elem_start, elem_count);
  Py_RETURN_NONE;
}

static PyObject *pygpu_batch_program_use_begin(BPyGPUBatch *self)
{
  if (!pygpu_batch_is_program_or_error(self)) {
    return nullptr;
  }
  GPU_shader_bind(self->batch->shader);
  Py_RETURN_NONE;
}

static PyObject *pygpu_batch_program_use_end(BPyGPUBatch *self)
{
  if (!pygpu_batch_is_program_or_error(self)) {
    return nullptr;
  }
  GPU_shader_unbind();
  Py_RETURN_NONE;
}

static PyMethodDef pygpu_batch__tp_methods[] = {
    {"vertbuf_add", (PyCFunction)pygpu_batch_vertbuf_add, METH_O, pygpu_batch_vertbuf_add_doc},
    {"program_set", (PyCFunction)pygpu_batch_program_set, METH_O, pygpu_batch_program_set_doc},
    {"draw", (PyCFunction)pygpu_batch_draw, METH_VARARGS, pygpu_batch_draw_doc},
    {"draw_instanced",
     (PyCFunction)pygpu_batch_draw_instanced,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_batch_draw_instanced_doc},
    {"draw_range",
     (PyCFunction)pygpu_batch_draw_range,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_batch_draw_range_doc},
    {"_program_use_begin", (PyCFunction)pygpu_batch_program_use_begin, METH_NOARGS, ""},
    {"_program_use_end", (PyCFunction)pygpu_batch_program_use_end, METH_NOARGS, ""},
    {nullptr, nullptr, 0, nullptr},
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

static int pygpu_batch__tp_is_gc(BPyGPUBatch *self)
{
  return self->references != nullptr;
}

#endif

static void pygpu_batch__tp_dealloc(BPyGPUBatch *self)
{
  GPU_batch_discard(self->batch);

#ifdef USE_GPU_PY_REFERENCES
  PyObject_GC_UnTrack(self);
  if (self->references) {
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
    "   :arg type: The primitive type of geometry to be drawn.\n"
    "      Possible values are `POINTS`, `LINES`, `TRIS`, `LINE_STRIP`, `LINE_LOOP`, `TRI_STRIP`, "
    "`TRI_FAN`, `LINES_ADJ`, `TRIS_ADJ` and `LINE_STRIP_ADJ`.\n"
    "   :type type: str\n"
    "   :arg buf: Vertex buffer containing all or some of the attributes required for drawing.\n"
    "   :type buf: :class:`gpu.types.GPUVertBuf`\n"
    "   :arg elem: An optional index buffer.\n"
    "   :type elem: :class:`gpu.types.GPUIndexBuf`\n");
PyTypeObject BPyGPUBatch_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUBatch",
    /*tp_basicsize*/ sizeof(BPyGPUBatch),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_batch__tp_dealloc,
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
#ifdef USE_GPU_PY_REFERENCES
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
#else
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
#endif
    /*tp_doc*/ pygpu_batch__tp_doc,
#ifdef USE_GPU_PY_REFERENCES
    /*tp_traverse*/ (traverseproc)pygpu_batch__tp_traverse,
#else
    /*tp_traverse*/ nullptr,
#endif
#ifdef USE_GPU_PY_REFERENCES
    /*tp_clear*/ (inquiry)pygpu_batch__tp_clear,
#else
    /*tp_clear*/ nullptr,
#endif
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_batch__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_batch__tp_new,
    /*tp_free*/ nullptr,
#ifdef USE_GPU_PY_REFERENCES
    /*tp_is_gc*/ (inquiry)pygpu_batch__tp_is_gc,
#else
    /*tp_is_gc*/ nullptr,
#endif
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

PyObject *BPyGPUBatch_CreatePyObject(GPUBatch *batch)
{
  BPyGPUBatch *self;

#ifdef USE_GPU_PY_REFERENCES
  self = (BPyGPUBatch *)_PyObject_GC_New(&BPyGPUBatch_Type);
  self->references = nullptr;
#else
  self = PyObject_New(BPyGPUBatch, &BPyGPUBatch_Type);
#endif

  self->batch = batch;

  return (PyObject *)self;
}

/** \} */

#undef BPY_GPU_BATCH_CHECK_OBJ
