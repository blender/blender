/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the gpu.state API.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "GPU_framebuffer.hh"
#include "GPU_state.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"

#include "gpu_py.hh"
#include "gpu_py_framebuffer.hh"
#include "gpu_py_state.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Helper Functions
 * \{ */

static const PyC_StringEnumItems pygpu_state_blend_items[] = {
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
    {0, nullptr},
};

static const PyC_StringEnumItems pygpu_state_depthtest_items[] = {
    {GPU_DEPTH_NONE, "NONE"},
    {GPU_DEPTH_ALWAYS, "ALWAYS"},
    {GPU_DEPTH_LESS, "LESS"},
    {GPU_DEPTH_LESS_EQUAL, "LESS_EQUAL"},
    {GPU_DEPTH_EQUAL, "EQUAL"},
    {GPU_DEPTH_GREATER, "GREATER"},
    {GPU_DEPTH_GREATER_EQUAL, "GREATER_EQUAL"},
    {0, nullptr},
};

static const PyC_StringEnumItems pygpu_state_faceculling_items[] = {
    {GPU_CULL_NONE, "NONE"},
    {GPU_CULL_FRONT, "FRONT"},
    {GPU_CULL_BACK, "BACK"},
    {0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manage Stack
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_blend_set_doc,
    ".. function:: blend_set(mode)\n"
    "\n"
    "   Defines the fixed pipeline blending equation.\n"
    "\n"
    "   :arg mode: The type of blend mode.\n"
    "\n"
    "      * ``NONE`` No blending.\n"
    "      * ``ALPHA`` The original color channels are interpolated according to the alpha "
    "value.\n"
    "      * ``ALPHA_PREMULT`` The original color channels are interpolated according to the "
    "alpha value with the new colors pre-multiplied by this value.\n"
    "      * ``ADDITIVE`` The original color channels are added by the corresponding ones.\n"
    "      * ``ADDITIVE_PREMULT`` The original color channels are added by the corresponding ones "
    "that are pre-multiplied by the alpha value.\n"
    "      * ``MULTIPLY`` The original color channels are multiplied by the corresponding ones.\n"
    "      * ``SUBTRACT`` The original color channels are subtracted by the corresponding ones.\n"
    "      * ``INVERT`` The original color channels are replaced by its complementary color.\n"
    //"      * ``OIT``.\n"
    //"      * ``BACKGROUND`` .\n"
    //"      * ``CUSTOM`` .\n"
    "   :type mode: str\n");
static PyObject *pygpu_state_blend_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  PyC_StringEnum pygpu_blend = {pygpu_state_blend_items};
  if (!PyC_ParseStringEnum(value, &pygpu_blend)) {
    return nullptr;
  }
  GPU_blend(GPUBlend(pygpu_blend.value_found));
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_blend_get_doc,
    ".. function:: blend_get()\n"
    "\n"
    "    Current blending equation.\n"
    "\n");
static PyObject *pygpu_state_blend_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  GPUBlend blend = GPU_blend_get();
  return PyUnicode_FromString(PyC_StringEnum_FindIDFromValue(pygpu_state_blend_items, blend));
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_clip_distances_set_doc,
    ".. function:: clip_distances_set(distances_enabled)\n"
    "\n"
    "   Sets the number of ``gl_ClipDistance`` planes used for clip geometry.\n"
    "\n"
    "   :arg distances_enabled: Number of clip distances enabled.\n"
    "   :type distances_enabled: int\n");
static PyObject *pygpu_state_clip_distances_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  int distances_enabled = int(PyLong_AsUnsignedLong(value));
  if (distances_enabled == -1) {
    return nullptr;
  }

  if (distances_enabled > 6) {
    PyErr_SetString(PyExc_ValueError, "too many distances enabled, max is 6");
  }

  GPU_clip_distances(distances_enabled);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_depth_test_set_doc,
    ".. function:: depth_test_set(mode)\n"
    "\n"
    "   Defines the depth_test equation.\n"
    "\n"
    "   :arg mode: The depth test equation name.\n"
    "      Possible values are ``NONE``, ``ALWAYS``, ``LESS``, ``LESS_EQUAL``, ``EQUAL``, "
    "``GREATER`` and ``GREATER_EQUAL``.\n"
    "   :type mode: str\n");
