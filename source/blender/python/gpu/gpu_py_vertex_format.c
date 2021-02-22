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
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "gpu_py_vertex_format.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Enum Conversion
 *
 * Use with PyArg_ParseTuple's "O&" formatting.
 * \{ */

static struct PyC_StringEnumItems pygpu_vertcomptype_items[] = {
    {GPU_COMP_I8, "I8"},
    {GPU_COMP_U8, "U8"},
    {GPU_COMP_I16, "I16"},
    {GPU_COMP_U16, "U16"},
    {GPU_COMP_I32, "I32"},
    {GPU_COMP_U32, "U32"},
    {GPU_COMP_F32, "F32"},
    {GPU_COMP_I10, "I10"},
    {0, NULL},
};

static struct PyC_StringEnumItems pygpu_vertfetchmode_items[] = {
    {GPU_FETCH_FLOAT, "FLOAT"},
    {GPU_FETCH_INT, "INT"},
    {GPU_FETCH_INT_TO_FLOAT_UNIT, "INT_TO_FLOAT_UNIT"},
    {GPU_FETCH_INT_TO_FLOAT, "INT_TO_FLOAT"},
    {0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name VertFormat Type
 * \{ */

static PyObject *pygpu_vertformat__tp_new(PyTypeObject *UNUSED(type),
                                          PyObject *args,
                                          PyObject *kwds)
{
  if (PyTuple_GET_SIZE(args) || (kwds && PyDict_Size(kwds))) {
    PyErr_SetString(PyExc_ValueError, "This function takes no arguments");
    return NULL;
  }
  return BPyGPUVertFormat_CreatePyObject(NULL);
}

PyDoc_STRVAR(
    pygpu_vertformat_attr_add_doc,
    ".. method:: attr_add(id, comp_type, len, fetch_mode)\n"
    "\n"
    "   Add a new attribute to the format.\n"
    "\n"
    "   :param id: Name the attribute. Often `position`, `normal`, ...\n"
    "   :type id: str\n"
    "   :param comp_type: The data type that will be used store the value in memory.\n"
    "      Possible values are `I8`, `U8`, `I16`, `U16`, `I32`, `U32`, `F32` and `I10`.\n"
    "   :type comp_type: `str`\n"
    "   :param len: How many individual values the attribute consists of\n"
    "      (e.g. 2 for uv coordinates).\n"
    "   :type len: int\n"
    "   :param fetch_mode: How values from memory will be converted when used in the shader.\n"
    "      This is mainly useful for memory optimizations when you want to store values with\n"
    "      reduced precision. E.g. you can store a float in only 1 byte but it will be\n"
    "      converted to a normal 4 byte float when used.\n"
    "      Possible values are `FLOAT`, `INT`, `INT_TO_FLOAT_UNIT` and `INT_TO_FLOAT`.\n"
    "   :type fetch_mode: `str`\n");
static PyObject *pygpu_vertformat_attr_add(BPyGPUVertFormat *self, PyObject *args, PyObject *kwds)
{
  const char *id;
  uint len;
  struct PyC_StringEnum comp_type = {pygpu_vertcomptype_items, GPU_COMP_I8};
  struct PyC_StringEnum fetch_mode = {pygpu_vertfetchmode_items, GPU_FETCH_FLOAT};

  if (self->fmt.attr_len == GPU_VERT_ATTR_MAX_LEN) {
    PyErr_SetString(PyExc_ValueError, "Maximum attr reached " STRINGIFY(GPU_VERT_ATTR_MAX_LEN));
    return NULL;
  }

  static const char *_keywords[] = {"id", "comp_type", "len", "fetch_mode", NULL};
  static _PyArg_Parser _parser = {"$sO&IO&:attr_add", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &id,
                                        PyC_ParseStringEnum,
                                        &comp_type,
                                        &len,
                                        PyC_ParseStringEnum,
                                        &fetch_mode)) {
    return NULL;
  }

  uint attr_id = GPU_vertformat_attr_add(
      &self->fmt, id, comp_type.value_found, len, fetch_mode.value_found);
  return PyLong_FromLong(attr_id);
}

static struct PyMethodDef pygpu_vertformat__tp_methods[] = {
    {"attr_add",
     (PyCFunction)pygpu_vertformat_attr_add,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_vertformat_attr_add_doc},
    {NULL, NULL, 0, NULL},
};

static void pygpu_vertformat__tp_dealloc(BPyGPUVertFormat *self)
{
  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pygpu_vertformat__tp_doc,
             ".. class:: GPUVertFormat()\n"
             "\n"
             "   This object contains information about the structure of a vertex buffer.\n");
PyTypeObject BPyGPUVertFormat_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUVertFormat",
    .tp_basicsize = sizeof(BPyGPUVertFormat),
    .tp_dealloc = (destructor)pygpu_vertformat__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = pygpu_vertformat__tp_doc,
    .tp_methods = pygpu_vertformat__tp_methods,
    .tp_new = pygpu_vertformat__tp_new,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUVertFormat_CreatePyObject(GPUVertFormat *fmt)
{
  BPyGPUVertFormat *self;

  self = PyObject_New(BPyGPUVertFormat, &BPyGPUVertFormat_Type);
  if (fmt) {
    self->fmt = *fmt;
  }
  else {
    memset(&self->fmt, 0, sizeof(self->fmt));
  }

  return (PyObject *)self;
}

/** \} */
