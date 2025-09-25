/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * This file defines the `blf` module, used for drawing text to the GPU or image buffers.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include "blf_py_api.hh"

#include "py_capi_utils.hh"

#include <Python.h>

#include "../../blenfont/BLF_api.hh"

#include "BLI_utildefines.h"

#include "../../imbuf/IMB_colormanagement.hh"
#include "../../imbuf/IMB_imbuf.hh"
#include "../../imbuf/IMB_imbuf_types.hh"

#include "python_compat.hh" /* IWYU pragma: keep. */

#include "python_utildefines.hh"

#include "imbuf_py_api.hh"

struct BPyBLFImBufContext {
  PyObject_HEAD /* Required Python macro. */
  PyObject *py_imbuf;

  int fontid;
  BLFBufferState *buffer_state;
};

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_position_doc,
    ".. function:: position(fontid, x, y, z)\n"
    "\n"
    "   Set the position for drawing text.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg x: X axis position to draw the text.\n"
    "   :type x: float\n"
    "   :arg y: Y axis position to draw the text.\n"
    "   :type y: float\n"
    "   :arg z: Z axis position to draw the text.\n"
    "   :type z: float\n");
static PyObject *py_blf_position(PyObject * /*self*/, PyObject *args)
{
  int fontid;
  float x, y, z;

  if (!PyArg_ParseTuple(args, "ifff:blf.position", &fontid, &x, &y, &z)) {
    return nullptr;
  }

  BLF_position(fontid, x, y, z);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_size_doc,
    ".. function:: size(fontid, size)\n"
    "\n"
    "   Set the size for drawing text.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg size: Point size of the font.\n"
    "   :type size: float\n");
static PyObject *py_blf_size(PyObject * /*self*/, PyObject *args)
{
  int fontid;
  float size;

  if (!PyArg_ParseTuple(args, "if:blf.size", &fontid, &size)) {
    return nullptr;
  }

  BLF_size(fontid, size);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_aspect_doc,
    ".. function:: aspect(fontid, aspect)\n"
    "\n"
    "   Set the aspect for drawing text.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg aspect: The aspect ratio for text drawing to use.\n"
    "   :type aspect: float\n");
static PyObject *py_blf_aspect(PyObject * /*self*/, PyObject *args)
{
  float aspect;
  int fontid;

  if (!PyArg_ParseTuple(args, "if:blf.aspect", &fontid, &aspect)) {
    return nullptr;
  }

  BLF_aspect(fontid, aspect, aspect, 1.0);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_color_doc,
    ".. function:: color(fontid, r, g, b, a)\n"
    "\n"
    "   Set the color for drawing text.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg r: red channel 0.0 - 1.0.\n"
    "   :type r: float\n"
    "   :arg g: green channel 0.0 - 1.0.\n"
    "   :type g: float\n"
    "   :arg b: blue channel 0.0 - 1.0.\n"
    "   :type b: float\n"
    "   :arg a: alpha channel 0.0 - 1.0.\n"
    "   :type a: float\n");
static PyObject *py_blf_color(PyObject * /*self*/, PyObject *args)
{
  int fontid;
  float rgba[4];

  if (!PyArg_ParseTuple(args, "iffff:blf.color", &fontid, &rgba[0], &rgba[1], &rgba[2], &rgba[3]))
  {
    return nullptr;
  }

  BLF_color4fv(fontid, rgba);

  /* NOTE(@ideasman42): that storing these colors separately looks like something that could
   * be refactored away if the font's internal color format was changed from `uint8` to `float`. */
  BLF_buffer_col(fontid, rgba);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_draw_doc,
    ".. function:: draw(fontid, text)\n"
    "\n"
    "   Draw text in the current context.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg text: the text to draw.\n"
    "   :type text: str\n");
