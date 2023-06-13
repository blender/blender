/* SPDX-FileCopyrightText: 2015 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the offscreen functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_scene.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"
#include "GPU_viewport.h"

#include "ED_view3d_offscreen.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py.h"
#include "gpu_py_texture.h"

#include "gpu_py_offscreen.h" /* own include */

/* Define the free method to avoid breakage. */
#define BPYGPU_USE_GPUOBJ_FREE_METHOD

/* -------------------------------------------------------------------- */
/** \name GPUOffScreen Common Utilities
 * \{ */

static const struct PyC_StringEnumItems pygpu_framebuffer_color_texture_formats[] = {
    {GPU_RGBA8, "RGBA8"},
    {GPU_RGBA16, "RGBA16"},
    {GPU_RGBA16F, "RGBA16F"},
    {GPU_RGBA32F, "RGBA32F"},
    {0, NULL},
};

static int pygpu_offscreen_valid_check(BPyGPUOffScreen *py_ofs)
{
  if (UNLIKELY(py_ofs->ofs == NULL)) {
    PyErr_SetString(PyExc_ReferenceError,
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
                    "GPU offscreen was freed, no further access is valid"
#else
                    "GPU offscreen: internal error"
#endif
    );
    return -1;
  }
  return 0;
}

#define BPY_GPU_OFFSCREEN_CHECK_OBJ(bpygpu) \
  { \
    if (UNLIKELY(pygpu_offscreen_valid_check(bpygpu) == -1)) { \
      return NULL; \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stack (Context Manager)
 *
 * Safer alternative to ensure balanced push/pop calls.
 *
 * \{ */

typedef struct {
  PyObject_HEAD /* Required Python macro. */
  BPyGPUOffScreen *py_offscreen;
  int level;
  bool is_explicitly_bound; /* Bound by "bind" method. */
} OffScreenStackContext;

static void pygpu_offscreen_stack_context__tp_dealloc(OffScreenStackContext *self)
{
  Py_DECREF(self->py_offscreen);
  PyObject_DEL(self);
}

static PyObject *pygpu_offscreen_stack_context_enter(OffScreenStackContext *self)
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self->py_offscreen);

  if (!self->is_explicitly_bound) {
    if (self->level != -1) {
      PyErr_SetString(PyExc_RuntimeError, "Already in use");
      return NULL;
    }

    GPU_offscreen_bind(self->py_offscreen->ofs, true);
    self->level = GPU_framebuffer_stack_level_get();
  }

  Py_RETURN_NONE;
}

static PyObject *pygpu_offscreen_stack_context_exit(OffScreenStackContext *self,
                                                    PyObject *UNUSED(args))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self->py_offscreen);

  if (self->level == -1) {
    PyErr_SetString(PyExc_RuntimeError, "Not yet in use\n");
    return NULL;
  }

  const int level = GPU_framebuffer_stack_level_get();
  if (level != self->level) {
    PyErr_Format(
        PyExc_RuntimeError, "Level of bind mismatch, expected %d, got %d\n", self->level, level);
  }

  GPU_offscreen_unbind(self->py_offscreen->ofs, true);
  Py_RETURN_NONE;
}

static PyMethodDef pygpu_offscreen_stack_context__tp_methods[] = {
    {"__enter__", (PyCFunction)pygpu_offscreen_stack_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)pygpu_offscreen_stack_context_exit, METH_VARARGS},
    {NULL},
};

static PyTypeObject PyGPUOffscreenStackContext_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUFrameBufferStackContext",
    .tp_basicsize = sizeof(OffScreenStackContext),
    .tp_dealloc = (destructor)pygpu_offscreen_stack_context__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = pygpu_offscreen_stack_context__tp_methods,
};

PyDoc_STRVAR(pygpu_offscreen_bind_doc,
             ".. function:: bind()\n"
             "\n"
             "   Context manager to ensure balanced bind calls, even in the case of an error.\n");