static PyObject *pygpu_state_depth_test_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  PyC_StringEnum pygpu_depth_test = {pygpu_state_depthtest_items};
  if (!PyC_ParseStringEnum(value, &pygpu_depth_test)) {
    return nullptr;
  }
  GPU_depth_test(GPUDepthTest(pygpu_depth_test.value_found));
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_depth_test_get_doc,
    ".. function:: depth_test_get()\n"
    "\n"
    "    Current depth_test equation.\n"
    "\n");
static PyObject *pygpu_state_depth_test_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  GPUDepthTest test = GPU_depth_test_get();
  return PyUnicode_FromString(PyC_StringEnum_FindIDFromValue(pygpu_state_depthtest_items, test));
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_depth_mask_set_doc,
    ".. function:: depth_mask_set(value)\n"
    "\n"
    "   Write to depth component.\n"
    "\n"
    "   :arg value: True for writing to the depth component.\n"
    "   :type near: bool\n");
static PyObject *pygpu_state_depth_mask_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  bool write_to_depth;
  if (!PyC_ParseBool(value, &write_to_depth)) {
    return nullptr;
  }
  GPU_depth_mask(write_to_depth);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_depth_mask_get_doc,
    ".. function:: depth_mask_get()\n"
    "\n"
    "   Writing status in the depth component.\n");
static PyObject *pygpu_state_depth_mask_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  return PyBool_FromLong(GPU_depth_mask_get());
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_viewport_set_doc,
    ".. function:: viewport_set(x, y, xsize, ysize)\n"
    "\n"
    "   Specifies the viewport of the active framebuffer.\n"
    "   Note: The viewport state is not saved upon framebuffer rebind.\n"
    "\n"
    "   :arg x, y: lower left corner of the viewport_set rectangle, in pixels.\n"
    "   :type x, y: int\n"
    "   :arg xsize, ysize: width and height of the viewport_set.\n"
    "   :type xsize, ysize: int\n");
static PyObject *pygpu_state_viewport_set(PyObject * /*self*/, PyObject *args)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  int x, y, xsize, ysize;
  if (!PyArg_ParseTuple(args, "iiii:viewport_set", &x, &y, &xsize, &ysize)) {
    return nullptr;
  }

  GPU_viewport(x, y, xsize, ysize);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_viewport_get_doc,
    ".. function:: viewport_get()\n"
    "\n"
    "   Viewport of the active framebuffer.\n");
static PyObject *pygpu_state_viewport_get(PyObject * /*self*/, PyObject * /*args*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  int viewport[4];
  GPU_viewport_size_get_i(viewport);

  PyObject *ret = PyTuple_New(4);
  PyTuple_SET_ITEMS(ret,
                    PyLong_FromLong(viewport[0]),
                    PyLong_FromLong(viewport[1]),
                    PyLong_FromLong(viewport[2]),
                    PyLong_FromLong(viewport[3]));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_scissor_set_doc,
    ".. function:: scissor_set(x, y, xsize, ysize)\n"
    "\n"
    "   Specifies the scissor area of the active framebuffer.\n"
    "   Note: The scissor state is not saved upon framebuffer rebind.\n"
    "\n"
    "   :arg x, y: lower left corner of the scissor rectangle, in pixels.\n"
    "   :type x, y: int\n"
    "   :arg xsize, ysize: width and height of the scissor rectangle.\n"
    "   :type xsize, ysize: int\n");
static PyObject *pygpu_state_scissor_set(PyObject * /*self*/, PyObject *args)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  int x, y, xsize, ysize;
  if (!PyArg_ParseTuple(args, "iiii:scissor_set", &x, &y, &xsize, &ysize)) {
    return nullptr;
  }

  GPU_scissor(x, y, xsize, ysize);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_scissor_get_doc,
    ".. function:: scissor_get()\n"
    "\n"
    "   Retrieve the scissors of the active framebuffer.\n"
    "   Note: Only valid between 'scissor_set' and a framebuffer rebind.\n"
    "\n"
    "   :return: The scissor of the active framebuffer as a tuple\n"
    "        (x, y, xsize, ysize).\n"
    "        x, y: lower left corner of the scissor rectangle, in pixels.\n"
    "        xsize, ysize: width and height of the scissor rectangle.\n"
    "   :rtype: tuple[int, int, int, int]\n");
