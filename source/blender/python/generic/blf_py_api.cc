/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 *
 * This file defines the `bgl` module, used for drawing text in OpenGL.
 */

/* Future-proof, See https://docs.python.org/3/c-api/arg.html#strings-and-buffers */
#define PY_SSIZE_T_CLEAN

#include "blf_py_api.hh"

#include "../generic/py_capi_utils.hh"

#include <Python.h>

#include "../../blenfont/BLF_api.hh"

#include "BLI_utildefines.h"

#include "python_utildefines.hh"

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
  float r_width, r_height;
  PyObject *ret;
  int fontid;

  if (!PyArg_ParseTuple(args, "is:blf.dimensions", &fontid, &text)) {
    return nullptr;
  }

  BLF_width_and_height(fontid, text, INT_MAX, &r_width, &r_height);

  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, PyFloat_FromDouble(r_width), PyFloat_FromDouble(r_height));
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

  BLF_disable(fontid, option);

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

  BLF_enable(fontid, option);

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

/*----------------------------MODULE INIT-------------------------*/
static PyMethodDef BLF_methods[] = {
    {"aspect", (PyCFunction)py_blf_aspect, METH_VARARGS, py_blf_aspect_doc},
    {"clipping", (PyCFunction)py_blf_clipping, METH_VARARGS, py_blf_clipping_doc},
    {"word_wrap", (PyCFunction)py_blf_word_wrap, METH_VARARGS, py_blf_word_wrap_doc},
    {"disable", (PyCFunction)py_blf_disable, METH_VARARGS, py_blf_disable_doc},
    {"dimensions", (PyCFunction)py_blf_dimensions, METH_VARARGS, py_blf_dimensions_doc},
    {"draw", (PyCFunction)py_blf_draw, METH_VARARGS, py_blf_draw_doc},
    {"enable", (PyCFunction)py_blf_enable, METH_VARARGS, py_blf_enable_doc},
    {"position", (PyCFunction)py_blf_position, METH_VARARGS, py_blf_position_doc},
    {"rotation", (PyCFunction)py_blf_rotation, METH_VARARGS, py_blf_rotation_doc},
    {"shadow", (PyCFunction)py_blf_shadow, METH_VARARGS, py_blf_shadow_doc},
    {"shadow_offset", (PyCFunction)py_blf_shadow_offset, METH_VARARGS, py_blf_shadow_offset_doc},
    {"size", (PyCFunction)py_blf_size, METH_VARARGS, py_blf_size_doc},
    {"color", (PyCFunction)py_blf_color, METH_VARARGS, py_blf_color_doc},
    {"load", (PyCFunction)py_blf_load, METH_VARARGS, py_blf_load_doc},
    {"unload", (PyCFunction)py_blf_unload, METH_VARARGS, py_blf_unload_doc},
    {nullptr, nullptr, 0, nullptr},
};

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
