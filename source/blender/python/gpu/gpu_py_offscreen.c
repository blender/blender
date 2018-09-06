/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/gpu/gpu_py_offscreen.c
 *  \ingroup bpygpu
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

#include "BKE_global.h"
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

#include "gpu_py_offscreen.h" /* own include */

/* -------------------------------------------------------------------- */

/** \name GPUOffscreen Type
 * \{ */

static PyObject *bpygpu_offscreen_new(PyTypeObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"width", "height", "samples", NULL};

	GPUOffScreen *ofs;
	int width, height, samples = 0;
	char err_out[256];

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "ii|i:new", (char **)(kwlist),
	        &width, &height, &samples))
	{
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

static int bpygpu_offscreen_valid_check(BPyGPUOffScreen *bpygpu_ofs)
{
	if (UNLIKELY(bpygpu_ofs->ofs == NULL)) {
		PyErr_SetString(PyExc_ReferenceError, "GPU offscreen was freed, no further access is valid");
		return -1;
	}
	return 0;
}

#define BPY_GPU_OFFSCREEN_CHECK_OBJ(bpygpu) { \
	if (UNLIKELY(bpygpu_offscreen_valid_check(bpygpu) == -1)) { \
		return NULL; \
	} \
} ((void)0)

PyDoc_STRVAR(bpygpu_offscreen_width_doc, "Texture width.\n\n:type: int");
static PyObject *bpygpu_offscreen_width_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
	return PyLong_FromLong(GPU_offscreen_width(self->ofs));
}

PyDoc_STRVAR(bpygpu_offscreen_height_doc, "Texture height.\n\n:type: int");
static PyObject *bpygpu_offscreen_height_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
	return PyLong_FromLong(GPU_offscreen_height(self->ofs));
}

PyDoc_STRVAR(bpygpu_offscreen_color_texture_doc, "Color texture.\n\n:type: int");
static PyObject *bpygpu_offscreen_color_texture_get(BPyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
	GPUTexture *texture = GPU_offscreen_color_texture(self->ofs);
	return PyLong_FromLong(GPU_texture_opengl_bindcode(texture));
}

PyDoc_STRVAR(bpygpu_offscreen_bind_doc,
"bind(save=True)\n"
"\n"
"   Bind the offscreen object.\n"
"\n"
"   :param save: save OpenGL current states.\n"
"   :type save: bool\n"
);
static PyObject *bpygpu_offscreen_bind(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"save", NULL};
	bool save = true;

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "|O&:bind", (char **)(kwlist),
	        PyC_ParseBool, &save))
	{
		return NULL;
	}

	GPU_offscreen_bind(self->ofs, save);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_offscreen_unbind_doc,
"unbind(restore=True)\n"
"\n"
"   Unbind the offscreen object.\n"
"\n"
"   :param restore: restore OpenGL previous states.\n"
"   :type restore: bool\n"
);
static PyObject *bpygpu_offscreen_unbind(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"restore", NULL};
	bool restore = true;

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "|O&:unbind", (char **)(kwlist),
	        PyC_ParseBool, &restore))
	{
		return NULL;
	}

	GPU_offscreen_unbind(self->ofs, restore);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_offscreen_draw_view3d_doc,