static PyObject *pygpu_offscreen_bind(BPyGPUOffScreen *self)
{
  OffScreenStackContext *ret = PyObject_New(OffScreenStackContext,
                                            &PyGPUOffscreenStackContext_Type);
  ret->py_offscreen = self;
  ret->level = -1;
  ret->is_explicitly_bound = false;
  Py_INCREF(self);

  pygpu_offscreen_stack_context_enter(ret);
  ret->is_explicitly_bound = true;

  return (PyObject *)ret;
}

PyDoc_STRVAR(pygpu_offscreen_unbind_doc,
             ".. method:: unbind(restore=True)\n"
             "\n"
             "   Unbind the offscreen object.\n"
             "\n"
             "   :arg restore: Restore the OpenGL state, can only be used when the state has been "
             "saved before.\n"
             "   :type restore: bool\n");
static PyObject *pygpu_offscreen_unbind(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
  bool restore = true;

  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

  static const char *_keywords[] = {"restore", NULL};
  static _PyArg_Parser _parser = {
      "|$" /* Optional keyword only arguments. */
      "O&" /* `restore` */
      ":unbind",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, PyC_ParseBool, &restore)) {
    return NULL;
  }

  GPU_offscreen_unbind(self->ofs, restore);
  GPU_apply_state();
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUOffscreen Type
 * \{ */

static PyObject *pygpu_offscreen__tp_new(PyTypeObject *UNUSED(self),
                                         PyObject *args,
                                         PyObject *kwds)
{
  GPUOffScreen *ofs = NULL;
  int width, height;
  struct PyC_StringEnum pygpu_textureformat = {pygpu_framebuffer_color_texture_formats, GPU_RGBA8};
  char err_out[256];

  static const char *_keywords[] = {"width", "height", "format", NULL};
  static _PyArg_Parser _parser = {
      "i"  /* `width` */
      "i"  /* `height` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `format` */
      ":GPUOffScreen.__new__",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &width, &height, PyC_ParseStringEnum, &pygpu_textureformat))
  {
    return NULL;
  }

  if (GPU_context_active_get()) {
    ofs = GPU_offscreen_create(width,
                               height,
                               true,
                               pygpu_textureformat.value_found,
                               GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_HOST_READ,
                               err_out);
  }
  else {
    STRNCPY(err_out, "No active GPU context found");
  }

  if (ofs == NULL) {
    PyErr_Format(PyExc_RuntimeError,
                 "gpu.offscreen.new(...) failed with '%s'",
                 err_out[0] ? err_out : "unknown error");
    return NULL;
  }

  return BPyGPUOffScreen_CreatePyObject(ofs);
}

PyDoc_STRVAR(pygpu_offscreen_width_doc, "Width of the texture.\n\n:type: `int`");
static PyObject *pygpu_offscreen_width_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  return PyLong_FromLong(GPU_offscreen_width(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_height_doc, "Height of the texture.\n\n:type: `int`");
static PyObject *pygpu_offscreen_height_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  return PyLong_FromLong(GPU_offscreen_height(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_color_texture_doc,
             "OpenGL bindcode for the color texture.\n\n:type: `int`");
static PyObject *pygpu_offscreen_color_texture_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  GPUTexture *texture = GPU_offscreen_color_texture(self->ofs);
  return PyLong_FromLong(GPU_texture_opengl_bindcode(texture));
}

PyDoc_STRVAR(pygpu_offscreen_texture_color_doc,
             "The color texture attached.\n"
             "\n"
             ":type: :class:`gpu.types.GPUTexture`");
static PyObject *pygpu_offscreen_texture_color_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  GPUTexture *texture = GPU_offscreen_color_texture(self->ofs);
  return BPyGPUTexture_CreatePyObject(texture, true);
}