static PyObject *pygpu_state_scissor_get(PyObject * /*self*/, PyObject * /*args*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  int scissor[4];
  GPU_scissor_get(scissor);

  PyObject *ret = PyTuple_New(4);
  PyTuple_SET_ITEMS(ret,
                    PyLong_FromLong(scissor[0]),
                    PyLong_FromLong(scissor[1]),
                    PyLong_FromLong(scissor[2]),
                    PyLong_FromLong(scissor[3]));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_scissor_test_set_doc,
    ".. function:: scissor_test_set(enable)\n"
    "\n"
    "   Enable/disable scissor testing on the active framebuffer.\n"
    "\n"
    "   :arg enable:\n"
    "        True - enable scissor testing.\n"
    "        False - disable scissor testing.\n"
    "   :type enable: bool\n");
static PyObject *pygpu_state_scissor_test_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  bool enabled;
  if (!PyC_ParseBool(value, &enabled)) {
    return nullptr;
  }

  GPU_scissor_test(enabled);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_line_width_set_doc,
    ".. function:: line_width_set(width)\n"
    "\n"
    "   Specify the width of rasterized lines.\n"
    "\n"
    "   :arg size: New width.\n"
    "   :type mode: float\n");
static PyObject *pygpu_state_line_width_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  float width = float(PyFloat_AsDouble(value));
  if (PyErr_Occurred()) {
    return nullptr;
  }

  GPU_line_width(width);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_line_width_get_doc,
    ".. function:: line_width_get()\n"
    "\n"
    "   Current width of rasterized lines.\n");
static PyObject *pygpu_state_line_width_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  float width = GPU_line_width_get();
  return PyFloat_FromDouble(double(width));
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_point_size_set_doc,
    ".. function:: point_size_set(size)\n"
    "\n"
    "   Specify the diameter of rasterized points.\n"
    "\n"
    "   :arg size: New diameter.\n"
    "   :type mode: float\n");
static PyObject *pygpu_state_point_size_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  float size = float(PyFloat_AsDouble(value));
  if (PyErr_Occurred()) {
    return nullptr;
  }

  GPU_point_size(size);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_color_mask_set_doc,
    ".. function:: color_mask_set(r, g, b, a)\n"
    "\n"
    "   Enable or disable writing of frame buffer color components.\n"
    "\n"
    "   :arg r, g, b, a: components red, green, blue, and alpha.\n"
    "   :type r, g, b, a: bool\n");
static PyObject *pygpu_state_color_mask_set(PyObject * /*self*/, PyObject *args)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  int r, g, b, a;
  if (!PyArg_ParseTuple(args, "pppp:color_mask_set", &r, &g, &b, &a)) {
    return nullptr;
  }

  GPU_color_mask(bool(r), bool(g), bool(b), bool(a));
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_face_culling_set_doc,
    ".. function:: face_culling_set(culling)\n"
    "\n"
    "   Specify whether none, front-facing or back-facing facets can be culled.\n"
    "\n"
    "   :arg mode: ``NONE``, ``FRONT`` or ``BACK``.\n"
    "   :type mode: str\n");
static PyObject *pygpu_state_face_culling_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  PyC_StringEnum pygpu_faceculling = {pygpu_state_faceculling_items};
  if (!PyC_ParseStringEnum(value, &pygpu_faceculling)) {
    return nullptr;
  }

  GPU_face_culling(GPUFaceCullTest(pygpu_faceculling.value_found));
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_front_facing_set_doc,
    ".. function:: front_facing_set(invert)\n"
    "\n"
    "   Specifies the orientation of front-facing polygons.\n"
    "\n"
    "   :arg invert: True for clockwise polygons as front-facing.\n"
    "   :type mode: bool\n");
static PyObject *pygpu_state_front_facing_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  bool invert;
  if (!PyC_ParseBool(value, &invert)) {
    return nullptr;
  }

  GPU_front_facing(invert);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_program_point_size_set_doc,
    ".. function:: program_point_size_set(enable)\n"
    "\n"
    "   If enabled, the derived point size is taken from the (potentially clipped) "
    "shader builtin gl_PointSize.\n"
    "\n"
    "   :arg enable: True for shader builtin gl_PointSize.\n"
    "   :type enable: bool\n");