static PyObject *py_blf_draw(PyObject * /*self*/, PyObject *args)
{
  const char *text;
  Py_ssize_t text_length;
  int fontid;

  if (!PyArg_ParseTuple(args, "is#:blf.draw", &fontid, &text, &text_length)) {
    return nullptr;
  }

  BLF_draw(fontid, text, uint(text_length));

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_draw_buffer_doc,
    ".. function:: draw_buffer(fontid, text)\n"
    "\n"
    "   Draw text into the buffer bound to the fontid.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg text: the text to draw.\n"
    "   :type text: str\n");
static PyObject *py_blf_draw_buffer(PyObject * /*self*/, PyObject *args)
{
  const char *text;
  Py_ssize_t text_length;
  int fontid;

  if (!PyArg_ParseTuple(args, "is#:blf.draw_buffer", &fontid, &text, &text_length)) {
    return nullptr;
  }

  BLF_draw_buffer(fontid, text, uint(text_length));

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_dimensions_doc,
    ".. function:: dimensions(fontid, text)\n"
    "\n"
    "   Return the width and height of the text.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg text: the text to draw.\n"
    "   :type text: str\n"
    "   :return: the width and height of the text.\n"
    "   :rtype: tuple[float, float]\n");
static PyObject *py_blf_dimensions(PyObject * /*self*/, PyObject *args)
{
  const char *text;
  float width, height;
  PyObject *ret;
  int fontid;

  if (!PyArg_ParseTuple(args, "is:blf.dimensions", &fontid, &text)) {
    return nullptr;
  }

  BLF_width_and_height(fontid, text, INT_MAX, &width, &height);

  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, PyFloat_FromDouble(width), PyFloat_FromDouble(height));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_clipping_doc,
    ".. function:: clipping(fontid, xmin, ymin, xmax, ymax)\n"
    "\n"
    "   Set the clipping, enable/disable using CLIPPING.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg xmin: Clip the drawing area by these bounds.\n"
    "   :type xmin: float\n"
    "   :arg ymin: Clip the drawing area by these bounds.\n"
    "   :type ymin: float\n"
    "   :arg xmax: Clip the drawing area by these bounds.\n"
    "   :type xmax: float\n"
    "   :arg ymax: Clip the drawing area by these bounds.\n"
    "   :type ymax: float\n");
static PyObject *py_blf_clipping(PyObject * /*self*/, PyObject *args)
{
  float xmin, ymin, xmax, ymax;
  int fontid;

  if (!PyArg_ParseTuple(args, "iffff:blf.clipping", &fontid, &xmin, &ymin, &xmax, &ymax)) {
    return nullptr;
  }

  BLF_clipping(fontid, xmin, ymin, xmax, ymax);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_word_wrap_doc,
    ".. function:: word_wrap(fontid, wrap_width)\n"
    "\n"
    "   Set the wrap width, enable/disable using WORD_WRAP.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg wrap_width: The width (in pixels) to wrap words at.\n"
    "   :type wrap_width: int\n");
static PyObject *py_blf_word_wrap(PyObject * /*self*/, PyObject *args)
{
  int wrap_width;
  int fontid;

  if (!PyArg_ParseTuple(args, "ii:blf.word_wrap", &fontid, &wrap_width)) {
    return nullptr;
  }

  BLF_wordwrap(fontid, wrap_width);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_disable_doc,
    ".. function:: disable(fontid, option)\n"
    "\n"
    "   Disable option.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg option: One of ROTATION, CLIPPING, SHADOW or KERNING_DEFAULT.\n"
    "   :type option: int\n");
static PyObject *py_blf_disable(PyObject * /*self*/, PyObject *args)
{
  int option, fontid;

  if (!PyArg_ParseTuple(args, "ii:blf.disable", &fontid, &option)) {
    return nullptr;
  }

  BLF_disable(fontid, FontFlags(option));

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_enable_doc,
    ".. function:: enable(fontid, option)\n"
    "\n"
    "   Enable option.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg option: One of ROTATION, CLIPPING, SHADOW or KERNING_DEFAULT.\n"
    "   :type option: int\n");
