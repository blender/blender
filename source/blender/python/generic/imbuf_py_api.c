/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * This file defines the 'imbuf' image manipulation module.
 */

#include <Python.h>

#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "py_capi_utils.h"

#include "python_utildefines.h"

#include "imbuf_py_api.h" /* own include */

#include "../../imbuf/IMB_imbuf.h"
#include "../../imbuf/IMB_imbuf_types.h"

/* File IO */
#include "BLI_fileops.h"
#include <errno.h>
#include <fcntl.h>

static PyObject *BPyInit_imbuf_types(void);

static PyObject *Py_ImBuf_CreatePyObject(ImBuf *ibuf);

/* -------------------------------------------------------------------- */
/** \name Type & Utilities
 * \{ */

typedef struct Py_ImBuf {
  PyObject_VAR_HEAD
  /* can be NULL */
  ImBuf *ibuf;
} Py_ImBuf;

static int py_imbuf_valid_check(Py_ImBuf *self)
{
  if (LIKELY(self->ibuf)) {
    return 0;
  }

  PyErr_Format(
      PyExc_ReferenceError, "ImBuf data of type %.200s has been freed", Py_TYPE(self)->tp_name);
  return -1;
}

#define PY_IMBUF_CHECK_OBJ(obj) \
  if (UNLIKELY(py_imbuf_valid_check(obj) == -1)) { \
    return NULL; \
  } \
  ((void)0)
#define PY_IMBUF_CHECK_INT(obj) \
  if (UNLIKELY(py_imbuf_valid_check(obj) == -1)) { \
    return -1; \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Methods
 * \{ */

PyDoc_STRVAR(py_imbuf_resize_doc,
             ".. method:: resize(size, method='FAST')\n"
             "\n"
             "   Resize the image.\n"
             "\n"
             "   :arg size: New size.\n"
             "   :type size: pair of ints\n"
             "   :arg method: Method of resizing ('FAST', 'BILINEAR')\n"
             "   :type method: str\n");
static PyObject *py_imbuf_resize(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);

  int size[2];

  enum { FAST, BILINEAR };
  const struct PyC_StringEnumItems method_items[] = {
      {FAST, "FAST"},
      {BILINEAR, "BILINEAR"},
      {0, NULL},
  };
  struct PyC_StringEnum method = {method_items, FAST};

  static const char *_keywords[] = {"size", "method", NULL};
  static _PyArg_Parser _parser = {
      "(ii)" /* `size` */
      "|$"   /* Optional keyword only arguments. */
      "O&"   /* `method` */
      ":resize",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &size[0], &size[1], PyC_ParseStringEnum, &method))
  {
    return NULL;
  }
  if (size[0] <= 0 || size[1] <= 0) {
    PyErr_Format(PyExc_ValueError, "resize: Image size cannot be below 1 (%d, %d)", UNPACK2(size));
    return NULL;
  }

  if (method.value_found == FAST) {
    IMB_scalefastImBuf(self->ibuf, UNPACK2(size));
  }
  else if (method.value_found == BILINEAR) {
    IMB_scaleImBuf(self->ibuf, UNPACK2(size));
  }
  else {
    BLI_assert_unreachable();
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_imbuf_crop_doc,
             ".. method:: crop(min, max)\n"
             "\n"
             "   Crop the image.\n"
             "\n"
             "   :arg min: X, Y minimum.\n"
             "   :type min: pair of ints\n"
             "   :arg max: X, Y maximum.\n"
             "   :type max: pair of ints\n");
static PyObject *py_imbuf_crop(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);

  rcti crop;

  static const char *_keywords[] = {"min", "max", NULL};
  static _PyArg_Parser _parser = {
      "(II)" /* `min` */
      "(II)" /* `max` */
      ":crop",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &crop.xmin, &crop.ymin, &crop.xmax, &crop.ymax))
  {
    return NULL;
  }

  if (/* X range. */
      !(crop.xmin >= 0 && crop.xmax < self->ibuf->x) ||
      /* Y range. */
      !(crop.ymin >= 0 && crop.ymax < self->ibuf->y) ||
      /* X order. */
      !(crop.xmin <= crop.xmax) ||
      /* Y order. */
      !(crop.ymin <= crop.ymax))
  {
    PyErr_SetString(PyExc_ValueError, "ImBuf crop min/max not in range");
    return NULL;
  }
  IMB_rect_crop(self->ibuf, &crop);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_imbuf_copy_doc,
             ".. method:: copy()\n"
             "\n"
             "   :return: A copy of the image.\n"
             "   :rtype: :class:`ImBuf`\n");
