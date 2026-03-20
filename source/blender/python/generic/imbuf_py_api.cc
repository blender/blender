/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "py_capi_utils.hh"

#include "python_compat.hh" /* IWYU pragma: keep. */

#include "imbuf_py_api.hh" /* own include */

#include "../../imbuf/IMB_imbuf.hh"
#include "../../imbuf/IMB_imbuf_types.hh"

/* File IO */
#include "BLI_fileops.h"

#include <cerrno>
#include <fcntl.h>
#include <optional>

#define IMB_FTYPE_DEFAULT IMB_FTYPE_PNG

namespace blender {

static PyObject *BPyInit_imbuf_types();

static PyObject *Py_ImBuf_CreatePyObject(ImBuf *ibuf);

struct Py_ImBuf;

#define PY_IMBUF_BUFFER_TYPE_DOC \
  "   :param type: The buffer type.\n" \
  "   :type type: Literal['BYTE', 'FLOAT']\n"

static const PyC_StringEnumItems py_imbuf_buffer_mode_items[] = {
    {IB_byte_data, "BYTE"},
    {IB_float_data, "FLOAT"},
    {0, nullptr},
};

const static char *py_imbuf_type_none = "NONE";

struct Py_ImBufBuffer {
  PyObject_HEAD
  /** Reference to the #ImBuf this came from (prevents freeing while in use). */
  Py_ImBuf *py_ibuf;
  /** Whether this wraps byte/float pixel data (#IB_byte_data, #IB_float_data). */
  int mode;
  /** Set by `__enter__`, cleared by `__exit__` (managed #Py_ImBuf.buffer_users). */
  bool is_entered;
  /** When false the `memoryview` is read-only. */
  bool writable;
  /** Sub-region bounds (min inclusive, max exclusive). */
  std::optional<rcti> region;
};
extern PyTypeObject Py_ImBufBuffer_Type;

struct Py_ImBufFileType {
  PyObject_HEAD
  int ftype;
};
extern PyTypeObject Py_ImBufFileType_Type;

/* -------------------------------------------------------------------- */
/** \name File Type Info
 *
 * TODO: check on merging this into #ImFileType,
 * this is currently private, but could be extended to provide more useful info.
 * \{ */

static std::optional<int> py_imbuf_ftype_from_string(const char *str)
{
  const int ftype = IMB_ftype_from_id(str);
  if (ftype == IMB_FTYPE_NONE && !STREQ(str, py_imbuf_type_none)) {
    return std::nullopt;
  }
  return ftype;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type & Utilities
 * \{ */

struct Py_ImBuf {
  PyObject_HEAD
  /* can be nullptr */
  ImBuf *ibuf;
  /** Number of active buffer protocol exports (via #Py_ImBufBuffer). */
  int buffer_users;
};

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
    return nullptr; \
  } \
  ((void)0)
#define PY_IMBUF_CHECK_INT(obj) \
  if (UNLIKELY(py_imbuf_valid_check(obj) == -1)) { \
    return -1; \
  } \
  ((void)0)

#define PY_IMBUF_CHECK_BUFFER_USERS(obj) \
  if (UNLIKELY((obj)->buffer_users > 0)) { \
    PyErr_SetString(PyExc_BufferError, \
                    "ImBuf cannot be modified while pixel buffers are exported"); \
    return nullptr; \
  } \
  ((void)0)

static void py_imbuf_warn_corrupt_ftype(const int ftype)
{
  /* Should not be possible, but avoid crashing on corrupt data. */
  BLI_assert_unreachable();
  PyErr_WarnFormat(PyExc_RuntimeWarning, 1, "unknown file type enum: %d", ftype);
}

/**
 * Return the file type ID string, falling back to "NONE" so
 * the caller doesn't have to deal with the very unlikely case
 * of an unknown/corrupt `ftype`.
 */
static const char *py_imbuf_ftype_to_id_with_fallback(const int ftype)
{
  if (ftype == IMB_FTYPE_NONE) {
    return py_imbuf_type_none;
  }
  const char *id = IMB_ftype_to_id(ftype);
  if (UNLIKELY(id == nullptr)) {
    py_imbuf_warn_corrupt_ftype(ftype);
    return py_imbuf_type_none;
  }
  return id;
}

/**
 * Clamp a region to image bounds.
 *
 * \param ibuf: The image buffer (used for clamping).
 * \param region: Region to clamp in-place (inverted ranges are clamped to zero area).
 * \param r_use_region: Set to false when the region covers the entire image.
 */
static bool py_imbuf_region_sanitize(const ImBuf *ibuf, rcti *region)
{
  BLI_rcti_sanitize(region);

  CLAMP(region->xmin, 0, ibuf->x);
  CLAMP(region->xmax, 0, ibuf->x);
  CLAMP(region->ymin, 0, ibuf->y);
  CLAMP(region->ymax, 0, ibuf->y);

  return !((region->xmin == 0 && region->ymin == 0) &&
           (region->xmax == ibuf->x && region->ymax == ibuf->y));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Methods
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_resize_doc,
    ".. method:: resize(size, *, method='FAST')\n"
    "\n"
    "   Resize the image in-place.\n"
    "\n"
    "   :param size: New size.\n"
    "   :type size: tuple[int, int]\n"
    "   :param method: Method of resizing ('FAST', 'BILINEAR').\n"
    "   :type method: str\n");
static PyObject *py_imbuf_resize(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);
  PY_IMBUF_CHECK_BUFFER_USERS(self);

  int size[2];

  enum { FAST, BILINEAR };
  const PyC_StringEnumItems method_items[] = {
      {FAST, "FAST"},
      {BILINEAR, "BILINEAR"},
      {0, nullptr},
  };
  PyC_StringEnum method = {method_items, FAST};

  static const char *_keywords[] = {"size", "method", nullptr};
  static _PyArg_Parser _parser = {
      "(ii)" /* `size` */
      "|$"   /* Optional keyword only arguments. */
      "O&"   /* `method` */
      ":resize",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &size[0], &size[1], PyC_ParseStringEnum, &method))
  {
    return nullptr;
  }
  if (size[0] <= 0 || size[1] <= 0) {
    PyErr_Format(PyExc_ValueError, "resize: Image size cannot be below 1 (%d, %d)", UNPACK2(size));
    return nullptr;
  }