static PyObject *py_blf_enable(PyObject * /*self*/, PyObject *args)
{
  int option, fontid;

  if (!PyArg_ParseTuple(args, "ii:blf.enable", &fontid, &option)) {
    return nullptr;
  }

  BLF_enable(fontid, FontFlags(option));

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_rotation_doc,
    ".. function:: rotation(fontid, angle)\n"
    "\n"
    "   Set the text rotation angle, enable/disable using ROTATION.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg angle: The angle for text drawing to use.\n"
    "   :type angle: float\n");
static PyObject *py_blf_rotation(PyObject * /*self*/, PyObject *args)
{
  float angle;
  int fontid;

  if (!PyArg_ParseTuple(args, "if:blf.rotation", &fontid, &angle)) {
    return nullptr;
  }

  BLF_rotation(fontid, angle);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_shadow_doc,
    ".. function:: shadow(fontid, level, r, g, b, a)\n"
    "\n"
    "   Shadow options, enable/disable using SHADOW .\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg level: The blur level (0, 3, 5) or outline (6).\n"
    "   :type level: int\n"
    "   :arg r: Shadow color (red channel 0.0 - 1.0).\n"
    "   :type r: float\n"
    "   :arg g: Shadow color (green channel 0.0 - 1.0).\n"
    "   :type g: float\n"
    "   :arg b: Shadow color (blue channel 0.0 - 1.0).\n"
    "   :type b: float\n"
    "   :arg a: Shadow color (alpha channel 0.0 - 1.0).\n"
    "   :type a: float\n");
static PyObject *py_blf_shadow(PyObject * /*self*/, PyObject *args)
{
  int level, fontid;
  float rgba[4];

  if (!PyArg_ParseTuple(
          args, "iiffff:blf.shadow", &fontid, &level, &rgba[0], &rgba[1], &rgba[2], &rgba[3]))
  {
    return nullptr;
  }

  if (!ELEM(level, 0, 3, 5, 6)) {
    PyErr_SetString(PyExc_TypeError, "blf.shadow expected arg to be in (0, 3, 5, 6)");
    return nullptr;
  }

  BLF_shadow(fontid, FontShadowType(level), rgba);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_shadow_offset_doc,
    ".. function:: shadow_offset(fontid, x, y)\n"
    "\n"
    "   Set the offset for shadow text.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg x: Vertical shadow offset value in pixels.\n"
    "   :type x: float\n"
    "   :arg y: Horizontal shadow offset value in pixels.\n"
    "   :type y: float\n");
static PyObject *py_blf_shadow_offset(PyObject * /*self*/, PyObject *args)
{
  int x, y, fontid;

  if (!PyArg_ParseTuple(args, "iii:blf.shadow_offset", &fontid, &x, &y)) {
    return nullptr;
  }

  BLF_shadow_offset(fontid, x, y);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_load_doc,
    ".. function:: load(filepath)\n"
    "\n"
    "   Load a new font.\n"
    "\n"
    "   :arg filepath: the filepath of the font.\n"
    "   :type filepath: str | bytes\n"
    "   :return: the new font's fontid or -1 if there was an error.\n"
    "   :rtype: int\n");
static PyObject *py_blf_load(PyObject * /*self*/, PyObject *args)
{
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};
  if (!PyArg_ParseTuple(args,
                        "O&" /* `filepath` */
                        ":blf.load",
                        PyC_ParseUnicodeAsBytesAndSize,
                        &filepath_data))
  {
    return nullptr;
  }
  const int font_id = BLF_load(filepath_data.value);
  Py_XDECREF(filepath_data.value_coerce);

  return PyLong_FromLong(font_id);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_blf_unload_doc,
    ".. function:: unload(filepath)\n"
    "\n"
    "   Unload an existing font.\n"
    "\n"
    "   :arg filepath: the filepath of the font.\n"
    "   :type filepath: str | bytes\n");
static PyObject *py_blf_unload(PyObject * /*self*/, PyObject *args)
{
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};
  if (!PyArg_ParseTuple(args,
                        "O&" /* `filepath` */
                        ":blf.unload",
                        PyC_ParseUnicodeAsBytesAndSize,
                        &filepath_data))
  {
    return nullptr;
  }

  BLF_unload(filepath_data.value);
  Py_XDECREF(filepath_data.value_coerce);

  Py_RETURN_NONE;
}

