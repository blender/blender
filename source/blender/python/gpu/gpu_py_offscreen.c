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
 *
 * Copyright 2015, Blender Foundation.
 */

/** \file
 * \ingroup bpygpu
 *
 * This file defines the offscreen functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_library.h"
#include "BKE_scene.h"

#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "../editors/include/ED_view3d.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu_py_api.h"
#include "gpu_py_offscreen.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name GPUOffScreen Common Utilities
 * \{ */

static int bpygpu_offscreen_valid_check(BPyGPUOffScreen *bpygpu_ofs)
{
  if (UNLIKELY(bpygpu_ofs->ofs == NULL)) {
    PyErr_SetString(PyExc_ReferenceError, "GPU offscreen was freed, no further access is valid");
    return -1;
  }
  return 0;
}

#define BPY_GPU_OFFSCREEN_CHECK_OBJ(bpygpu) \
  { \
    if (UNLIKELY(bpygpu_offscreen_valid_check(bpygpu) == -1)) { \
      return NULL; \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUOffscreen Type
 * \{ */

static PyObject *bpygpu_offscreen_new(PyTypeObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  GPUOffScreen *ofs;
  int width, height, samples = 0;
  char err_out[256];

  static const char *_keywords[] = {"width", "height", "samples", NULL};
  static _PyArg_Parser _parser = {"ii|i:GPUOffScreen.__new__", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &width, &height, &samples)) {
    return NULL;
  }

  ofs = GPU_offscreen_create(width, height, samples, true, false, err_out);

  if (ofs == NULL) {
    PyErr_Format(PyExc_RuntimeError,
                 "gpu.offscreen.new(...) failed with '%s'",
                 err_out[0] ? err_out : "unknown error");
    return NULL;
  }

  return BPyGPUOffScreen_CreatePyObject(ofs);
}

PyDoc_STRVAR(bpygpu_offscreen_width_doc, "Width of the texture.\n\n:type: `int`");
static PyObject *bpygpu_offscreen_width_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  return PyLong_FromLong(GPU_offscreen_width(self->ofs));
}

PyDoc_STRVAR(bpygpu_offscreen_height_doc, "Height of the texture.\n\n:type: `int`");
static PyObject *bpygpu_offscreen_height_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  return PyLong_FromLong(GPU_offscreen_height(self->ofs));
}

PyDoc_STRVAR(bpygpu_offscreen_color_texture_doc,
             "OpenGL bindcode for the color texture.\n\n:type: `int`");
static PyObject *bpygpu_offscreen_color_texture_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  GPUTexture *texture = GPU_offscreen_color_texture(self->ofs);
  return PyLong_FromLong(GPU_texture_opengl_bindcode(texture));
}

PyDoc_STRVAR(
    bpygpu_offscreen_bind_doc,
    ".. method:: bind(save=True)\n"
    "\n"
    "   Bind the offscreen object.\n"
    "   To make sure that the offscreen gets unbind whether an exception occurs or not, pack it "
    "into a `with` statement.\n"
    "\n"
    "   :arg save: Save the current OpenGL state, so that it can be restored when unbinding.\n"
    "   :type save: `bool`\n");
static PyObject *bpygpu_offscreen_bind(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
  bool save = true;

  static const char *_keywords[] = {"save", NULL};
  static _PyArg_Parser _parser = {"|O&:bind", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, PyC_ParseBool, &save)) {
    return NULL;
  }

  GPU_offscreen_bind(self->ofs, save);

  self->is_saved = save;
  Py_INCREF(self);

  return (PyObject *)self;
}

PyDoc_STRVAR(bpygpu_offscreen_unbind_doc,
             ".. method:: unbind(restore=True)\n"
             "\n"
             "   Unbind the offscreen object.\n"
             "\n"
             "   :arg restore: Restore the OpenGL state, can only be used when the state has been "
             "saved before.\n"
             "   :type restore: `bool`\n");
static PyObject *bpygpu_offscreen_unbind(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
  bool restore = true;

  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

  static const char *_keywords[] = {"restore", NULL};
  static _PyArg_Parser _parser = {"|O&:unbind", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, PyC_ParseBool, &restore)) {
    return NULL;
  }

  GPU_offscreen_unbind(self->ofs, restore);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    bpygpu_offscreen_draw_view3d_doc,
    ".. method:: draw_view3d(scene, view3d, region, view_matrix, projection_matrix)\n"
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
    "   :type projection_matrix: :class:`mathutils.Matrix`\n");