  if (method.value_found == FAST) {
    IMB_scale(self->ibuf, UNPACK2(size), IMBScaleFilter::Nearest, false);
  }
  else if (method.value_found == BILINEAR) {
    IMB_scale(self->ibuf, UNPACK2(size), IMBScaleFilter::Box, false);
  }
  else {
    BLI_assert_unreachable();
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_crop_doc,
    ".. method:: crop(min, max)\n"
    "\n"
    "   Crop the image in-place.\n"
    "\n"
    "   :param min: Minimum pixel coordinates (X, Y), inclusive.\n"
    "   :type min: tuple[int, int]\n"
    "   :param max: Maximum pixel coordinates (X, Y), inclusive.\n"
    "   :type max: tuple[int, int]\n");
static PyObject *py_imbuf_crop(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);
  PY_IMBUF_CHECK_BUFFER_USERS(self);

  rcti crop;

  static const char *_keywords[] = {"min", "max", nullptr};
  static _PyArg_Parser _parser = {
      "(II)" /* `min` */
      "(II)" /* `max` */
      ":crop",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &crop.xmin, &crop.ymin, &crop.xmax, &crop.ymax))
  {
    return nullptr;
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
    return nullptr;
  }
  IMB_rect_crop(self->ibuf, &crop);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_copy_doc,
    ".. method:: copy()\n"
    "\n"
    "   Return a copy of the image.\n"
    "\n"
    "   :return: A copy of the image.\n"
    "   :rtype: :class:`ImBuf`\n");
static PyObject *py_imbuf_copy(Py_ImBuf *self)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf_copy = IMB_dupImBuf(self->ibuf);

  if (UNLIKELY(ibuf_copy == nullptr)) {
    PyErr_SetString(PyExc_MemoryError,
                    "ImBuf.copy(): "
                    "failed to allocate memory");
    return nullptr;
  }
  return Py_ImBuf_CreatePyObject(ibuf_copy);
}

static PyObject *py_imbuf_deepcopy(Py_ImBuf *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return nullptr;
  }
  return py_imbuf_copy(self);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_with_buffer_doc,
    ".. method:: with_buffer(type, *, write=False, region=None)\n"
    "\n"
    "   Return a context manager that yields a :class:`memoryview` "
    "of the image's pixel data.\n"
    "\n"
    "   Usage::\n"
    "\n"
    "      with image.with_buffer('BYTE', write=True) as buf:\n"
    "          buf[0] = 255  # set red channel of first pixel\n"
    "\n" PY_IMBUF_BUFFER_TYPE_DOC
    "   :param write: When true the buffer is writable.\n"
    "   :type write: bool\n"
    "   :param region: Optional sub-region ``((x_min, y_min), (x_max, y_max))``.\n"
    "      When set the memoryview is 2D (rows x columns), "
    "clamped to image bounds.\n"
    "   :type region: tuple[tuple[int, int], tuple[int, int]] | None\n"
    "   :return: A context manager yielding a :class:`memoryview` of pixel data.\n"
    "   :rtype: :class:`ImBufBuffer`\n");
static PyObject *py_imbuf_with_buffer(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);

  PyC_StringEnum type = {py_imbuf_buffer_mode_items, -1};
  bool write = false;
  std::optional<rcti> region;

  static const char *_keywords[] = {
      "type",
      "write",
      "region",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      "O&" /* `type` (required) */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `write` */
      "O&" /* `region` */
      ":with_buffer",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseStringEnum,
                                        &type,
                                        PyC_ParseBool,
                                        &write,
                                        PyC_ParseOptionalRectI,
                                        &region))
  {
    return nullptr;
  }

  ImBuf *ibuf = self->ibuf;
  const int mode = type.value_found;

  if (mode == IB_byte_data) {
    if (ibuf->byte_buffer.data == nullptr) {
      PyErr_SetString(PyExc_RuntimeError, "ImBuf has no byte pixel data");
      return nullptr;
    }
  }
  else {
    if (ibuf->float_buffer.data == nullptr) {
      PyErr_SetString(PyExc_RuntimeError, "ImBuf has no float pixel data");
      return nullptr;
    }
  }

  bool use_region = false;
  if (region.has_value()) {
    use_region = py_imbuf_region_sanitize(ibuf, &region.value());
  }

  Py_ImBufBuffer *ctx = PyObject_GC_New(Py_ImBufBuffer, &Py_ImBufBuffer_Type);
  if (UNLIKELY(ctx == nullptr)) {
    return nullptr;
  }
  Py_INCREF(self);
  ctx->py_ibuf = self;
  ctx->is_entered = false;
  ctx->mode = mode;
  ctx->writable = write;
  ctx->region = use_region ? region : std::nullopt;
  return reinterpret_cast<PyObject *>(ctx);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_ensure_buffer_doc,
    ".. method:: ensure_buffer(type)\n"
    "\n"
    "   Ensure the image has pixel data of the given type.\n"
    "   If absent, it is allocated and converted from the other buffer when available.\n"
    "\n" PY_IMBUF_BUFFER_TYPE_DOC);
static PyObject *py_imbuf_ensure_buffer(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);
  PY_IMBUF_CHECK_BUFFER_USERS(self);

  PyC_StringEnum type = {py_imbuf_buffer_mode_items, -1};

  static const char *_keywords[] = {"type", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `type` (required) */
      ":ensure_buffer",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, PyC_ParseStringEnum, &type)) {
    return nullptr;
  }

  ImBuf *ibuf = self->ibuf;

  if (type.value_found == IB_byte_data) {
    if (ibuf->byte_buffer.data == nullptr) {
      if (ibuf->float_buffer.data != nullptr) {
        IMB_byte_from_float(ibuf);
      }
      else {
        IMB_alloc_byte_pixels(ibuf);
      }
      if (UNLIKELY(ibuf->byte_buffer.data == nullptr)) {
        PyErr_SetString(PyExc_MemoryError, "failed to allocate byte buffer");
        return nullptr;
      }
    }
  }
  else {
    if (ibuf->float_buffer.data == nullptr) {
      if (ibuf->byte_buffer.data != nullptr) {
        IMB_float_from_byte(ibuf);
      }
      else {
        IMB_alloc_float_pixels(ibuf, 4);
      }
      if (UNLIKELY(ibuf->float_buffer.data == nullptr)) {
        PyErr_SetString(PyExc_MemoryError, "failed to allocate float buffer");
        return nullptr;
      }
    }
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_has_buffer_doc,
    ".. method:: has_buffer(type)\n"
    "\n"
    "   Return whether the image has pixel data of the given type.\n"
    "\n" PY_IMBUF_BUFFER_TYPE_DOC
    "   :return: True if the buffer exists.\n"
    "   :rtype: bool\n");