/* -------------------------------------------------------------------- */
/** \name Image Buffer Access
 *
 * Context manager for #ImBuf.
 * \{ */

static PyObject *py_blf_bind_imbuf_enter(BPyBLFImBufContext *self)
{
  if (UNLIKELY(self->buffer_state)) {
    PyErr_SetString(PyExc_ValueError,
                    "BLFImBufContext.__enter__: unable to enter the same context more than once");
    return nullptr;
  }

  ImBuf *ibuf = BPy_ImBuf_FromPyObject(self->py_imbuf);
  if (ibuf == nullptr) {
    /* The error will have been set. */
    return nullptr;
  }
  BLFBufferState *buffer_state = BLF_buffer_state_push(self->fontid);
  if (buffer_state == nullptr) {
    PyErr_Format(PyExc_ValueError, "bind_imbuf: unknown fontid %d", self->fontid);
    return nullptr;
  }
  BLF_buffer(self->fontid,
             ibuf->float_buffer.data,
             ibuf->byte_buffer.data,
             ibuf->x,
             ibuf->y,
             ibuf->byte_buffer.colorspace);
  self->buffer_state = buffer_state;

  Py_RETURN_NONE;
}

static PyObject *py_blf_bind_imbuf_exit(BPyBLFImBufContext *self, PyObject * /*args*/)
{
  BLF_buffer_state_pop(self->buffer_state);
  self->buffer_state = nullptr;

  Py_RETURN_NONE;
}

static void py_blf_bind_imbuf_dealloc(BPyBLFImBufContext *self)
{
  if (self->buffer_state) {
    /* This should practically never happen since it implies
     * `__enter__` is called without a matching `__exit__`.
     * Do this mainly for correctness:
     * if the process somehow exits before exiting the context manager. */
    BLF_buffer_state_free(self->buffer_state);
  }

  PyObject_GC_UnTrack(self);
  Py_CLEAR(self->py_imbuf);
  PyObject_GC_Del(self);
}

static int py_blf_bind_imbuf_traverse(BPyBLFImBufContext *self, visitproc visit, void *arg)
{
  Py_VISIT(self->py_imbuf);
  return 0;
}

static int py_blf_bind_imbuf_clear(BPyBLFImBufContext *self)
{
  Py_CLEAR(self->py_imbuf);
  return 0;
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

static PyMethodDef py_blf_bind_imbuf_methods[] = {
    {"__enter__", (PyCFunction)py_blf_bind_imbuf_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)py_blf_bind_imbuf_exit, METH_VARARGS},
    {nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyTypeObject BPyBLFImBufContext_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BLFImBufContext",
    /*tp_basicsize*/ sizeof(BPyBLFImBufContext),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)py_blf_bind_imbuf_dealloc,
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
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ (traverseproc)py_blf_bind_imbuf_traverse,
    /*tp_clear*/ (inquiry)py_blf_bind_imbuf_clear,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ py_blf_bind_imbuf_methods,
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

/* NOTE(@ideasman42): `BLFImBufContext` isn't accessible from (without creating an instance),
 * it should be exposed although it doesn't seem especially important either. */
PyDoc_STRVAR(
    /* Wrap. */
    py_blf_bind_imbuf_doc,
    ".. method:: bind_imbuf(fontid, image)\n"
    "\n"
    "   Context manager to draw text into an image buffer instead of the GPU's context.\n"
    "\n"
    "   :arg fontid: The id of the typeface as returned by :func:`blf.load`, for default "
    "font use 0.\n"
    "   :type fontid: int\n"
    "   :arg imbuf: The image to draw into.\n"
    "   :type imbuf: :class:`imbuf.types.ImBuf`\n"

    "   :return: The BLF ImBuf context manager.\n"
    "   :rtype: BLFImBufContext\n");
static PyObject *py_blf_bind_imbuf(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
  int fontid;
  PyObject *py_imbuf = nullptr;
  const char *display_name = nullptr;

  static const char *_keywords[] = {
      "",
      "",
      "display_name",
      nullptr,
  };

  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "i" /* `fontid` */
      "O!" /* `image` */
      "|" /* Optional arguments. */
      "z" /* `display_name` */
      ":bind_imbuf",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &fontid, &Py_ImBuf_Type, &py_imbuf, &display_name))
  {
    return nullptr;
  }

  /* Display name is ignored, it is only kept for backwards compatibility. This should
   * always have been the image buffer byte colorspace rather than a display. */

  BPyBLFImBufContext *ret = PyObject_GC_New(BPyBLFImBufContext, &BPyBLFImBufContext_Type);

  ret->py_imbuf = Py_NewRef(py_imbuf);

  ret->fontid = fontid;
  ret->buffer_state = nullptr;

  PyObject_GC_Track(ret);

  return (PyObject *)ret;
}

