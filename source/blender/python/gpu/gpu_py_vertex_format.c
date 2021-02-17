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

#ifdef __BIG_ENDIAN__
/* big endian */
#  define MAKE_ID2(c, d) ((c) << 8 | (d))
#  define MAKE_ID3(a, b, c) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8)
#  define MAKE_ID4(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* little endian  */
#  define MAKE_ID2(c, d) ((d) << 8 | (c))
#  define MAKE_ID3(a, b, c) ((int)(c) << 16 | (b) << 8 | (a))
#  define MAKE_ID4(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif

/* -------------------------------------------------------------------- */
/** \name Enum Conversion
 *
 * Use with PyArg_ParseTuple's "O&" formatting.
 * \{ */

static int pygpu_vertformat_parse_component_type(const char *str, int length)
{
  if (length == 2) {
    switch (*((ushort *)str)) {
      case MAKE_ID2('I', '8'):
        return GPU_COMP_I8;
      case MAKE_ID2('U', '8'):
        return GPU_COMP_U8;
      default:
        break;
    }
  }
  else if (length == 3) {
    switch (*((uint *)str)) {
      case MAKE_ID3('I', '1', '6'):
        return GPU_COMP_I16;
      case MAKE_ID3('U', '1', '6'):
        return GPU_COMP_U16;
      case MAKE_ID3('I', '3', '2'):
        return GPU_COMP_I32;
      case MAKE_ID3('U', '3', '2'):
        return GPU_COMP_U32;
      case MAKE_ID3('F', '3', '2'):
        return GPU_COMP_F32;
      case MAKE_ID3('I', '1', '0'):
        return GPU_COMP_I10;
      default:
        break;
    }
  }
  return -1;
}

static int pygpu_vertformat_parse_fetch_mode(const char *str, int length)
{
#define MATCH_ID(id) \
  if (length == strlen(STRINGIFY(id))) { \
    if (STREQ(str, STRINGIFY(id))) { \
      return GPU_FETCH_##id; \
    } \
  } \
  ((void)0)

  MATCH_ID(FLOAT);
  MATCH_ID(INT);
  MATCH_ID(INT_TO_FLOAT_UNIT);
  MATCH_ID(INT_TO_FLOAT);
#undef MATCH_ID

  return -1;
}

static int pygpu_ParseVertCompType(PyObject *o, void *p)
{
  Py_ssize_t length;
  const char *str = PyUnicode_AsUTF8AndSize(o, &length);

  if (str == NULL) {
    PyErr_Format(PyExc_ValueError, "expected a string, got %s", Py_TYPE(o)->tp_name);
    return 0;
  }

  const int comp_type = pygpu_vertformat_parse_component_type(str, length);
  if (comp_type == -1) {
    PyErr_Format(PyExc_ValueError, "unknown component type: '%s", str);
    return 0;
  }

  *((GPUVertCompType *)p) = comp_type;
  return 1;
}

static int pygpu_ParseVertFetchMode(PyObject *o, void *p)
{
  Py_ssize_t length;
  const char *str = PyUnicode_AsUTF8AndSize(o, &length);

  if (str == NULL) {
    PyErr_Format(PyExc_ValueError, "expected a string, got %s", Py_TYPE(o)->tp_name);
    return 0;
  }

  const int fetch_mode = pygpu_vertformat_parse_fetch_mode(str, length);
  if (fetch_mode == -1) {
    PyErr_Format(PyExc_ValueError, "unknown type literal: '%s'", str);
    return 0;
  }

  (*(GPUVertFetchMode *)p) = fetch_mode;
  return 1;
}

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
  struct {
    const char *id;
    GPUVertCompType comp_type;
    uint len;
    GPUVertFetchMode fetch_mode;
  } params;

  if (self->fmt.attr_len == GPU_VERT_ATTR_MAX_LEN) {
    PyErr_SetString(PyExc_ValueError, "Maximum attr reached " STRINGIFY(GPU_VERT_ATTR_MAX_LEN));
    return NULL;
  }

  static const char *_keywords[] = {"id", "comp_type", "len", "fetch_mode", NULL};
  static _PyArg_Parser _parser = {"$sO&IO&:attr_add", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &params.id,
                                        pygpu_ParseVertCompType,
                                        &params.comp_type,
                                        &params.len,
                                        pygpu_ParseVertFetchMode,
                                        &params.fetch_mode)) {
    return NULL;
  }

  uint attr_id = GPU_vertformat_attr_add(
      &self->fmt, params.id, params.comp_type, params.len, params.fetch_mode);
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