static PyObject *py_imbuf_copy(Py_ImBuf *self)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf_copy = IMB_dupImBuf(self->ibuf);

  if (UNLIKELY(ibuf_copy == NULL)) {
    PyErr_SetString(PyExc_MemoryError,
                    "ImBuf.copy(): "
                    "failed to allocate memory");
    return NULL;
  }
  return Py_ImBuf_CreatePyObject(ibuf_copy);
}

static PyObject *py_imbuf_deepcopy(Py_ImBuf *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return NULL;
  }
  return py_imbuf_copy(self);
}

PyDoc_STRVAR(py_imbuf_free_doc,
             ".. method:: free()\n"
             "\n"
             "   Clear image data immediately (causing an error on re-use).\n");
static PyObject *py_imbuf_free(Py_ImBuf *self)
{
  if (self->ibuf) {
    IMB_freeImBuf(self->ibuf);
    self->ibuf = NULL;
  }
  Py_RETURN_NONE;
}

static PyMethodDef Py_ImBuf_methods[] = {
    {"resize", (PyCFunction)py_imbuf_resize, METH_VARARGS | METH_KEYWORDS, py_imbuf_resize_doc},
    {"crop", (PyCFunction)py_imbuf_crop, METH_VARARGS | METH_KEYWORDS, (char *)py_imbuf_crop_doc},
    {"free", (PyCFunction)py_imbuf_free, METH_NOARGS, py_imbuf_free_doc},
    {"copy", (PyCFunction)py_imbuf_copy, METH_NOARGS, py_imbuf_copy_doc},
    {"__copy__", (PyCFunction)py_imbuf_copy, METH_NOARGS, py_imbuf_copy_doc},
    {"__deepcopy__", (PyCFunction)py_imbuf_deepcopy, METH_VARARGS, py_imbuf_copy_doc},
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attributes
 * \{ */

PyDoc_STRVAR(py_imbuf_size_doc, "size of the image in pixels.\n\n:type: pair of ints");
static PyObject *py_imbuf_size_get(Py_ImBuf *self, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf = self->ibuf;
  return PyC_Tuple_Pack_I32(ibuf->x, ibuf->y);
}

PyDoc_STRVAR(py_imbuf_ppm_doc, "pixels per meter.\n\n:type: pair of floats");
static PyObject *py_imbuf_ppm_get(Py_ImBuf *self, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf = self->ibuf;
  return PyC_Tuple_Pack_F64(ibuf->ppm[0], ibuf->ppm[1]);
}

static int py_imbuf_ppm_set(Py_ImBuf *self, PyObject *value, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_INT(self);
  double ppm[2];

  if (PyC_AsArray(ppm, sizeof(*ppm), value, 2, &PyFloat_Type, "ppm") == -1) {
    return -1;
  }

  if (ppm[0] <= 0.0 || ppm[1] <= 0.0) {
    PyErr_SetString(PyExc_ValueError, "invalid ppm value");
    return -1;
  }

  ImBuf *ibuf = self->ibuf;
  ibuf->ppm[0] = ppm[0];
  ibuf->ppm[1] = ppm[1];
  return 0;
}

PyDoc_STRVAR(py_imbuf_filepath_doc, "filepath associated with this image.\n\n:type: string");
static PyObject *py_imbuf_filepath_get(Py_ImBuf *self, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf = self->ibuf;
  return PyC_UnicodeFromBytes(ibuf->filepath);
}

static int py_imbuf_filepath_set(Py_ImBuf *self, PyObject *value, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_INT(self);

  if (!PyUnicode_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "expected a string!");
    return -1;
  }

  ImBuf *ibuf = self->ibuf;
  const Py_ssize_t value_str_len_max = sizeof(ibuf->filepath);
  Py_ssize_t value_str_len;
  const char *value_str = PyUnicode_AsUTF8AndSize(value, &value_str_len);
  if (value_str_len >= value_str_len_max) {
    PyErr_Format(PyExc_TypeError, "filepath length over %zd", value_str_len_max - 1);
    return -1;
  }
  memcpy(ibuf->filepath, value_str, value_str_len + 1);
  return 0;
}

PyDoc_STRVAR(py_imbuf_planes_doc, "Number of bits associated with this image.\n\n:type: int");
static PyObject *py_imbuf_planes_get(Py_ImBuf *self, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *imbuf = self->ibuf;
  return PyLong_FromLong(imbuf->planes);
}

PyDoc_STRVAR(py_imbuf_channels_doc, "Number of bit-planes.\n\n:type: int");
static PyObject *py_imbuf_channels_get(Py_ImBuf *self, void *UNUSED(closure))
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *imbuf = self->ibuf;
  return PyLong_FromLong(imbuf->channels);
}