/** \} */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

/*----------------------------MODULE INIT-------------------------*/
static PyMethodDef BLF_methods[] = {
    {"aspect", (PyCFunction)py_blf_aspect, METH_VARARGS, py_blf_aspect_doc},
    {"clipping", (PyCFunction)py_blf_clipping, METH_VARARGS, py_blf_clipping_doc},
    {"word_wrap", (PyCFunction)py_blf_word_wrap, METH_VARARGS, py_blf_word_wrap_doc},
    {"disable", (PyCFunction)py_blf_disable, METH_VARARGS, py_blf_disable_doc},
    {"dimensions", (PyCFunction)py_blf_dimensions, METH_VARARGS, py_blf_dimensions_doc},
    {"draw", (PyCFunction)py_blf_draw, METH_VARARGS, py_blf_draw_doc},
    {"draw_buffer", (PyCFunction)py_blf_draw_buffer, METH_VARARGS, py_blf_draw_buffer_doc},
    {"enable", (PyCFunction)py_blf_enable, METH_VARARGS, py_blf_enable_doc},
    {"position", (PyCFunction)py_blf_position, METH_VARARGS, py_blf_position_doc},
    {"rotation", (PyCFunction)py_blf_rotation, METH_VARARGS, py_blf_rotation_doc},
    {"shadow", (PyCFunction)py_blf_shadow, METH_VARARGS, py_blf_shadow_doc},
    {"shadow_offset", (PyCFunction)py_blf_shadow_offset, METH_VARARGS, py_blf_shadow_offset_doc},
    {"size", (PyCFunction)py_blf_size, METH_VARARGS, py_blf_size_doc},
    {"color", (PyCFunction)py_blf_color, METH_VARARGS, py_blf_color_doc},
    {"load", (PyCFunction)py_blf_load, METH_VARARGS, py_blf_load_doc},
    {"unload", (PyCFunction)py_blf_unload, METH_VARARGS, py_blf_unload_doc},

    {"bind_imbuf",
     (PyCFunction)py_blf_bind_imbuf,
     METH_VARARGS | METH_KEYWORDS,
     py_blf_bind_imbuf_doc},

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
    BLF_doc,
    "This module provides access to Blender's text drawing functions.");
static PyModuleDef BLF_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "blf",
    /*m_doc*/ BLF_doc,
    /*m_size*/ 0,
    /*m_methods*/ BLF_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_blf()
{
  PyObject *submodule;

  submodule = PyModule_Create(&BLF_module_def);

  PyModule_AddIntConstant(submodule, "ROTATION", BLF_ROTATION);
  PyModule_AddIntConstant(submodule, "CLIPPING", BLF_CLIPPING);
  PyModule_AddIntConstant(submodule, "SHADOW", BLF_SHADOW);
  PyModule_AddIntConstant(submodule, "WORD_WRAP", BLF_WORD_WRAP);
  PyModule_AddIntConstant(submodule, "MONOCHROME", BLF_MONOCHROME);

  return submodule;
}