PyDoc_STRVAR(
    pygpu_offscreen_draw_view3d_doc,
    ".. method:: draw_view3d(scene, view_layer, view3d, region, view_matrix, projection_matrix, "
    "do_color_management=False, draw_background=True)\n"
    "\n"
    "   Draw the 3d viewport in the offscreen object.\n"
    "\n"
    "   :arg scene: Scene to draw.\n"
    "   :type scene: :class:`bpy.types.Scene`\n"
    "   :arg view_layer: View layer to draw.\n"
    "   :type view_layer: :class:`bpy.types.ViewLayer`\n"
    "   :arg view3d: 3D View to get the drawing settings from.\n"
    "   :type view3d: :class:`bpy.types.SpaceView3D`\n"
    "   :arg region: Region of the 3D View (required as temporary draw target).\n"
    "   :type region: :class:`bpy.types.Region`\n"
    "   :arg view_matrix: View Matrix (e.g. ``camera.matrix_world.inverted()``).\n"
    "   :type view_matrix: :class:`mathutils.Matrix`\n"
    "   :arg projection_matrix: Projection Matrix (e.g. ``camera.calc_matrix_camera(...)``).\n"
    "   :type projection_matrix: :class:`mathutils.Matrix`\n"
    "   :arg do_color_management: Color manage the output.\n"
    "   :type do_color_management: bool\n"
    "   :arg draw_background: Draw background.\n"
    "   :type draw_background: bool\n");
static PyObject *pygpu_offscreen_draw_view3d(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
  MatrixObject *py_mat_view, *py_mat_projection;
  PyObject *py_scene, *py_view_layer, *py_region, *py_view3d;

  struct Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  View3D *v3d;
  ARegion *region;

  bool do_color_management = false;
  bool draw_background = true;

  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

  static const char *_keywords[] = {
      "scene",
      "view_layer",
      "view3d",
      "region",
      "view_matrix",
      "projection_matrix",
      "do_color_management",
      "draw_background",
      NULL,
  };
  static _PyArg_Parser _parser = {
      "O"  /* `scene` */
      "O"  /* `view_layer` */
      "O"  /* `view3d` */
      "O"  /* `region` */
      "O&" /* `view_matrix` */
      "O&" /* `projection_matrix` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `do_color_management` */
      "O&" /* `draw_background` */
      ":draw_view3d",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &py_scene,
                                        &py_view_layer,
                                        &py_view3d,
                                        &py_region,
                                        Matrix_Parse4x4,
                                        &py_mat_view,
                                        Matrix_Parse4x4,
                                        &py_mat_projection,
                                        PyC_ParseBool,
                                        &do_color_management,
                                        PyC_ParseBool,
                                        &draw_background) ||
      (!(scene = PyC_RNA_AsPointer(py_scene, "Scene")) ||
       !(view_layer = PyC_RNA_AsPointer(py_view_layer, "ViewLayer")) ||
       !(v3d = PyC_RNA_AsPointer(py_view3d, "SpaceView3D")) ||
       !(region = PyC_RNA_AsPointer(py_region, "Region"))))
  {
    return NULL;
  }

  BLI_assert(BKE_id_is_in_global_main(&scene->id));

  depsgraph = BKE_scene_ensure_depsgraph(G_MAIN, scene, view_layer);

  /* Disable 'bgl' state since it interfere with off-screen drawing, see: #84402. */
  const bool is_bgl = GPU_bgl_get();
  if (is_bgl) {
    GPU_bgl_end();
  }

  GPU_offscreen_bind(self->ofs, true);

  /* Cache the #GPUViewport so the frame-buffers and associated textures are
   * not reallocated each time, see: #89204 */
  if (!self->viewport) {
    self->viewport = GPU_viewport_create();
  }
  else {
    GPU_viewport_tag_update(self->viewport);
  }

  ED_view3d_draw_offscreen(depsgraph,
                           scene,
                           v3d->shading.type,
                           v3d,
                           region,
                           GPU_offscreen_width(self->ofs),
                           GPU_offscreen_height(self->ofs),
                           (const float(*)[4])py_mat_view->matrix,
                           (const float(*)[4])py_mat_projection->matrix,
                           true,
                           draw_background,
                           "",
                           do_color_management,
                           true,
                           self->ofs,
                           self->viewport);

  GPU_offscreen_unbind(self->ofs, true);

  if (is_bgl) {
    GPU_bgl_start();
  }

  Py_RETURN_NONE;
}

