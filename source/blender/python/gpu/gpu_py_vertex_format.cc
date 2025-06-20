/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "gpu_py.hh"
#include "gpu_py_vertex_format.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Enum Conversion
 *
 * Use with PyArg_ParseTuple's "O&" formatting.
 * \{ */

static PyC_StringEnumItems pygpu_vertcomptype_items[] = {
    {GPU_COMP_I8, "I8"},
    {GPU_COMP_U8, "U8"},
    {GPU_COMP_I16, "I16"},
    {GPU_COMP_U16, "U16"},
    {GPU_COMP_I32, "I32"},
    {GPU_COMP_U32, "U32"},
    {GPU_COMP_F32, "F32"},
    {GPU_COMP_I10, "I10"},
    {0, nullptr},
};

static PyC_StringEnumItems pygpu_vertfetchmode_items[] = {
    {GPU_FETCH_FLOAT, "FLOAT"},
    {GPU_FETCH_INT, "INT"},
    {GPU_FETCH_INT_TO_FLOAT_UNIT, "INT_TO_FLOAT_UNIT"},
    {0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name VertFormat Type
 * \{ */

static PyObject *pygpu_vertformat__tp_new(PyTypeObject * /*type*/, PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  if (PyTuple_GET_SIZE(args) || (kwds && PyDict_Size(kwds))) {
    PyErr_SetString(PyExc_ValueError, "This function takes no arguments");
    return nullptr;
  }
  return BPyGPUVertFormat_CreatePyObject(nullptr);
}

static uint attr_size(GPUVertCompType type, int len)
{
  if (type == GPU_COMP_I10) {
    return 4; /* Always packed as 10_10_10_2. */
  }
  BLI_assert(type <= GPU_COMP_F32); /* Other types have irregular sizes (not bytes). */
  const uint sizes[] = {1, 1, 2, 2, 4, 4, 4};
  return len * sizes[type];
}

PyDoc_STRVAR(
    /* Wrap. */
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
    "      Possible values are `FLOAT`, `INT` or `INT_TO_FLOAT_UNIT`.\n"
    "   :type fetch_mode: str\n");
static PyObject *pygpu_vertformat_attr_add(BPyGPUVertFormat *self, PyObject *args, PyObject *kwds)
{
  const char *id;
  uint len;
  PyC_StringEnum comp_type = {pygpu_vertcomptype_items, GPU_COMP_I8};
  PyC_StringEnum fetch_mode = {pygpu_vertfetchmode_items, GPU_FETCH_FLOAT};

  if (self->fmt.attr_len == GPU_VERT_ATTR_MAX_LEN) {
    PyErr_SetString(PyExc_ValueError, "Maximum attr reached " STRINGIFY(GPU_VERT_ATTR_MAX_LEN));
    return nullptr;
  }

  static const char *_keywords[] = {"id", "comp_type", "len", "fetch_mode", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "$"  /* Keyword only arguments. */
      "s"  /* `id` */
      "O&" /* `comp_type` */
      "I"  /* `len` */
      "O&" /* `fetch_mode` */
      ":attr_add",
      _keywords,
      nullptr,
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
    return nullptr;
  }

  GPUVertCompType comp_type_enum = GPUVertCompType(comp_type.value_found);
  GPUVertFetchMode fetch_mode_enum = GPUVertFetchMode(fetch_mode.value_found);

  if (len > 4) {
    PyErr_WarnEx(
        PyExc_DeprecationWarning,
        "Using GPUVertFormat.attr_add(...) with component count greater than 4 is deprecated. "
        "Use several attributes for each matrix columns instead.",
        1);
  }

  if (attr_size(comp_type_enum, len) % 4 != 0) {
    PyErr_WarnEx(PyExc_DeprecationWarning,
                 "Using GPUVertFormat.attr_add(...) with a format that is not 4 bytes aligned is "
                 "deprecated. Add padding components and/or higher precision integers.",
                 1);
  }

  uint attr_id = GPU_vertformat_attr_add_legacy(
      &self->fmt, id, comp_type_enum, len, fetch_mode_enum);

  return PyLong_FromLong(attr_id);
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef pygpu_vertformat__tp_methods[] = {
    {"attr_add",
     (PyCFunction)pygpu_vertformat_attr_add,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_vertformat_attr_add_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static void pygpu_vertformat__tp_dealloc(BPyGPUVertFormat *self)
{
  Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_vertformat__tp_doc,
    ".. class:: GPUVertFormat()\n"
    "\n"
    "   This object contains information about the structure of a vertex buffer.\n");
PyTypeObject BPyGPUVertFormat_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GPUVertFormat",
    /*tp_basicsize*/ sizeof(BPyGPUVertFormat),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)pygpu_vertformat__tp_dealloc,
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
    /*tp_doc*/ pygpu_vertformat__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pygpu_vertformat__tp_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pygpu_vertformat__tp_new,
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