static PyGetSetDef Py_ImBuf_getseters[] = {
    {"size", (getter)py_imbuf_size_get, (setter)NULL, py_imbuf_size_doc, NULL},
    {"ppm", (getter)py_imbuf_ppm_get, (setter)py_imbuf_ppm_set, py_imbuf_ppm_doc, NULL},
    {"filepath",
     (getter)py_imbuf_filepath_get,
     (setter)py_imbuf_filepath_set,
     py_imbuf_filepath_doc,
     NULL},
    {"planes", (getter)py_imbuf_planes_get, NULL, py_imbuf_planes_doc, NULL},
    {"channels", (getter)py_imbuf_channels_get, NULL, py_imbuf_channels_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type & Implementation
 * \{ */

static void py_imbuf_dealloc(Py_ImBuf *self)
{
  ImBuf *ibuf = self->ibuf;
  if (ibuf != NULL) {
    IMB_freeImBuf(self->ibuf);
    self->ibuf = NULL;
  }
  PyObject_DEL(self);
}

static PyObject *py_imbuf_repr(Py_ImBuf *self)
{
  const ImBuf *ibuf = self->ibuf;
  if (ibuf != NULL) {
    return PyUnicode_FromFormat("<imbuf: address=%p, filepath='%s', size=(%d, %d)>",
                                ibuf,
                                ibuf->filepath,
                                ibuf->x,
                                ibuf->y);
  }

  return PyUnicode_FromString("<imbuf: address=0x0>");
}

static Py_hash_t py_imbuf_hash(Py_ImBuf *self)
{
  return _Py_HashPointer(self->ibuf);
}

PyTypeObject Py_ImBuf_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    /*tp_name*/ "ImBuf",
    /*tp_basicsize*/ sizeof(Py_ImBuf),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)py_imbuf_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ NULL,
    /*tp_setattr*/ NULL,
    /*tp_as_async*/ NULL,
    /*tp_repr*/ (reprfunc)py_imbuf_repr,
    /*tp_as_number*/ NULL,
    /*tp_as_sequence*/ NULL,
    /*tp_as_mapping*/ NULL,
    /*tp_hash*/ (hashfunc)py_imbuf_hash,
    /*tp_call*/ NULL,
    /*tp_str*/ NULL,
    /*tp_getattro*/ NULL,
    /*tp_setattro*/ NULL,
    /*tp_as_buffer*/ NULL,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ NULL,
    /*tp_traverse*/ NULL,
    /*tp_clear*/ NULL,
    /*tp_richcompare*/ NULL,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ NULL,
    /*tp_iternext*/ NULL,
    /*tp_methods*/ Py_ImBuf_methods,
    /*tp_members*/ NULL,
    /*tp_getset*/ Py_ImBuf_getseters,
    /*tp_base*/ NULL,
    /*tp_dict*/ NULL,
    /*tp_descr_get*/ NULL,
    /*tp_descr_set*/ NULL,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ NULL,
    /*tp_alloc*/ NULL,
    /*tp_new*/ NULL,
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

static PyObject *Py_ImBuf_CreatePyObject(ImBuf *ibuf)
{
  Py_ImBuf *self = PyObject_New(Py_ImBuf, &Py_ImBuf_Type);
  self->ibuf = ibuf;
  return (PyObject *)self;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Functions
 * \{ */

PyDoc_STRVAR(M_imbuf_new_doc,
             ".. function:: new(size)\n"
             "\n"
             "   Load a new image.\n"
             "\n"
             "   :arg size: The size of the image in pixels.\n"
             "   :type size: pair of ints\n"
             "   :return: the newly loaded image.\n"
             "   :rtype: :class:`ImBuf`\n");
static PyObject *M_imbuf_new(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  int size[2];
  static const char *_keywords[] = {"size", NULL};
  static _PyArg_Parser _parser = {
      "(ii)" /* `size` */
      ":new",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &size[0], &size[1])) {
    return NULL;
  }
  if (size[0] <= 0 || size[1] <= 0) {
    PyErr_Format(PyExc_ValueError, "new: Image size cannot be below 1 (%d, %d)", UNPACK2(size));
    return NULL;
  }

  /* TODO: make options. */
  const uchar planes = 4;
  const uint flags = IB_rect;

  ImBuf *ibuf = IMB_allocImBuf(UNPACK2(size), planes, flags);
  if (ibuf == NULL) {
    PyErr_Format(PyExc_ValueError, "new: Unable to create image (%d, %d)", UNPACK2(size));
    return NULL;
  }
  return Py_ImBuf_CreatePyObject(ibuf);
}

PyDoc_STRVAR(M_imbuf_load_doc,
             ".. function:: load(filepath)\n"
             "\n"
             "   Load an image from a file.\n"
             "\n"
             "   :arg filepath: the filepath of the image.\n"
             "   :type filepath: string\n"
             "   :return: the newly loaded image.\n"
             "   :rtype: :class:`ImBuf`\n");
static PyObject *M_imbuf_load(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  const char *filepath;

  static const char *_keywords[] = {"filepath", NULL};
  static _PyArg_Parser _parser = {
      "s" /* `filepath` */
      ":load",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &filepath)) {
    return NULL;
  }

  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    PyErr_Format(PyExc_IOError, "load: %s, failed to open file '%s'", strerror(errno), filepath);
    return NULL;
  }

  ImBuf *ibuf = IMB_loadifffile(file, IB_rect, NULL, filepath);

  close(file);

  if (ibuf == NULL) {
    PyErr_Format(
        PyExc_ValueError, "load: Unable to recognize image format for file '%s'", filepath);
    return NULL;
  }

  STRNCPY(ibuf->filepath, filepath);

  return Py_ImBuf_CreatePyObject(ibuf);
}

PyDoc_STRVAR(
    M_imbuf_write_doc,
    ".. function:: write(image, filepath=image.filepath)\n"
    "\n"
    "   Write an image.\n"
    "\n"
    "   :arg image: the image to write.\n"
    "   :type image: :class:`ImBuf`\n"
    "   :arg filepath: Optional filepath of the image (fallback to the images file path).\n"
    "   :type filepath: string\n");
static PyObject *M_imbuf_write(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  Py_ImBuf *py_imb;
  const char *filepath = NULL;

  static const char *_keywords[] = {"image", "filepath", NULL};
  static _PyArg_Parser _parser = {
      "O!" /* `image` */
      "|$" /* Optional keyword only arguments. */
      "s"  /* `filepath` */
      ":write",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &Py_ImBuf_Type, &py_imb, &filepath)) {
    return NULL;
  }

  if (filepath == NULL) {
    filepath = py_imb->ibuf->filepath;
  }

  const bool ok = IMB_saveiff(py_imb->ibuf, filepath, IB_rect);
  if (ok == false) {
    PyErr_Format(
        PyExc_IOError, "write: Unable to write image file (%s) '%s'", strerror(errno), filepath);
    return NULL;
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Definition (`imbuf`)
 * \{ */

static PyMethodDef IMB_methods[] = {
    {"new", (PyCFunction)M_imbuf_new, METH_VARARGS | METH_KEYWORDS, M_imbuf_new_doc},
    {"load", (PyCFunction)M_imbuf_load, METH_VARARGS | METH_KEYWORDS, M_imbuf_load_doc},
    {"write", (PyCFunction)M_imbuf_write, METH_VARARGS | METH_KEYWORDS, M_imbuf_write_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(IMB_doc,
             "This module provides access to Blender's image manipulation API.\n"
             "\n"
             "It provides access to image buffers outside of Blender's\n"
             ":class:`bpy.types.Image` data-block context.\n");
static PyModuleDef IMB_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "imbuf",
    /*m_doc*/ IMB_doc,
    /*m_size*/ 0,
    /*m_methods*/ IMB_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *BPyInit_imbuf(void)
{
  PyObject *mod;
  PyObject *submodule;
  PyObject *sys_modules = PyImport_GetModuleDict();

  mod = PyModule_Create(&IMB_module_def);

  /* `imbuf.types` */
  PyModule_AddObject(mod, "types", (submodule = BPyInit_imbuf_types()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  return mod;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Definition (`imbuf.types`)
 *
 * `imbuf.types` module, only include this to expose access to `imbuf.types.ImBuf`
 * for docs and the ability to use with built-ins such as `isinstance`, `issubclass`.
 * \{ */

PyDoc_STRVAR(IMB_types_doc,
             "This module provides access to image buffer types.\n"
             "\n"
             ".. note::\n"
             "\n"
             "   Image buffer is also the structure used by :class:`bpy.types.Image`\n"
             "   ID type to store and manipulate image data at runtime.\n");

static PyModuleDef IMB_types_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "imbuf.types",
    /*m_doc*/ IMB_types_doc,
    /*m_size*/ 0,
    /*m_methods*/ NULL,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *BPyInit_imbuf_types(void)
{
  PyObject *submodule = PyModule_Create(&IMB_types_module_def);

  if (PyType_Ready(&Py_ImBuf_Type) < 0) {
    return NULL;
  }

  PyModule_AddType(submodule, &Py_ImBuf_Type);

  return submodule;
}

/** \} */