static PyObject *py_imbuf_has_buffer(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);

  PyC_StringEnum type = {py_imbuf_buffer_mode_items, -1};

  static const char *_keywords[] = {"type", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `type` (required) */
      ":has_buffer",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, PyC_ParseStringEnum, &type)) {
    return nullptr;
  }

  ImBuf *ibuf = self->ibuf;
  if (type.value_found == IB_byte_data) {
    return PyBool_FromLong(ibuf->byte_buffer.data != nullptr);
  }
  return PyBool_FromLong(ibuf->float_buffer.data != nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_clear_buffer_doc,
    ".. method:: clear_buffer(type)\n"
    "\n"
    "   Free pixel data of the given type.\n"
    "\n" PY_IMBUF_BUFFER_TYPE_DOC);
static PyObject *py_imbuf_clear_buffer(Py_ImBuf *self, PyObject *args, PyObject *kw)
{
  PY_IMBUF_CHECK_OBJ(self);
  PY_IMBUF_CHECK_BUFFER_USERS(self);

  PyC_StringEnum type = {py_imbuf_buffer_mode_items, -1};

  static const char *_keywords[] = {"type", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `type` (required) */
      ":clear_buffer",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, PyC_ParseStringEnum, &type)) {
    return nullptr;
  }

  ImBuf *ibuf = self->ibuf;
  if (type.value_found == IB_byte_data) {
    IMB_free_byte_pixels(ibuf);
  }
  else {
    IMB_free_float_pixels(ibuf);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_free_doc,
    ".. method:: free()\n"
    "\n"
    "   Clear image data immediately (causing an error on re-use).\n");
static PyObject *py_imbuf_free(Py_ImBuf *self)
{
  PY_IMBUF_CHECK_BUFFER_USERS(self);
  if (self->ibuf) {
    IMB_freeImBuf(self->ibuf);
    self->ibuf = nullptr;
  }
  Py_RETURN_NONE;
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

static PyMethodDef Py_ImBuf_methods[] = {
    {"resize",
     reinterpret_cast<PyCFunction>(py_imbuf_resize),
     METH_VARARGS | METH_KEYWORDS,
     py_imbuf_resize_doc},
    {"crop",
     reinterpret_cast<PyCFunction>(py_imbuf_crop),
     METH_VARARGS | METH_KEYWORDS,
     const_cast<char *>(py_imbuf_crop_doc)},
    {"with_buffer",
     reinterpret_cast<PyCFunction>(py_imbuf_with_buffer),
     METH_VARARGS | METH_KEYWORDS,
     py_imbuf_with_buffer_doc},
    {"ensure_buffer",
     reinterpret_cast<PyCFunction>(py_imbuf_ensure_buffer),
     METH_VARARGS | METH_KEYWORDS,
     py_imbuf_ensure_buffer_doc},
    {"has_buffer",
     reinterpret_cast<PyCFunction>(py_imbuf_has_buffer),
     METH_VARARGS | METH_KEYWORDS,
     py_imbuf_has_buffer_doc},
    {"clear_buffer",
     reinterpret_cast<PyCFunction>(py_imbuf_clear_buffer),
     METH_VARARGS | METH_KEYWORDS,
     py_imbuf_clear_buffer_doc},
    {"free", reinterpret_cast<PyCFunction>(py_imbuf_free), METH_NOARGS, py_imbuf_free_doc},
    {"copy", reinterpret_cast<PyCFunction>(py_imbuf_copy), METH_NOARGS, py_imbuf_copy_doc},
    {"__copy__", reinterpret_cast<PyCFunction>(py_imbuf_copy), METH_NOARGS, py_imbuf_copy_doc},
    {"__deepcopy__",
     reinterpret_cast<PyCFunction>(py_imbuf_deepcopy),
     METH_VARARGS,
     py_imbuf_copy_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attributes
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_size_doc,
    "Size of the image in pixels.\n"
    "\n"
    ":type: tuple[int, int]\n");
static PyObject *py_imbuf_size_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf = self->ibuf;
  return PyC_Tuple_Pack_I32({ibuf->x, ibuf->y});
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_ppm_doc,
    "Pixels per meter.\n"
    "\n"
    ":type: tuple[float, float]\n");
static PyObject *py_imbuf_ppm_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf = self->ibuf;
  return PyC_Tuple_Pack_F64({ibuf->ppm[0], ibuf->ppm[1]});
}

static int py_imbuf_ppm_set(Py_ImBuf *self, PyObject *value, void * /*closure*/)
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

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_filepath_doc,
    "Filepath associated with this image.\n"
    "\n"
    ":type: str | bytes\n");
static PyObject *py_imbuf_filepath_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *ibuf = self->ibuf;
  return PyC_UnicodeFromBytes(ibuf->filepath);
}

static int py_imbuf_filepath_set(Py_ImBuf *self, PyObject *value, void * /*closure*/)
{
  PY_IMBUF_CHECK_INT(self);

  ImBuf *ibuf = self->ibuf;
  const Py_ssize_t value_str_len_max = sizeof(ibuf->filepath);
  PyObject *value_coerce = nullptr;
  Py_ssize_t value_str_len;
  const char *value_str = PyC_UnicodeAsBytesAndSize(value, &value_str_len, &value_coerce);
  if (UNLIKELY(value_str == nullptr)) {
    return -1;
  }
  if (value_str_len >= value_str_len_max) {
    PyErr_Format(PyExc_TypeError, "filepath length over %zd", value_str_len_max - 1);
    Py_XDECREF(value_coerce);
    return -1;
  }
  memcpy(ibuf->filepath, value_str, value_str_len + 1);
  Py_XDECREF(value_coerce);
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_planes_doc,
    "Number of bits per pixel for the byte buffer.\n"
    "Used when reading and writing image files.\n"
    "\n"
    "- 8: Gray-scale.\n"
    "- 16: Gray-scale with alpha.\n"
    "- 24: RGB.\n"
    "- 32: RGBA.\n"
    "\n"
    ".. note::\n"
    "\n"
    "   This value may be set by the file format on load,\n"
    "   and determines how many channels are written on save.\n"
    "\n"
    ":type: int\n");
