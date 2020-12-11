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
 * This file defines the gpu.state API.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "GPU_state.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py_state.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Helper Functions
 * \{ */

static const struct PyC_StringEnumItems pygpu_blend_items[] = {
    {GPU_BLEND_NONE, "NONE"},
    {GPU_BLEND_ALPHA, "ALPHA"},
    {GPU_BLEND_ALPHA_PREMULT, "ALPHA_PREMULT"},
    {GPU_BLEND_ADDITIVE, "ADDITIVE"},
    {GPU_BLEND_ADDITIVE_PREMULT, "ADDITIVE_PREMULT"},
    {GPU_BLEND_MULTIPLY, "MULTIPLY"},
    {GPU_BLEND_SUBTRACT, "SUBTRACT"},
    {GPU_BLEND_INVERT, "INVERT"},
    /**
     * These are quite special cases used inside the draw manager.
     * {GPU_BLEND_OIT, "OIT"},
     * {GPU_BLEND_BACKGROUND, "BACKGROUND"},
     * {GPU_BLEND_CUSTOM, "CUSTOM"},
     */
    {0, NULL},
};

static const struct PyC_StringEnumItems pygpu_depthtest_items[] = {
    {GPU_DEPTH_NONE, "NONE"},
    {GPU_DEPTH_ALWAYS, "ALWAYS"},
    {GPU_DEPTH_LESS, "LESS"},
    {GPU_DEPTH_LESS_EQUAL, "LESS_EQUAL"},
    {GPU_DEPTH_EQUAL, "EQUAL"},
    {GPU_DEPTH_GREATER, "GREATER"},
    {GPU_DEPTH_GREATER_EQUAL, "GREATER_EQUAL"},
    {0, NULL},
};

static const struct PyC_StringEnumItems pygpu_faceculling_items[] = {
    {GPU_CULL_NONE, "NONE"},
    {GPU_CULL_FRONT, "FRONT"},
    {GPU_CULL_BACK, "BACK"},
    {0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manage Stack
 * \{ */

PyDoc_STRVAR(py_state_blend_set_doc,
             ".. function:: blend_set(mode)\n"
             "\n"
             "   Defines the fixed pipeline blending equation.\n"
             "\n"
             "   :param mode: One of these modes: {\n"
             "      `NONE`,\n"
             "      `ALPHA`,\n"
             "      `ALPHA_PREMULT`,\n"
             "      `ADDITIVE`,\n"
             "      `ADDITIVE_PREMULT`,\n"
             "      `MULTIPLY`,\n"
             "      `SUBTRACT`,\n"
             "      `INVERT`,\n"
             //"      `OIT`,\n"
             //"      `BACKGROUND`,\n"
             //"      `CUSTOM`,\n"
             "   :type mode: `str`\n");
static PyObject *py_state_blend_set(PyObject *UNUSED(self), PyObject *value)
{
  const struct PyC_StringEnum pygpu_blend = {&pygpu_blend_items, GPU_BLEND_NONE};
  if (!PyC_ParseStringEnum(value, &pygpu_blend)) {
    return NULL;
  }
  GPU_blend(pygpu_blend.value_found);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_blend_get_doc,
             ".. function:: blend_get()\n"
             "\n"
             "    Current blending equation.\n"
             "\n");
static PyObject *py_state_blend_get(PyObject *UNUSED(self))
{
  eGPUBlend blend = GPU_blend_get();
  return PyUnicode_FromString(PyC_StringEnum_find_id(&pygpu_blend_items, blend));
}

PyDoc_STRVAR(py_state_depth_test_set_doc,
             ".. function:: depth_test_set(mode)\n"
             "\n"
             "   Defines the depth_test equation.\n"
             "\n"
             "   :param mode: One of these modes: {\n"
             "      `NONE`,\n"
             "      `ALWAYS`,\n"
             "      `LESS`,\n"
             "      `LESS_EQUAL`,\n"
             "      `EQUAL`,\n"
             "      `GREATER`,\n"
             "      `GREATER_EQUAL`,\n"
             "   :type mode: `str`\n");
static PyObject *py_state_depth_test_set(PyObject *UNUSED(self), PyObject *value)
{
  const struct PyC_StringEnum pygpu_depth_test = {&pygpu_depthtest_items, GPU_DEPTH_NONE};
  if (!PyC_ParseStringEnum(value, &pygpu_depth_test)) {
    return NULL;
  }
  GPU_depth_test(pygpu_depth_test.value_found);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_depth_test_get_doc,
             ".. function:: blend_depth_test_get()\n"
             "\n"
             "    Current depth_test equation.\n"
             "\n");
static PyObject *py_state_depth_test_get(PyObject *UNUSED(self))
{
  eGPUDepthTest test = GPU_depth_test_get();
  return PyUnicode_FromString(PyC_StringEnum_find_id(&pygpu_depthtest_items, test));
}

PyDoc_STRVAR(py_state_face_culling_doc,
             ".. function:: face_culling(culling)\n"
             "\n"
             "   Specify whether none, front-facing or back-facing facets can be culled.\n"
             "\n"
             "   :param mode: One of these modes: {\n"
             "      `NONE`,\n"
             "      `FRONT`,\n"
             "      `BACK`,\n"
             "   :type mode: `str`\n");
static PyObject *py_state_face_culling(PyObject *UNUSED(self), PyObject *value)
{
  const struct PyC_StringEnum pygpu_faceculling = {&pygpu_faceculling_items, GPU_CULL_NONE};
  if (!PyC_ParseStringEnum(value, &pygpu_faceculling)) {
    return NULL;
  }

  GPU_face_culling(pygpu_faceculling.value_found);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_front_facing_doc,
             ".. function:: front_facing(invert)\n"
             "\n"
             "   Specifies the orientation of front-facing polygons.\n"
             "\n"
             "   :param invert: True for clockwise polygons as front-facing.\n"
             "   :type mode: `bool`\n");
static PyObject *py_state_front_facing(PyObject *UNUSED(self), PyObject *value)
{
  bool invert;
  if (!PyC_ParseBool(value, &invert)) {
    return NULL;
  }

  GPU_front_facing(invert);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_point_size_doc,
             ".. function:: point_size(size)\n"
             "\n"
             "   Specify the diameter of rasterized points.\n"
             "\n"
             "   :param size: New diameter.\n"
             "   :type mode: `float`\n");
static PyObject *py_state_point_size(PyObject *UNUSED(self), PyObject *value)
{
  float size = (float)PyFloat_AsDouble(value);
  if (PyErr_Occurred()) {
    return NULL;
  }

  GPU_point_size(size);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_line_width_doc,
             ".. function:: line_width(width)\n"
             "\n"
             "   Specify the width of rasterized lines.\n"
             "\n"
             "   :param size: New width.\n"
             "   :type mode: `float`\n");
static PyObject *py_state_line_width(PyObject *UNUSED(self), PyObject *value)
{
  float width = (float)PyFloat_AsDouble(value);
  if (PyErr_Occurred()) {
    return NULL;
  }

  GPU_line_width(width);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_viewport_doc,
             ".. function:: viewport(x, y, width, height)\n"
             "\n"
             "   Specifies the viewport of the active framebuffer.\n"
             "\n"
             "   :param x, y: lower left corner of the viewport rectangle, in pixels.\n"
             "   :param width, height: width and height of the viewport.\n"
             "   :type x, y, width, height: `int`\n");
static int py_state_viewport(PyObject *UNUSED(self), PyObject *args)
{
  int x, y, width, height;
  if (!PyArg_ParseTuple(args, "iiii:viewport", &x, &y, &width, &height)) {
    return NULL;
  }

  GPU_viewport(x, y, width, height);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_color_mask_doc,
             ".. function:: color_mask(r, g, b, a)\n"
             "\n"
             "   Enable or disable writing of frame buffer color components.\n"
             "\n"
             "   :param r, g, b, a: components red, green, blue, and alpha.\n"
             "   :type r, g, b, a: `bool`\n");
static int py_state_color_mask(PyObject *UNUSED(self), PyObject *args)
{
  int r, g, b, a;
  if (!PyArg_ParseTuple(args, "pppp:color_mask", &r, &g, &b, &a)) {
    return NULL;
  }

  GPU_color_mask((bool)r, (bool)g, (bool)b, (bool)a);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_state_depth_mask_doc,
             ".. function:: depth_mask_set(value)\n"
             "\n"
             "   Write to depth component.\n"
             "\n"
             "   :param value: True for writing to the depth component.\n"
             "   :type near: `bool`\n");
static PyObject *py_state_depth_mask(PyObject *UNUSED(self), PyObject *value)
{
  bool write_to_depth;
  if (!PyC_ParseBool(value, &write_to_depth)) {
    return NULL;
  }
  GPU_depth_mask(write_to_depth);
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static struct PyMethodDef bpygpu_py_state_methods[] = {
    /* Manage Stack */
    {"blend_set", (PyCFunction)py_state_blend_set, METH_O, py_state_blend_set_doc},
    {"blend_get", (PyCFunction)py_state_blend_get, METH_NOARGS, py_state_blend_get_doc},
    {"depth_test_set", (PyCFunction)py_state_depth_test_set, METH_O, py_state_depth_test_set_doc},
    {"depth_test_get",
     (PyCFunction)py_state_depth_test_get,
     METH_NOARGS,
     py_state_depth_test_get_doc},
    {"face_culling", (PyCFunction)py_state_face_culling, METH_O, py_state_face_culling_doc},
    {"front_facing", (PyCFunction)py_state_front_facing, METH_O, py_state_front_facing_doc},
    {"point_size", (PyCFunction)py_state_point_size, METH_O, py_state_point_size_doc},
    {"line_width", (PyCFunction)py_state_line_width, METH_O, py_state_line_width_doc},
    {"viewport", (PyCFunction)py_state_viewport, METH_VARARGS, py_state_viewport_doc},
    {"color_mask", (PyCFunction)py_state_color_mask, METH_VARARGS, py_state_color_mask_doc},
    {"depth_mask", (PyCFunction)py_state_depth_mask, METH_O, py_state_depth_mask_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(bpygpu_py_state_doc, "This module provides access to the gpu state.");
static PyModuleDef BPyGPU_py_state_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.state",
    .m_doc = bpygpu_py_state_doc,
    .m_methods = bpygpu_py_state_methods,
};

PyObject *BPyInit_gpu_state(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPyGPU_py_state_module_def);

  return submodule;
}

/** \} */