static PyObject *pygpu_state_program_point_size_set(PyObject * /*self*/, PyObject *value)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  bool enable;
  if (!PyC_ParseBool(value, &enable)) {
    return nullptr;
  }

  GPU_program_point_size(enable);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_state_active_framebuffer_get_doc,
    ".. function:: active_framebuffer_get(enable)\n"
    "\n"
    "   Return the active frame-buffer in context.\n");
static PyObject *pygpu_state_active_framebuffer_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  blender::gpu::FrameBuffer *fb = GPU_framebuffer_active_get();
  return BPyGPUFrameBuffer_CreatePyObject(fb, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
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

static PyMethodDef pygpu_state__tp_methods[] = {
    /* Manage Stack */
    {"blend_set", (PyCFunction)pygpu_state_blend_set, METH_O, pygpu_state_blend_set_doc},
    {"blend_get", (PyCFunction)pygpu_state_blend_get, METH_NOARGS, pygpu_state_blend_get_doc},
    {"clip_distances_set",
     (PyCFunction)pygpu_state_clip_distances_set,
     METH_O,
     pygpu_state_clip_distances_set_doc},
    {"depth_test_set",
     (PyCFunction)pygpu_state_depth_test_set,
     METH_O,
     pygpu_state_depth_test_set_doc},
    {"depth_test_get",
     (PyCFunction)pygpu_state_depth_test_get,
     METH_NOARGS,
     pygpu_state_depth_test_get_doc},
    {"depth_mask_set",
     (PyCFunction)pygpu_state_depth_mask_set,
     METH_O,
     pygpu_state_depth_mask_set_doc},
    {"depth_mask_get",
     (PyCFunction)pygpu_state_depth_mask_get,
     METH_NOARGS,
     pygpu_state_depth_mask_get_doc},
    {"viewport_set",
     (PyCFunction)pygpu_state_viewport_set,
     METH_VARARGS,
     pygpu_state_viewport_set_doc},
    {"viewport_get",
     (PyCFunction)pygpu_state_viewport_get,
     METH_NOARGS,
     pygpu_state_viewport_get_doc},
    {"scissor_set",
     (PyCFunction)pygpu_state_scissor_set,
     METH_VARARGS,
     pygpu_state_scissor_set_doc},
    {"scissor_get",
     (PyCFunction)pygpu_state_scissor_get,
     METH_NOARGS,
     pygpu_state_scissor_get_doc},
    {"scissor_test_set",
     (PyCFunction)pygpu_state_scissor_test_set,
     METH_O,
     pygpu_state_scissor_test_set_doc},
    {"line_width_set",
     (PyCFunction)pygpu_state_line_width_set,
     METH_O,
     pygpu_state_line_width_set_doc},
    {"line_width_get",
     (PyCFunction)pygpu_state_line_width_get,
     METH_NOARGS,
     pygpu_state_line_width_get_doc},
    {"point_size_set",
     (PyCFunction)pygpu_state_point_size_set,
     METH_O,
     pygpu_state_point_size_set_doc},
    {"color_mask_set",
     (PyCFunction)pygpu_state_color_mask_set,
     METH_VARARGS,
     pygpu_state_color_mask_set_doc},
    {"face_culling_set",
     (PyCFunction)pygpu_state_face_culling_set,
     METH_O,
     pygpu_state_face_culling_set_doc},
    {"front_facing_set",
     (PyCFunction)pygpu_state_front_facing_set,
     METH_O,
     pygpu_state_front_facing_set_doc},
    {"program_point_size_set",
     (PyCFunction)pygpu_state_program_point_size_set,
     METH_O,
     pygpu_state_program_point_size_set_doc},
    {"active_framebuffer_get",
     (PyCFunction)pygpu_state_active_framebuffer_get,
     METH_NOARGS,
     pygpu_state_active_framebuffer_get_doc},
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
    pygpu_state__tp_doc,
    "This module provides access to the gpu state.");
static PyModuleDef pygpu_state_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.state",
    /*m_doc*/ pygpu_state__tp_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_state__tp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpygpu_state_init()
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_state_module_def);

  return submodule;
}

/** \} */