"draw_view3d(scene, view3d, region, modelview_matrix, projection_matrix)\n"
"\n"
"   Draw the 3d viewport in the offscreen object.\n"
"\n"
"   :param scene: Scene to draw.\n"
"   :type scene: :class:`bpy.types.Scene`\n"
"   :param view3d: 3D View to get the drawing settings from.\n"
"   :type view3d: :class:`bpy.types.SpaceView3D`\n"
"   :param region: Region of the 3D View.\n"
"   :type region: :class:`bpy.types.Region`\n"
"   :param modelview_matrix: ModelView Matrix.\n"
"   :type modelview_matrix: :class:`mathutils.Matrix`\n"
"   :param projection_matrix: Projection Matrix.\n"
"   :type projection_matrix: :class:`mathutils.Matrix`\n"
);
static PyObject *bpygpu_offscreen_draw_view3d(BPyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"scene", "view_layer", "view3d", "region", "projection_matrix", "modelview_matrix", NULL};

	MatrixObject *py_mat_modelview, *py_mat_projection;
	PyObject *py_scene, *py_view_layer, *py_region, *py_view3d;

	struct Depsgraph *depsgraph;
	struct Scene *scene;
	struct ViewLayer *view_layer;
	View3D *v3d;
	ARegion *ar;
	struct RV3DMatrixStore *rv3d_mats;

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "OOOOO&O&:draw_view3d", (char **)(kwlist),
	        &py_scene, &py_view_layer, &py_view3d, &py_region,
	        Matrix_Parse4x4, &py_mat_projection,
	        Matrix_Parse4x4, &py_mat_modelview) ||
	    (!(scene       = PyC_RNA_AsPointer(py_scene, "Scene")) ||
	     !(view_layer = PyC_RNA_AsPointer(py_view_layer, "ViewLayer")) ||
	     !(v3d         = PyC_RNA_AsPointer(py_view3d, "SpaceView3D")) ||
	     !(ar          = PyC_RNA_AsPointer(py_region, "Region"))))
	{
		return NULL;
	}

	BLI_assert(BKE_id_is_in_gobal_main(&scene->id));

	depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);

	rv3d_mats = ED_view3d_mats_rv3d_backup(ar->regiondata);

	GPU_offscreen_bind(self->ofs, true); /* bind */

	ED_view3d_draw_offscreen(depsgraph,
	                         scene,
	                         v3d->shading.type,
	                         v3d,
	                         ar,
	                         GPU_offscreen_width(self->ofs),
	                         GPU_offscreen_height(self->ofs),
	                         (float(*)[4])py_mat_modelview->matrix,
	                         (float(*)[4])py_mat_projection->matrix,
	                         false,
	                         true,
	                         "",
	                         NULL,
	                         self->ofs,
	                         NULL);

	GPU_offscreen_unbind(self->ofs, true); /* unbind */

	ED_view3d_mats_rv3d_restore(ar->regiondata, rv3d_mats);
	MEM_freeN(rv3d_mats);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpygpu_offscreen_free_doc,
"free()\n"
"\n"
"   Free the offscreen object\n"
"   The framebuffer, texture and render objects will no longer be accessible.\n"
);
static PyObject *bpygpu_offscreen_free(BPyGPUOffScreen *self)
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

	GPU_offscreen_free(self->ofs);
	self->ofs = NULL;
	Py_RETURN_NONE;
}

static void BPyGPUOffScreen__tp_dealloc(BPyGPUOffScreen *self)
{
	if (self->ofs)
		GPU_offscreen_free(self->ofs);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef bpygpu_offscreen_getseters[] = {
	{(char *)"color_texture", (getter)bpygpu_offscreen_color_texture_get, (setter)NULL, bpygpu_offscreen_color_texture_doc, NULL},
	{(char *)"width", (getter)bpygpu_offscreen_width_get, (setter)NULL, bpygpu_offscreen_width_doc, NULL},
	{(char *)"height", (getter)bpygpu_offscreen_height_get, (setter)NULL, bpygpu_offscreen_height_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

static struct PyMethodDef bpygpu_offscreen_methods[] = {
	{"bind", (PyCFunction)bpygpu_offscreen_bind, METH_VARARGS | METH_KEYWORDS, bpygpu_offscreen_bind_doc},
	{"unbind", (PyCFunction)bpygpu_offscreen_unbind, METH_VARARGS | METH_KEYWORDS, bpygpu_offscreen_unbind_doc},
	{"draw_view3d", (PyCFunction)bpygpu_offscreen_draw_view3d, METH_VARARGS | METH_KEYWORDS, bpygpu_offscreen_draw_view3d_doc},
	{"free", (PyCFunction)bpygpu_offscreen_free, METH_NOARGS, bpygpu_offscreen_free_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(bpygpu_offscreen_doc,
"GPUOffScreen(width, height, samples=0)\n"
"\n"
"   This object gives access to off screen buffers.\n"
"\n"
"   :param width: Horizontal dimension of the buffer.\n"
"   :type width: `int`\n"
"   :param height: Vertical dimension of the buffer.\n"
"   :type height: `int`\n"
"   :param samples: OpenGL samples to use for MSAA or zero to disable.\n"
"   :type samples: `int`\n"
);
PyTypeObject BPyGPUOffScreen_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "GPUOffScreen",
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