static PyObject *py_imbuf_planes_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *imbuf = self->ibuf;
  return PyLong_FromLong(imbuf->planes);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_channels_doc,
    "Number of color channels.\n"
    "\n"
    ":type: int\n");
static PyObject *py_imbuf_channels_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  ImBuf *imbuf = self->ibuf;
  return PyLong_FromLong(imbuf->channels);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_quality_doc,
    "Quality for formats that support lossy compression (0 - 100, clamped).\n"
    "\n"
    ":type: int\n");
static PyObject *py_imbuf_quality_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  return PyLong_FromLong(self->ibuf->foptions.quality);
}
static int py_imbuf_quality_set(Py_ImBuf *self, PyObject *value, void * /*closure*/)
{
  PY_IMBUF_CHECK_INT(self);
  const int quality = PyC_Long_AsI32(value);
  if (quality == -1 && PyErr_Occurred()) {
    return -1;
  }
  self->ibuf->foptions.quality = char(std::clamp(quality, 0, 100));
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_compress_doc,
    "Compression level for formats that support lossless compression levels (0 - 100, clamped).\n"
    "\n"
    ":type: int\n");
static PyObject *py_imbuf_compress_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  return PyLong_FromLong(self->ibuf->foptions.compress);
}
static int py_imbuf_compress_set(Py_ImBuf *self, PyObject *value, void * /*closure*/)
{
  PY_IMBUF_CHECK_INT(self);
  const int compress = PyC_Long_AsI32(value);
  if (compress == -1 && PyErr_Occurred()) {
    return -1;
  }
  self->ibuf->foptions.compress = char(std::clamp(compress, 0, 100));
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_imbuf_file_type_doc,
    "The file type identifier.\n"
    "\n"
    ":type: str\n");
static PyObject *py_imbuf_file_type_get(Py_ImBuf *self, void * /*closure*/)
{
  PY_IMBUF_CHECK_OBJ(self);
  const char *id = py_imbuf_ftype_to_id_with_fallback(self->ibuf->ftype);
  return PyUnicode_FromString(id);
}

static int py_imbuf_file_type_set(Py_ImBuf *self, PyObject *value, void * /*closure*/)
{
  PY_IMBUF_CHECK_INT(self);
  const char *str = PyUnicode_AsUTF8(value);
  if (!str) {
    return -1;
  }
  const std::optional<int> ftype = py_imbuf_ftype_from_string(str);
  if (!ftype) {
    PyErr_Format(PyExc_ValueError, "unknown file type: '%.200s'", str);
    return -1;
  }
  self->ibuf->ftype = eImbFileType(*ftype);
  return 0;
}

