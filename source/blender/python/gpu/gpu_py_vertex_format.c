/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

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
    "   :arg id: Name the attribute. Often `position`, `normal`, ...\n"
    "   :type id: str\n"
    "   :arg comp_type: The data type that will be used store the value in memory.\n"
    "      Possible values are `I8`, `U8`, `I16`, `U16`, `I32`, `U32`, `F32` and `I10`.\n"
    "   :type comp_type: str\n"
    "   :arg len: How many individual values the attribute consists of\n"
    "      (e.g. 2 for uv coordinates).\n"
    "   :type len: int\n"
    "   :arg fetch_mode: How values from memory will be converted when used in the shader.\n"
    "      This is mainly useful for memory optimizations when you want to store values with\n"
    "      reduced precision. E.g. you can store a float in only 1 byte but it will be\n"
    "      converted to a normal 4 byte float when used.\n"
    "      Possible values are `FLOAT`, `INT`, `INT_TO_FLOAT_UNIT` and `INT_TO_FLOAT`.\n"
    "   :type fetch_mode: str\n");
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
  static _PyArg_Parser _parser = {
      "$"  /* Keyword only arguments. */
      "s"  /* `id` */
      "O&" /* `comp_type` */
      "I"  /* `len` */
      "O&" /* `fetch_mode` */
      ":attr_add",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &id,
                                        PyC_ParseStringEnum,
                                        &comp_type,
                                        &len,
                                        PyC_ParseStringEnum,
                                        &fetch_mode))
  {
    return NULL;
  }

  uint attr_id = GPU_vertformat_attr_add(
      &self->fmt, id, comp_type.value_found, len, fetch_mode.value_found);
  return PyLong_FromLong(attr_id);
}

static PyMethodDef pygpu_vertformat__tp_methods[] = {
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
    /*ob_base*/ PyVarObject_HEAD_INIT(NULL, 0)
    /*tp_name*/ "GPUVertFormat",
    /*tp_basicsize*/ sizeof(BPyGPUVertFormat),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_vertformat__tp_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ NULL,
    /*tp_setattr*/ NULL,
    /*tp_as_async*/ NULL,
    /*tp_repr*/ NULL,
    /*tp_as_number*/ NULL,
    /*tp_as_sequence*/ NULL,
    /*tp_as_mapping*/ NULL,
    /*tp_hash*/ NULL,
    /*tp_call*/ NULL,
    /*tp_str*/ NULL,
    /*tp_getattro*/ NULL,
    /*tp_setattro*/ NULL,
    /*tp_as_buffer*/ NULL,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ pygpu_vertformat__tp_doc,
    /*tp_traverse*/ NULL,
    /*tp_clear*/ NULL,
    /*tp_richcompare*/ NULL,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ NULL,
    /*tp_iternext*/ NULL,
    /*tp_methods*/ pygpu_vertformat__tp_methods,
    /*tp_members*/ NULL,
    /*tp_getset*/ NULL,
    /*tp_base*/ NULL,
    /*tp_dict*/ NULL,
    /*tp_descr_get*/ NULL,
    /*tp_descr_set*/ NULL,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ NULL,
    /*tp_alloc*/ NULL,
    /*tp_new*/ pygpu_vertformat__tp_new,
    /*tp_free*/ NULL,
    /*tp_is_gc*/ NULL,
    /*tp_bases*/ NULL,
    /*tp_mro*/ NULL,
    /*tp_cache*/ NULL,
    /*tp_subclasses*/ NULL,
    /*tp_weaklist*/ NULL,
    /*tp_del*/ NULL,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ NULL,
    /*tp_vectorcall*/ NULL,
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