static PyObject *bpygpu_offscreen_draw_view3d(BPyGPUOffScreen *self,
                                              PyObject *args,
                                              PyObject *kwds)
{
  MatrixObject *py_mat_view, *py_mat_projection;
  PyObject *py_scene, *py_view_layer, *py_region, *py_view3d;

  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct ViewLayer *view_layer;
  View3D *v3d;
  ARegion *ar;
  struct RV3DMatrixStore *rv3d_mats;

  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

  static const char *_keywords[] = {
      "scene", "view_layer", "view3d", "region", "view_matrix", "projection_matrix", NULL};

  static _PyArg_Parser _parser = {"OOOOO&O&:draw_view3d", _keywords, 0};
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
                                        &py_mat_projection) ||
      (!(scene = PyC_RNA_AsPointer(py_scene, "Scene")) ||
       !(view_layer = PyC_RNA_AsPointer(py_view_layer, "ViewLayer")) ||
       !(v3d = PyC_RNA_AsPointer(py_view3d, "SpaceView3D")) ||
       !(ar = PyC_RNA_AsPointer(py_region, "Region")))) {
    return NULL;
  }

  BLI_assert(BKE_id_is_in_global_main(&scene->id));

  depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);

  rv3d_mats = ED_view3d_mats_rv3d_backup(ar->regiondata);

  GPU_offscreen_bind(self->ofs, true);

  ED_view3d_draw_offscreen(depsgraph,
                           scene,
                           v3d->shading.type,
                           v3d,
                           ar,
                           GPU_offscreen_width(self->ofs),
                           GPU_offscreen_height(self->ofs),
                           (float(*)[4])py_mat_view->matrix,
                           (float(*)[4])py_mat_projection->matrix,
                           false,
                           true,
                           "",
                           true,
                           self->ofs,
                           NULL);

  GPU_offscreen_unbind(self->ofs, true);

  ED_view3d_mats_rv3d_restore(ar->regiondata, rv3d_mats);
  MEM_freeN(rv3d_mats);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_offscreen_free_doc,
             ".. method:: free()\n"
             "\n"
             "   Free the offscreen object.\n"
             "   The framebuffer, texture and render objects will no longer be accessible.\n");
static PyObject *bpygpu_offscreen_free(BPyGPUOffScreen *self)
{
  BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

  GPU_offscreen_free(self->ofs);
  self->ofs = NULL;
  Py_RETURN_NONE;
}

static PyObject *bpygpu_offscreen_bind_context_enter(BPyGPUOffScreen *UNUSED(self))
{
  Py_RETURN_NONE;
}

static PyObject *bpygpu_offscreen_bind_context_exit(BPyGPUOffScreen *self, PyObject *UNUSED(args))
{
  GPU_offscreen_unbind(self->ofs, self->is_saved);
  Py_RETURN_NONE;
}

static void BPyGPUOffScreen__tp_dealloc(BPyGPUOffScreen *self)
{
  if (self->ofs) {
    GPU_offscreen_free(self->ofs);
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef bpygpu_offscreen_getseters[] = {
    {(char *)"color_texture",
     (getter)bpygpu_offscreen_color_texture_get,
     (setter)NULL,
     bpygpu_offscreen_color_texture_doc,
     NULL},
    {(char *)"width",
     (getter)bpygpu_offscreen_width_get,
     (setter)NULL,
     bpygpu_offscreen_width_doc,
     NULL},
    {(char *)"height",
     (getter)bpygpu_offscreen_height_get,
     (setter)NULL,
     bpygpu_offscreen_height_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef bpygpu_offscreen_methods[] = {
    {"bind",
     (PyCFunction)bpygpu_offscreen_bind,
     METH_VARARGS | METH_KEYWORDS,
     bpygpu_offscreen_bind_doc},
    {"unbind",
     (PyCFunction)bpygpu_offscreen_unbind,
     METH_VARARGS | METH_KEYWORDS,
     bpygpu_offscreen_unbind_doc},
    {"draw_view3d",
     (PyCFunction)bpygpu_offscreen_draw_view3d,
     METH_VARARGS | METH_KEYWORDS,
     bpygpu_offscreen_draw_view3d_doc},
    {"free", (PyCFunction)bpygpu_offscreen_free, METH_NOARGS, bpygpu_offscreen_free_doc},
    {"__enter__", (PyCFunction)bpygpu_offscreen_bind_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpygpu_offscreen_bind_context_exit, METH_VARARGS},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(bpygpu_offscreen_doc,
             ".. class:: GPUOffScreen(width, height, samples=0)\n"
             "\n"
             "   This object gives access to off screen buffers.\n"
             "\n"
             "   :arg width: Horizontal dimension of the buffer.\n"
             "   :type width: `int`\n"
             "   :arg height: Vertical dimension of the buffer.\n"
             "   :type height: `int`\n"
             "   :arg samples: OpenGL samples to use for MSAA or zero to disable.\n"
             "   :type samples: `int`\n");
PyTypeObject BPyGPUOffScreen_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "GPUOffScreen",
    .tp_basicsize = sizeof(BPyGPUOffScreen),
    .tp_dealloc = (destructor)BPyGPUOffScreen__tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = bpygpu_offscreen_doc,
    .tp_methods = bpygpu_offscreen_methods,
    .tp_getset = bpygpu_offscreen_getseters,
    .tp_new = bpygpu_offscreen_new,
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

  return (PyObject *)self;
}

/** \} */

#undef BPY_GPU_OFFSCREEN_CHECK_OBJ
