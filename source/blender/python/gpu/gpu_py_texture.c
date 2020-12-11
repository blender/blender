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
 * This file defines the texture functionalities of the 'gpu' module
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_context.h"
#include "GPU_texture.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py_api.h"

#include "gpu_py_texture.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPUTexture Common Utilities
 * \{ */

static const struct PyC_StringEnumItems pygpu_textureformat_items[] = {
    {GPU_RGBA8UI, "RGBA8UI"},
    {GPU_RGBA8I, "RGBA8I"},
    {GPU_RGBA8, "RGBA8"},
    {GPU_RGBA32UI, "RGBA32UI"},
    {GPU_RGBA32I, "RGBA32I"},
    {GPU_RGBA32F, "RGBA32F"},
    {GPU_RGBA16UI, "RGBA16UI"},
    {GPU_RGBA16I, "RGBA16I"},
    {GPU_RGBA16F, "RGBA16F"},
    {GPU_RGBA16, "RGBA16"},
    {GPU_RG8UI, "RG8UI"},
    {GPU_RG8I, "RG8I"},
    {GPU_RG8, "RG8"},
    {GPU_RG32UI, "RG32UI"},
    {GPU_RG32I, "RG32I"},
    {GPU_RG32F, "RG32F"},
    {GPU_RG16UI, "RG16UI"},
    {GPU_RG16I, "RG16I"},
    {GPU_RG16F, "RG16F"},
    {GPU_RG16, "RG16"},
    {GPU_R8UI, "R8UI"},
    {GPU_R8I, "R8I"},
    {GPU_R8, "R8"},
    {GPU_R32UI, "R32UI"},
    {GPU_R32I, "R32I"},
    {GPU_R32F, "R32F"},
    {GPU_R16UI, "R16UI"},
    {GPU_R16I, "R16I"},
    {GPU_R16F, "R16F"},
    {GPU_R16, "R16"},
    {GPU_R11F_G11F_B10F, "R11F_G11F_B10F"},
    {GPU_DEPTH32F_STENCIL8, "DEPTH32F_STENCIL8"},
    {GPU_DEPTH24_STENCIL8, "DEPTH24_STENCIL8"},
    {GPU_SRGB8_A8, "SRGB8_A8"},
    {GPU_RGB16F, "RGB16F"},
    {GPU_SRGB8_A8_DXT1, "SRGB8_A8_DXT1"},
    {GPU_SRGB8_A8_DXT3, "SRGB8_A8_DXT3"},
    {GPU_SRGB8_A8_DXT5, "SRGB8_A8_DXT5"},
    {GPU_RGBA8_DXT1, "RGBA8_DXT1"},
    {GPU_RGBA8_DXT3, "RGBA8_DXT3"},
    {GPU_RGBA8_DXT5, "RGBA8_DXT5"},
    {GPU_DEPTH_COMPONENT32F, "DEPTH_COMPONENT32F"},
    {GPU_DEPTH_COMPONENT24, "DEPTH_COMPONENT24"},
    {GPU_DEPTH_COMPONENT16, "DEPTH_COMPONENT16"},
    {0, NULL},
};

static const struct PyC_StringEnumItems pygpu_dataformat_items[] = {
    {GPU_DATA_FLOAT, "FLOAT"},
    {GPU_DATA_INT, "INT"},
    {GPU_DATA_UNSIGNED_INT, "UNSIGNED_INT"},
    {GPU_DATA_UNSIGNED_BYTE, "UNSIGNED_BYTE"},
    {GPU_DATA_UNSIGNED_INT_24_8, "UNSIGNED_INT_24_8"},
    {GPU_DATA_10_11_11_REV, "10_11_11_REV"},
    {0, NULL},
};

static int py_texture_valid_check(BPyGPUTexture *bpygpu_tex)
{
  if (UNLIKELY(bpygpu_tex->tex == NULL)) {
    PyErr_SetString(PyExc_ReferenceError, "GPU texture was freed, no further access is valid");
    return -1;
  }
  return 0;
}

#define PY_TEXTURE_CHECK_OBJ(bpygpu) \
  { \
    if (UNLIKELY(py_texture_valid_check(bpygpu) == -1)) { \
      return NULL; \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUTexture Type
 * \{ */

static PyObject *py_texture_new(PyTypeObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  GPUTexture *tex = NULL;
  int width, height, mips;
  const struct PyC_StringEnum pygpu_textureformat = {&pygpu_textureformat_items, GPU_RGBA8};
  char err_out[256] = "unknown error. See console";

  static const char *_keywords[] = {"width", "height", "mips", "format", NULL};
  static _PyArg_Parser _parser = {"iiiO&:GPUTexture.__new__", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &width,
                                        &height,
                                        &mips,
                                        PyC_ParseStringEnum,
                                        &pygpu_textureformat)) {
    return NULL;
  }

  if (GPU_context_active_get()) {
    tex = GPU_texture_create_2d(
        "python_texture", width, height, mips, pygpu_textureformat.value_found, NULL);
  }
  else {
    strncpy(err_out, "No active GPU context found", 256);
  }

  if (tex == NULL) {
    PyErr_Format(PyExc_RuntimeError, "gpu.texture.new(...) failed with '%s'", err_out);
    return NULL;
  }

  return BPyGPUTexture_CreatePyObject(tex);
}

PyDoc_STRVAR(py_texture_width_doc, "Width of the texture.\n\n:type: `int`");
static PyObject *py_texture_width_get(BPyGPUTexture *self, void *UNUSED(type))
{
  PY_TEXTURE_CHECK_OBJ(self);
  return PyLong_FromLong(GPU_texture_width(self->tex));
}

PyDoc_STRVAR(py_texture_height_doc, "Height of the texture.\n\n:type: `int`");
static PyObject *py_texture_height_get(BPyGPUTexture *self, void *UNUSED(type))
{
  PY_TEXTURE_CHECK_OBJ(self);
  return PyLong_FromLong(GPU_texture_height(self->tex));
}

PyDoc_STRVAR(py_texture_bind_doc,
             ".. method:: bind(number)\n"
             "\n"
             "   Bind the texture object.\n"
             "\n"
             "   :arg slot: texture slot.\n"
             "   :type slot: `int`\n");
static PyObject *py_texture_bind(BPyGPUTexture *self, PyObject *args, PyObject *kwds)
{
  PY_TEXTURE_CHECK_OBJ(self);
  int slot;

  static const char *_keywords[] = {"slot", NULL};
  static _PyArg_Parser _parser = {"i:bind", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &slot)) {
    return NULL;
  }

  GPU_texture_bind(self->tex, slot);

  Py_INCREF(self);

  return (PyObject *)self;
}

PyDoc_STRVAR(py_texture_unbind_doc,
             ".. method:: unbind()\n"
             "\n"
             "   Unbind the texture object.\n");
static PyObject *py_texture_unbind(BPyGPUTexture *self)
{
  GPU_texture_unbind(self->tex);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_texture_free_doc,
             ".. method:: free()\n"
             "\n"
             "   Free the texture object.\n"
             "   The texture object will no longer be accessible.\n");
static PyObject *py_texture_free(BPyGPUTexture *self)
{
  PY_TEXTURE_CHECK_OBJ(self);

  GPU_texture_free(self->tex);
  self->tex = NULL;
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_texture_clear_doc,
             ".. method:: clear(format='FLOAT', value=(0.0, 0.0, 0.0, 1.0))\n"
             "\n"
             "   Fill texture with specific value.\n"
             "\n"
             "   :param format: One of these primitive types: {\n"
             "      `FLOAT`,\n"
             "      `INT`,\n"
             "      `UNSIGNED_INT`,\n"
             "      `UNSIGNED_BYTE`,\n"
             "      `UNSIGNED_INT_24_8`,\n"
             "      `10_11_11_REV`,\n"
             "   :type type: `str`\n"
             "   :arg value: sequence each representing the value to fill.\n"
             "   :type value: sequence of 1, 2, 3 or 4 values\n");
static PyObject *py_texture_clear(BPyGPUTexture *self, PyObject *args)
{
  PY_TEXTURE_CHECK_OBJ(self);
  const struct PyC_StringEnum pygpu_dataformat = {&pygpu_dataformat_items, GPU_DATA_FLOAT};
  union {
    int i[4];
    float f[4];
  } values;

  PyObject *py_values;
  if (!PyArg_ParseTuple(args, "O&O:clear", PyC_ParseStringEnum, &pygpu_dataformat, &py_values)) {
    return NULL;
  }

  if (!PySequence_Check(py_values)) {
    return NULL;
  }

  int dimensions = PySequence_Size(py_values);
  if (dimensions > 4) {
    PyErr_SetString(PyExc_AttributeError, "too many dimensions, max is 4");
    return NULL;
  }

  memset(&values, 0, sizeof(values));
  for (int i = 0; i < dimensions; i++) {
    PyObject *ob = PySequence_GetItem(py_values, i);

    if (pygpu_dataformat.value_found == GPU_DATA_FLOAT) {
      values.f[i] = (float)PyFloat_AsDouble(ob);
    }
    else {
      values.i[i] = PyLong_AsLong(ob);
    }
    Py_DECREF(ob);
  }

  GPU_texture_clear(self->tex, pygpu_dataformat.value_found, &values);
  Py_RETURN_NONE;
}

static PyObject *py_texture_bind_context_enter(BPyGPUTexture *UNUSED(self))
{
  Py_RETURN_NONE;
}

static PyObject *py_texture_bind_context_exit(BPyGPUTexture *self, PyObject *UNUSED(args))
{
  GPU_texture_unbind(self->tex);
  Py_RETURN_NONE;
}

static void BPyGPUTexture__tp_dealloc(BPyGPUTexture *self)
{
  if (self->tex) {
    GPU_texture_free(self->tex);
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef py_texture_getseters[] = {
    {"width", (getter)py_texture_width_get, (setter)NULL, py_texture_width_doc, NULL},
    {"height", (getter)py_texture_height_get, (setter)NULL, py_texture_height_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef py_texture_methods[] = {
    {"bind", (PyCFunction)py_texture_bind, METH_VARARGS | METH_KEYWORDS, py_texture_bind_doc},
    {"unbind",
     (PyCFunction)py_texture_unbind,
     METH_VARARGS | METH_KEYWORDS,
     py_texture_unbind_doc},
    {"free", (PyCFunction)py_texture_free, METH_NOARGS, py_texture_free_doc},
    {"clear", (PyCFunction)py_texture_clear, METH_VARARGS, py_texture_clear_doc},
    {"__enter__", (PyCFunction)py_texture_bind_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)py_texture_bind_context_exit, METH_VARARGS},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(py_texture_doc,
             ".. class:: GPUTexture(width, height, data_type)\n"
             "\n"
             "   This object gives access to off screen buffers.\n"
             "\n"
             "   :arg width: Horizontal dimension of the buffer.\n"
             "   :type width: `int`\n"
             "   :arg height: Vertical dimension of the buffer.\n"
             "   :type height: `int`\n"
             "   :param type: One of these primitive types: {\n"
             "      `RGBA8UI`,\n"
             "      `RGBA8I`,\n"
             "      `RGBA8`,\n"
             "      `RGBA32UI`,\n"
             "      `RGBA32I`,\n"
             "      `RGBA32F`,\n"
             "      `RGBA16UI`,\n"
             "      `RGBA16I`,\n"
             "      `RGBA16F`,\n"
             "      `RGBA16`,\n"
             "      `RG8UI`,\n"
             "      `RG8I`,\n"
             "      `RG8`,\n"
             "      `RG32UI`,\n"
             "      `RG32I`,\n"
             "      `RG32F`,\n"
             "      `RG16UI`,\n"
             "      `RG16I`,\n"
             "      `RG16F`,\n"
             "      `RG16`,\n"
             "      `R8UI`,\n"
             "      `R8I`,\n"
             "      `R8`,\n"
             "      `R32UI`,\n"
             "      `R32I`,\n"
             "      `R32F`,\n"
             "      `R16UI`,\n"
             "      `R16I`,\n"
             "      `R16F`,\n"
             "      `R16`,\n"
             "      `R11F_G11F_B10F`,\n"
             "      `DEPTH32F_STENCIL8`,\n"
             "      `DEPTH24_STENCIL8`,\n"
             "      `SRGB8_A8`,\n"
             "      `RGB16F`,\n"
             "      `SRGB8_A8_DXT1`,\n"
             "      `SRGB8_A8_DXT3`,\n"
             "      `SRGB8_A8_DXT5`,\n"
             "      `RGBA8_DXT1`,\n"
             "      `RGBA8_DXT3`,\n"
             "      `RGBA8_DXT5`,\n"
             "      `DEPTH_COMPONENT32F`,\n"
             "      `DEPTH_COMPONENT24`,\n"
             "      `DEPTH_COMPONENT16`,\n"
             "   :type type: `str`\n");
PyTypeObject BPyGPUTexture_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUTexture",
    .tp_basicsize = sizeof(BPyGPUTexture),
    .tp_dealloc = (destructor)BPyGPUTexture__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = py_texture_doc,
    .tp_methods = py_texture_methods,
    .tp_getset = py_texture_getseters,
    .tp_new = py_texture_new,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUTexture_CreatePyObject(GPUTexture *tex)
{
  BPyGPUTexture *self;

  self = PyObject_New(BPyGPUTexture, &BPyGPUTexture_Type);
  self->tex = tex;

  return (PyObject *)self;
}

/** \} */

#undef PY_TEXTURE_CHECK_OBJ