#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
PyDoc_STRVAR(pygpu_offscreen_free_doc,
             ".. method:: free()\n"
             "\n"
             "   Free the offscreen object.\n"
             "   The framebuffer, texture and render objects will no longer be accessible.\n");
static PyObject *pygpu_offscreen_free(BPyGPUOffScreen *self)
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

  if (self->viewport) {
    GPU_viewport_free(self->viewport);
    self->viewport = NULL;
  }

  GPU_offscreen_free(self->ofs);
  self->ofs = NULL;
  Py_RETURN_NONE;
}
#endif

static void BPyGPUOffScreen__tp_dealloc(BPyGPUOffScreen *self)
{
  if (self->viewport) {
    GPU_viewport_free(self->viewport);
  }
  if (self->ofs) {
    GPU_offscreen_free(self->ofs);
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef pygpu_offscreen__tp_getseters[] = {
    {"color_texture",
     (getter)pygpu_offscreen_color_texture_get,
     (setter)NULL,
     pygpu_offscreen_color_texture_doc,
     NULL},
    {"texture_color",
     (getter)pygpu_offscreen_texture_color_get,
     (setter)NULL,
     pygpu_offscreen_texture_color_doc,
     NULL},
    {"width", (getter)pygpu_offscreen_width_get, (setter)NULL, pygpu_offscreen_width_doc, NULL},
    {"height", (getter)pygpu_offscreen_height_get, (setter)NULL, pygpu_offscreen_height_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyMethodDef pygpu_offscreen__tp_methods[] = {
    {"bind", (PyCFunction)pygpu_offscreen_bind, METH_NOARGS, pygpu_offscreen_bind_doc},
    {"unbind",
     (PyCFunction)pygpu_offscreen_unbind,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_offscreen_unbind_doc},
    {"draw_view3d",
     (PyCFunction)pygpu_offscreen_draw_view3d,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_offscreen_draw_view3d_doc},
#ifdef BPYGPU_USE_GPUOBJ_FREE_METHOD
    {"free", (PyCFunction)pygpu_offscreen_free, METH_NOARGS, pygpu_offscreen_free_doc},
#endif
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(pygpu_offscreen__tp_doc,
             ".. class:: GPUOffScreen(width, height, *, format='RGBA8')\n"
             "\n"
             "   This object gives access to off screen buffers.\n"
             "\n"
             "   :arg width: Horizontal dimension of the buffer.\n"
             "   :type width: int\n"
             "   :arg height: Vertical dimension of the buffer.\n"
             "   :type height: int\n"
             "   :arg format: Internal data format inside GPU memory for color attachment "
             "texture. Possible values are:\n"
             "      `RGBA8`,\n"
             "      `RGBA16`,\n"
             "      `RGBA16F`,\n"
             "      `RGBA32F`,\n"
             "   :type format: str\n");
PyTypeObject BPyGPUOffScreen_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUOffScreen",
    .tp_basicsize = sizeof(BPyGPUOffScreen),
    .tp_dealloc = (destructor)BPyGPUOffScreen__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = pygpu_offscreen__tp_doc,
    .tp_methods = pygpu_offscreen__tp_methods,
    .tp_getset = pygpu_offscreen__tp_getseters,
    .tp_new = pygpu_offscreen__tp_new,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

PyObject *BPyGPUOffScreen_CreatePyObject(GPUOffScreen *ofs)
{
  BPyGPUOffScreen *self;

  self = PyObject_New(BPyGPUOffScreen, &BPyGPUOffScreen_Type);
  self->ofs = ofs;
  self->viewport = NULL;

  return (PyObject *)self;
}

/** \} */

#undef BPY_GPU_OFFSCREEN_CHECK_OBJ