static PyGetSetDef Py_ImBuf_getseters[] = {
    {"size",
     reinterpret_cast<getter>(py_imbuf_size_get),
     static_cast<setter>(nullptr),
     py_imbuf_size_doc,
     nullptr},
    {"ppm",
     reinterpret_cast<getter>(py_imbuf_ppm_get),
     reinterpret_cast<setter>(py_imbuf_ppm_set),
     py_imbuf_ppm_doc,
     nullptr},
    {"filepath",
     reinterpret_cast<getter>(py_imbuf_filepath_get),
     reinterpret_cast<setter>(py_imbuf_filepath_set),
     py_imbuf_filepath_doc,
     nullptr},
    {"planes",
     reinterpret_cast<getter>(py_imbuf_planes_get),
     nullptr,
     py_imbuf_planes_doc,
     nullptr},
    {"channels",
     reinterpret_cast<getter>(py_imbuf_channels_get),
     nullptr,
     py_imbuf_channels_doc,
     nullptr},
    {"file_type",
     reinterpret_cast<getter>(py_imbuf_file_type_get),
     reinterpret_cast<setter>(py_imbuf_file_type_set),
     py_imbuf_file_type_doc,
     nullptr},
    {"quality",
     reinterpret_cast<getter>(py_imbuf_quality_get),
     reinterpret_cast<setter>(py_imbuf_quality_set),
     py_imbuf_quality_doc,
     nullptr},
    {"compress",
     reinterpret_cast<getter>(py_imbuf_compress_get),
     reinterpret_cast<setter>(py_imbuf_compress_set),
     py_imbuf_compress_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type & Implementation
 * \{ */

static void py_imbuf_dealloc(Py_ImBuf *self)
{
  ImBuf *ibuf = self->ibuf;
  if (ibuf != nullptr) {
    IMB_freeImBuf(self->ibuf);
    self->ibuf = nullptr;
  }
  PyObject_DEL(self);
}

static PyObject *py_imbuf_repr(Py_ImBuf *self)
{
  const ImBuf *ibuf = self->ibuf;
  if (ibuf != nullptr) {
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
  return Py_HashPointer(self->ibuf);
}

PyTypeObject Py_ImBuf_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ImBuf",
    /*tp_basicsize*/ sizeof(Py_ImBuf),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(py_imbuf_dealloc),
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ reinterpret_cast<reprfunc>(py_imbuf_repr),
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ reinterpret_cast<hashfunc>(py_imbuf_hash),
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ Py_ImBuf_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ Py_ImBuf_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
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

static PyObject *Py_ImBuf_CreatePyObject(ImBuf *ibuf)
{
  Py_ImBuf *self = PyObject_New(Py_ImBuf, &Py_ImBuf_Type);
  self->ibuf = ibuf;
  self->buffer_users = 0;
  return reinterpret_cast<PyObject *>(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Buffer, Type & Implementation
 * \{ */

static void py_imbuf_buffer_dealloc(Py_ImBufBuffer *self)
{
  if (self->is_entered && self->py_ibuf) {
    self->py_ibuf->buffer_users--;
    BLI_assert(self->py_ibuf->buffer_users >= 0);
  }
  PyObject_GC_UnTrack(self);
  Py_CLEAR(self->py_ibuf);
  PyObject_GC_Del(self);
}

static int py_imbuf_buffer_traverse(Py_ImBufBuffer *self, visitproc visit, void *arg)
{
  Py_VISIT(self->py_ibuf);
  return 0;
}

static int py_imbuf_buffer_clear(Py_ImBufBuffer *self)
{
  Py_CLEAR(self->py_ibuf);
  return 0;
}

static PyObject *py_imbuf_buffer_repr(Py_ImBufBuffer *self)
{
  const ImBuf *ibuf = self->py_ibuf ? self->py_ibuf->ibuf : nullptr;
  if (ibuf != nullptr) {
    return PyUnicode_FromFormat(
        "<ImBufBuffer: size=(%d, %d), entered=%d>", ibuf->x, ibuf->y, int(self->is_entered));
  }
  return PyUnicode_FromString("<ImBufBuffer: invalid>");
}

static PyObject *py_imbuf_buffer_enter(Py_ImBufBuffer *self)
{
  if (UNLIKELY(self->is_entered)) {
    PyErr_SetString(PyExc_RuntimeError, "ImBufBuffer context is already entered");
    return nullptr;
  }
  ImBuf *ibuf = self->py_ibuf->ibuf;
  if (UNLIKELY(ibuf == nullptr)) {
    PyErr_SetString(PyExc_ReferenceError, "ImBuf data has been freed");
    return nullptr;
  }

  const bool is_byte = (self->mode == IB_byte_data);

  if (is_byte) {
    if (UNLIKELY(ibuf->byte_buffer.data == nullptr)) {
      PyErr_SetString(PyExc_RuntimeError, "ImBuf has no byte pixel data");
      return nullptr;
    }
  }
  else {
    if (UNLIKELY(ibuf->float_buffer.data == nullptr)) {
      PyErr_SetString(PyExc_RuntimeError, "ImBuf has no float pixel data");
      return nullptr;
    }
  }

  const int channels = is_byte ? 4 : (ibuf->channels ? ibuf->channels : 4);
  const Py_ssize_t itemsize = is_byte ? 1 : Py_ssize_t(sizeof(float));
  const char *format = is_byte ? "B" : "f";

  PyObject *memview;

  if (self->region == std::nullopt) {
    Py_ssize_t num_items = Py_ssize_t(ibuf->x) * ibuf->y * channels;

    Py_buffer pybuf;
    memset(&pybuf, 0, sizeof(pybuf));
    pybuf.buf = is_byte ? static_cast<void *>(ibuf->byte_buffer.data) :
                          static_cast<void *>(ibuf->float_buffer.data);
    pybuf.len = num_items * itemsize;
    pybuf.itemsize = itemsize;
    pybuf.readonly = !self->writable;
    pybuf.format = const_cast<char *>(format);
    pybuf.ndim = 1;
    pybuf.shape = &num_items;
    pybuf.strides = &pybuf.itemsize;
    memview = PyMemoryView_FromBuffer(&pybuf);
  }
  else {
    const rcti &region = *self->region;
    const int xmin = region.xmin;
    const int ymin = region.ymin;
    const int xmax = region.xmax;
    const int ymax = region.ymax;

    const Py_ssize_t row_stride = Py_ssize_t(ibuf->x) * channels * itemsize;
    const Py_ssize_t offset = (Py_ssize_t(ymin) * ibuf->x + xmin) * channels * itemsize;

    Py_ssize_t shape[2] = {ymax - ymin, (xmax - xmin) * channels};
    Py_ssize_t strides[2] = {row_stride, itemsize};

    Py_buffer pybuf;
    memset(&pybuf, 0, sizeof(pybuf));
    pybuf.buf = is_byte ? static_cast<void *>(ibuf->byte_buffer.data + offset) :
                          static_cast<void *>(reinterpret_cast<char *>(ibuf->float_buffer.data) +
                                              offset);
    pybuf.len = shape[0] * shape[1] * itemsize;
    pybuf.itemsize = itemsize;
    pybuf.readonly = !self->writable;
    pybuf.format = const_cast<char *>(format);
    pybuf.ndim = 2;
    pybuf.shape = shape;
    pybuf.strides = strides;
    memview = PyMemoryView_FromBuffer(&pybuf);
  }

  if (UNLIKELY(memview == nullptr)) {
    return nullptr;
  }

  self->is_entered = true;
  self->py_ibuf->buffer_users++;
  return memview;
}

static PyObject *py_imbuf_buffer_exit(Py_ImBufBuffer *self, PyObject * /*args*/)
{
  if (self->is_entered && self->py_ibuf) {
    self->is_entered = false;
    self->py_ibuf->buffer_users--;
    BLI_assert(self->py_ibuf->buffer_users >= 0);

    if (self->writable) {
      ImBuf *ibuf = self->py_ibuf->ibuf;
      if (ibuf != nullptr) {
        if (self->mode == IB_byte_data) {
          if (ibuf->float_buffer.data != nullptr) {
            IMB_float_from_byte(ibuf);
          }
        }
        else {
          if (ibuf->byte_buffer.data != nullptr) {
            IMB_byte_from_float(ibuf);
          }
        }
      }
    }
  }
  Py_RETURN_NONE;
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

static PyMethodDef Py_ImBufBuffer_methods[] = {
    {"__enter__", reinterpret_cast<PyCFunction>(py_imbuf_buffer_enter), METH_NOARGS, nullptr},
    {"__exit__", reinterpret_cast<PyCFunction>(py_imbuf_buffer_exit), METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyTypeObject Py_ImBufBuffer_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ImBufBuffer",
    /*tp_basicsize*/ sizeof(Py_ImBufBuffer),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(py_imbuf_buffer_dealloc),
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ reinterpret_cast<reprfunc>(py_imbuf_buffer_repr),
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ PyObject_HashNotImplemented,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ reinterpret_cast<traverseproc>(py_imbuf_buffer_traverse),
    /*tp_clear*/ reinterpret_cast<inquiry>(py_imbuf_buffer_clear),
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ Py_ImBufBuffer_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
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

/* -------------------------------------------------------------------- */
/** \name File Type, Type & Implementation
 * \{ */

static PyObject *py_imbuf_file_type_id_get(Py_ImBufFileType *self, void * /*closure*/)
{
  const char *id = py_imbuf_ftype_to_id_with_fallback(self->ftype);
  return PyUnicode_FromString(id);
}

static PyObject *py_imbuf_file_type_file_extensions_get(Py_ImBufFileType *self, void * /*closure*/)
{
  const char **ext = IMB_ftype_file_extensions(self->ftype);
  if (!ext) {
    return PyTuple_New(0);
  }
  int len = 0;
  for (const char **p = ext; *p; p++) {
    len++;
  }
  PyObject *tuple = PyTuple_New(len);
  for (int i = 0; i < len; i++) {
    PyTuple_SET_ITEM(tuple, i, PyUnicode_FromString(ext[i]));
  }
  return tuple;
}

static PyObject *py_imbuf_file_type_capability_read_get(Py_ImBufFileType *self, void *flag_p)
{
  const eImFileTypeCapability flag = eImFileTypeCapability(POINTER_AS_INT(flag_p));
  return PyBool_FromLong((IMB_ftype_capability_read(self->ftype) & flag) !=
                         eImFileTypeCapability::Zero);
}

static PyObject *py_imbuf_file_type_capability_write_get(Py_ImBufFileType *self, void *flag_p)
{
  const eImFileTypeCapability flag = eImFileTypeCapability(POINTER_AS_INT(flag_p));
  return PyBool_FromLong((IMB_ftype_capability_write(self->ftype) & flag) !=
                         eImFileTypeCapability::Zero);
}

static PyGetSetDef Py_ImBufFileType_getseters[] = {
    {"id", reinterpret_cast<getter>(py_imbuf_file_type_id_get), nullptr, nullptr, nullptr},
    {"file_extensions",
     reinterpret_cast<getter>(py_imbuf_file_type_file_extensions_get),
     nullptr,
     nullptr,
     nullptr},
    {"has_read_file",
     reinterpret_cast<getter>(py_imbuf_file_type_capability_read_get),
     nullptr,
     nullptr,
     POINTER_FROM_INT(eImFileTypeCapability::File)},
    {"has_write_file",
     reinterpret_cast<getter>(py_imbuf_file_type_capability_write_get),
     nullptr,
     nullptr,
     POINTER_FROM_INT(eImFileTypeCapability::File)},
    {"has_read_memory",
     reinterpret_cast<getter>(py_imbuf_file_type_capability_read_get),
     nullptr,
     nullptr,
     POINTER_FROM_INT(eImFileTypeCapability::Memory)},
    {"has_write_memory",
     reinterpret_cast<getter>(py_imbuf_file_type_capability_write_get),
     nullptr,
     nullptr,
     POINTER_FROM_INT(eImFileTypeCapability::Memory)},
    {nullptr},
};

static PyObject *py_imbuf_file_type_repr(Py_ImBufFileType *self)
{
  const char *id = py_imbuf_ftype_to_id_with_fallback(self->ftype);
  return PyUnicode_FromFormat("<ImBufFileType: id='%s'>", id);
}

static Py_hash_t py_imbuf_file_type_hash(Py_ImBufFileType *self)
{
  return self->ftype;
}

PyTypeObject Py_ImBufFileType_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ImBufFileType",
    /*tp_basicsize*/ sizeof(Py_ImBufFileType),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ reinterpret_cast<reprfunc>(py_imbuf_file_type_repr),
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ reinterpret_cast<hashfunc>(py_imbuf_file_type_hash),
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ Py_ImBufFileType_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
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
/** \name Module Functions
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    M_imbuf_new_doc,
    ".. function:: new(size, *, planes=32)\n"
    "\n"
    "   Create a new image.\n"
    "\n"
    "   :param size: The size of the image in pixels.\n"
    "   :type size: tuple[int, int]\n"
    "   :param planes: Number of bits per pixel.\n"
    "   :type planes: Literal[8, 16, 24, 32]\n"
    "   :return: The newly created image.\n"
    "   :rtype: :class:`ImBuf`\n");
static PyObject *M_imbuf_new(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  int size[2];
  int planes = 32;
  static const char *_keywords[] = {
      "size",
      "planes",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      "(ii)" /* `size` */
      "|$"   /* Optional keyword only arguments. */
      "i"    /* `planes` */
      ":new",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &size[0], &size[1], &planes)) {
    return nullptr;
  }
  if (size[0] <= 0 || size[1] <= 0) {
    PyErr_Format(PyExc_ValueError, "new: Image size cannot be below 1 (%d, %d)", UNPACK2(size));
    return nullptr;
  }
  if (!ELEM(planes, 8, 16, 24, 32)) {
    PyErr_Format(PyExc_ValueError, "new: planes must be 8, 16, 24 or 32, got %d", planes);
    return nullptr;
  }

  const uint flags = IB_byte_data;

  ImBuf *ibuf = IMB_allocImBuf(UNPACK2(size), uchar(planes), flags);
  if (ibuf == nullptr) {
    PyErr_Format(PyExc_ValueError, "new: Unable to create image (%d, %d)", UNPACK2(size));
    return nullptr;
  }
  return Py_ImBuf_CreatePyObject(ibuf);
}

static PyObject *imbuf_load_impl(const char *filepath)
{
  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    PyErr_Format(PyExc_IOError, "load: %s, failed to open file '%s'", strerror(errno), filepath);
    return nullptr;
  }

  ImBuf *ibuf = IMB_load_image_from_file_descriptor(file, IB_byte_data, filepath);

  close(file);

  if (ibuf == nullptr) {
    PyErr_Format(
        PyExc_ValueError, "load: Unable to recognize image format for file '%s'", filepath);
    return nullptr;
  }

  STRNCPY(ibuf->filepath, filepath);

  return Py_ImBuf_CreatePyObject(ibuf);
}

PyDoc_STRVAR(
    /* Wrap. */
    M_imbuf_load_doc,
    ".. function:: load(filepath)\n"
    "\n"
    "   Load an image from a file.\n"
    "\n"
    "   :param filepath: The filepath of the image.\n"
    "   :type filepath: str | bytes\n"
    "   :return: The newly loaded image.\n"
    "   :rtype: :class:`ImBuf`\n");
static PyObject *M_imbuf_load(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};

  static const char *_keywords[] = {"filepath", nullptr};
  static _PyArg_Parser _parser = {
      "O&" /* `filepath` */
      ":load",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, PyC_ParseUnicodeAsBytesAndSize, &filepath_data))
  {
    return nullptr;
  }

  PyObject *result = imbuf_load_impl(filepath_data.value);
  Py_XDECREF(filepath_data.value_coerce);
  return result;
}

static PyObject *imbuf_load_from_memory_impl(const char *buffer,
                                             const size_t buffer_size,
                                             int flags)
{
  ImBuf *ibuf = IMB_load_image_from_memory(
      reinterpret_cast<const uchar *>(buffer), buffer_size, flags, "<imbuf.load_from_buffer>");

  if (ibuf == nullptr) {
    PyErr_SetString(PyExc_ValueError, "load_from_buffer: Unable to load image from memory");
    return nullptr;
  }

  return Py_ImBuf_CreatePyObject(ibuf);
}

PyDoc_STRVAR(
    /* Wrap. */
    M_imbuf_load_from_buffer_doc,
    ".. function:: load_from_buffer(buffer)\n"
    "\n"
    "   Load an image from a buffer.\n"
    "\n"
    "   :param buffer: A buffer containing the image data.\n"
    "   :type buffer: collections.abc.Buffer\n"
    "   :return: The newly loaded image.\n"
    "   :rtype: :class:`ImBuf`\n");
static PyObject *M_imbuf_load_from_buffer(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyObject *buffer_py_ob;

  static const char *_keywords[] = {"buffer", nullptr};
  static _PyArg_Parser _parser = {
      "O" /* `buffer` */
      ":load_from_buffer",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &buffer_py_ob)) {
    return nullptr;
  }

  PyObject *result = nullptr;
  /* TODO: should be arguments. */
  int flags = IB_byte_data;

  /* This supports `PyBytes`, no need for a separate check. */
  if (PyObject_CheckBuffer(buffer_py_ob)) {
    Py_buffer pybuffer;
    if (PyObject_GetBuffer(buffer_py_ob, &pybuffer, PyBUF_SIMPLE) == -1) {
      return nullptr;
    }
    result = imbuf_load_from_memory_impl(
        reinterpret_cast<const char *>(pybuffer.buf), pybuffer.len, flags);

    PyBuffer_Release(&pybuffer);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "load_from_buffer: expected a buffer, unsupported type %.200s",
                 Py_TYPE(buffer_py_ob)->tp_name);
    return nullptr;
  }
  return result;
}

static PyObject *imbuf_write_impl(ImBuf *ibuf, const char *filepath)
{
  const bool ok = IMB_save_image(ibuf, filepath, IB_byte_data);
  if (ok == false) {
    PyErr_Format(
        PyExc_IOError, "write: Unable to write image file (%s) '%s'", strerror(errno), filepath);
    return nullptr;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_imbuf_write_doc,
    ".. function:: write(image, *, filepath=None)\n"
    "\n"
    "   Write an image.\n"
    "\n"
    "   :param image: The image to write.\n"
    "   :type image: :class:`ImBuf`\n"
    "   :param filepath: Optional filepath of the image (fallback to the image's file path).\n"
    "   :type filepath: str | bytes | None\n");
static PyObject *M_imbuf_write(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  Py_ImBuf *py_imb;
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};

  static const char *_keywords[] = {"image", "filepath", nullptr};
  static _PyArg_Parser _parser = {
      "O!" /* `image` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `filepath` */
      ":write",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        &Py_ImBuf_Type,
                                        &py_imb,
                                        PyC_ParseUnicodeAsBytesAndSize_OrNone,
                                        &filepath_data))
  {
    return nullptr;
  }

  PY_IMBUF_CHECK_OBJ(py_imb);

  if ((IMB_ftype_capability_write(py_imb->ibuf->ftype) & eImFileTypeCapability::File) ==
      eImFileTypeCapability::Zero)
  {
    const char *id = py_imbuf_ftype_to_id_with_fallback(py_imb->ibuf->ftype);
    PyErr_Format(
        PyExc_ValueError, "write: file type '%.200s' does not support writing to a file", id);
    return nullptr;
  }

  const char *filepath = filepath_data.value;
  if (filepath == nullptr) {
    /* Argument omitted, use images path. */
    filepath = py_imb->ibuf->filepath;
  }
  PyObject *result = imbuf_write_impl(py_imb->ibuf, filepath);
  Py_XDECREF(filepath_data.value_coerce);
  return result;
}

/**
 * Encode `ibuf` to memory and write the result to `file`.
 */
static PyObject *imbuf_write_to_buffer_impl(ImBuf *ibuf, PyObject *file)
{
  const bool is_float = ibuf->float_buffer.data != nullptr;
  if (ibuf->ftype == IMB_FTYPE_NONE) {
    ibuf->ftype = IMB_FTYPE_DEFAULT;
  }

  const bool ok = IMB_save_image(
      ibuf, "<memory>", eImBufFlags(IB_mem | (is_float ? IB_float_data : IB_byte_data)));
  if (!ok) {
    PyErr_SetString(PyExc_RuntimeError, "write_to_buffer: failed to write image to memory");
    return nullptr;
  }

  PyObject *memview = PyMemoryView_FromMemory(reinterpret_cast<char *>(ibuf->encoded_buffer.data),
                                              Py_ssize_t(ibuf->encoded_size),
                                              PyBUF_READ);
  if (!memview) {
    return nullptr;
  }

  /* Handles missing attribute, non-callable attribute, and write errors. */
  PyObject *result = PyObject_CallMethod(file, "write", "O", memview);
  Py_DECREF(memview);

  if (!result) {
    return nullptr;
  }
  Py_DECREF(result);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_imbuf_write_to_buffer_doc,
    ".. function:: write_to_buffer(image, file)\n"
    "\n"
    "   Write an image to a file-like object.\n"
    "\n"
    "   :param image: The image to write.\n"
    "   :type image: :class:`ImBuf`\n"
    "   :param file: A writable file-like object (e.g. :class:`io.BytesIO`).\n"
    "   :type file: :class:`BinaryIO`\n");
static PyObject *M_imbuf_write_to_buffer(PyObject * /*self*/, PyObject *args)
{
  Py_ImBuf *py_imb;
  PyObject *file;

  if (!PyArg_ParseTuple(args, "O!O:write_to_buffer", &Py_ImBuf_Type, &py_imb, &file)) {
    return nullptr;
  }
  PY_IMBUF_CHECK_OBJ(py_imb);

  if ((IMB_ftype_capability_write(py_imb->ibuf->ftype) & eImFileTypeCapability::Memory) ==
      eImFileTypeCapability::Zero)
  {
    const char *id = py_imbuf_ftype_to_id_with_fallback(py_imb->ibuf->ftype);
    PyErr_Format(PyExc_ValueError,
                 "write_to_buffer: file type '%.200s' does not support writing to memory",
                 id);
    return nullptr;
  }

  /* Work on a copy to avoid mutating the original (encoded_buffer, ftype).
   * This could be avoided by making the encoded buffer free function public. */
  ImBuf *ibuf = IMB_dupImBuf(py_imb->ibuf);
  if (!ibuf) {
    PyErr_SetString(PyExc_MemoryError, "write_to_buffer: failed to duplicate image buffer");
    return nullptr;
  }
  PyObject *result = imbuf_write_to_buffer_impl(ibuf, file);
  IMB_freeImBuf(ibuf);
  return result;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_imbuf_file_type_from_buffer_doc,
    ".. function:: file_type_from_buffer(buffer)\n"
    "\n"
    "   Detect the image file type from a buffer.\n"
    "\n"
    "   :param buffer: A buffer containing image data.\n"
    "   :type buffer: collections.abc.Buffer\n"
    "   :return: The detected file type, or None if unrecognized.\n"
    "   :rtype: :class:`ImBufFileType` or None\n");
static PyObject *M_imbuf_file_type_from_buffer(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyObject *buffer_py_ob;

  static const char *_keywords[] = {"buffer", nullptr};
  static _PyArg_Parser _parser = {
      "O" /* `buffer` */
      ":file_type_from_buffer",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &buffer_py_ob)) {
    return nullptr;
  }

  if (!PyObject_CheckBuffer(buffer_py_ob)) {
    PyErr_Format(PyExc_TypeError,
                 "file_type_from_buffer: expected a buffer, unsupported type %.200s",
                 Py_TYPE(buffer_py_ob)->tp_name);
    return nullptr;
  }

  Py_buffer pybuffer;
  if (PyObject_GetBuffer(buffer_py_ob, &pybuffer, PyBUF_SIMPLE) == -1) {
    return nullptr;
  }
  const int ftype = IMB_test_image_type_from_memory(
      reinterpret_cast<const unsigned char *>(pybuffer.buf), pybuffer.len);
  PyBuffer_Release(&pybuffer);

  if (ftype == IMB_FTYPE_NONE) {
    Py_RETURN_NONE;
  }

  Py_ImBufFileType *val = PyObject_New(Py_ImBufFileType, &Py_ImBufFileType_Type);
  val->ftype = ftype;
  return reinterpret_cast<PyObject *>(val);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Definition (`imbuf`)
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef IMB_methods[] = {
    {"new",
     reinterpret_cast<PyCFunction>(M_imbuf_new),
     METH_VARARGS | METH_KEYWORDS,
     M_imbuf_new_doc},
    {"load",
     reinterpret_cast<PyCFunction>(M_imbuf_load),
     METH_VARARGS | METH_KEYWORDS,
     M_imbuf_load_doc},
    {"load_from_buffer",
     reinterpret_cast<PyCFunction>(M_imbuf_load_from_buffer),
     METH_VARARGS | METH_KEYWORDS,
     M_imbuf_load_from_buffer_doc},
    {"write",
     reinterpret_cast<PyCFunction>(M_imbuf_write),
     METH_VARARGS | METH_KEYWORDS,
     M_imbuf_write_doc},
    {"write_to_buffer",
     reinterpret_cast<PyCFunction>(M_imbuf_write_to_buffer),
     METH_VARARGS,
     M_imbuf_write_to_buffer_doc},
    {"file_type_from_buffer",
     reinterpret_cast<PyCFunction>(M_imbuf_file_type_from_buffer),
     METH_VARARGS | METH_KEYWORDS,
     M_imbuf_file_type_from_buffer_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    IMB_doc,
    "This module provides access to Blender's image manipulation API.\n"
    "\n"
    "It provides access to image buffers outside of Blender's\n"
    ":class:`bpy.types.Image` data-block context.\n");
static PyModuleDef IMB_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "imbuf",
    /*m_doc*/ IMB_doc,
    /*m_size*/ 0,
    /*m_methods*/ IMB_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_imbuf()
{
  PyObject *mod;
  PyObject *submodule;
  PyObject *sys_modules = PyImport_GetModuleDict();

  mod = PyModule_Create(&IMB_module_def);

  /* `imbuf.types` */
  PyModule_AddObject(mod, "types", (submodule = BPyInit_imbuf_types()));
  PyC_Module_AddToSysModules(sys_modules, submodule);

  /* `imbuf.file_types` (read-only dict of supported file type identifiers). */
  {
    if (PyType_Ready(&Py_ImBufFileType_Type) < 0) {
      return nullptr;
    }
    PyObject *dict = _PyDict_NewPresized(IMB_FTYPE_LAST + 1);
    for (int ftype = 0; ftype <= IMB_FTYPE_LAST; ftype++) {
      const char *id = (ftype != IMB_FTYPE_NONE) ? IMB_ftype_to_id(ftype) : py_imbuf_type_none;
      if (id) {
        Py_ImBufFileType *val = PyObject_New(Py_ImBufFileType, &Py_ImBufFileType_Type);
        val->ftype = ftype;
        PyDict_SetItemString(dict, id, reinterpret_cast<PyObject *>(val));
        Py_DECREF(val);
      }
    }
    PyObject *proxy = PyDictProxy_New(dict);
    PyModule_AddObject(mod, "file_types", proxy);
    Py_DECREF(dict);
  }

  return mod;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Definition (`imbuf.types`)
 *
 * `imbuf.types` module, only include this to expose access to `imbuf.types.ImBuf`
 * for docs and the ability to use with built-ins such as `isinstance`, `issubclass`.
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    IMB_types_doc,
    "This module provides access to image buffer types.\n"
    "\n"
    ".. note::\n"
    "\n"
    "   Image buffer is also the structure used by :class:`bpy.types.Image`\n"
    "   ID type to store and manipulate image data at runtime.\n");
static PyModuleDef IMB_types_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "imbuf.types",
    /*m_doc*/ IMB_types_doc,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_imbuf_types()
{
  PyObject *submodule = PyModule_Create(&IMB_types_module_def);

  if (PyType_Ready(&Py_ImBuf_Type) < 0) {
    return nullptr;
  }

  PyModule_AddType(submodule, &Py_ImBuf_Type);
  PyModule_AddType(submodule, &Py_ImBufBuffer_Type);
  PyModule_AddType(submodule, &Py_ImBufFileType_Type);

  return submodule;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

ImBuf *BPy_ImBuf_FromPyObject(PyObject *py_imbuf)
{
  /* The caller must ensure this. */
  BLI_assert(Py_TYPE(py_imbuf) == &Py_ImBuf_Type);

  if (py_imbuf_valid_check(reinterpret_cast<Py_ImBuf *>(py_imbuf)) == -1) {
    return nullptr;
  }

  return (reinterpret_cast<Py_ImBuf *>(py_imbuf))->ibuf;
}

/** \} */

}  // namespace blender
