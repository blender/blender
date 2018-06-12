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

/** \file blender/python/intern/gpu_offscreen.c
 *  \ingroup pythonintern
 *
 * This file defines the offscreen functionalities of the 'gpu' module
 * used for off-screen OpenGL rendering.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "WM_types.h"

#include "BKE_global.h"

#include "ED_screen.h"

#include "GPU_compositing.h"
#include "GPU_framebuffer.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "gpu.h"

#include "ED_view3d.h"

/* -------------------------------------------------------------------- */
/* GPU Offscreen PyObject */

typedef struct {
	PyObject_HEAD
	GPUOffScreen *ofs;
} BPy_GPUOffScreen;

static int bpy_gpu_offscreen_valid_check(BPy_GPUOffScreen *py_gpu_ofs)
{
	if (UNLIKELY(py_gpu_ofs->ofs == NULL)) {
		PyErr_SetString(PyExc_ReferenceError, "GPU offscreen was freed, no further access is valid");
		return -1;
	}
	return 0;
}

#define BPY_GPU_OFFSCREEN_CHECK_OBJ(pygpu) { \
	if (UNLIKELY(bpy_gpu_offscreen_valid_check(pygpu) == -1)) { \
		return NULL; \
	} \
} ((void)0)

PyDoc_STRVAR(pygpu_offscreen_width_doc, "Texture width.\n\n:type: int");
static PyObject *pygpu_offscreen_width_get(BPy_GPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
	return PyLong_FromLong(GPU_offscreen_width(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_height_doc, "Texture height.\n\n:type: int");
static PyObject *pygpu_offscreen_height_get(BPy_GPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
	return PyLong_FromLong(GPU_offscreen_height(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_color_texture_doc, "Color texture.\n\n:type: int");
static PyObject *pygpu_offscreen_color_texture_get(BPy_GPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);
	return PyLong_FromLong(GPU_offscreen_color_texture(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_bind_doc,
"bind(save=True)\n"
"\n"
"   Bind the offscreen object.\n"
"\n"
"   :param save: save OpenGL current states.\n"
"   :type save: bool\n"
);
static PyObject *pygpu_offscreen_bind(BPy_GPUOffScreen *self, PyObject *args, PyObject *kwds)
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

PyDoc_STRVAR(pygpu_offscreen_unbind_doc,
"unbind(restore=True)\n"
"\n"
"   Unbind the offscreen object.\n"
"\n"
"   :param restore: restore OpenGL previous states.\n"
"   :type restore: bool\n"
);
static PyObject *pygpu_offscreen_unbind(BPy_GPUOffScreen *self, PyObject *args, PyObject *kwds)
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

PyDoc_STRVAR(pygpu_offscreen_draw_view3d_doc,
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
static PyObject *pygpu_offscreen_draw_view3d(BPy_GPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"scene", "view3d", "region", "projection_matrix", "modelview_matrix", NULL};

	MatrixObject *py_mat_modelview, *py_mat_projection;
	PyObject *py_scene, *py_region, *py_view3d;

	struct Main *bmain = G.main;  /* XXX UGLY! */
	Scene *scene;
	View3D *v3d;
	ARegion *ar;
	GPUFX *fx;
	GPUFXSettings fx_settings;
	struct RV3DMatrixStore *rv3d_mats;

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "OOOO&O&:draw_view3d", (char **)(kwlist),
	        &py_scene, &py_view3d, &py_region,
	        Matrix_Parse4x4, &py_mat_projection,
	        Matrix_Parse4x4, &py_mat_modelview) ||
	    (!(scene    = PyC_RNA_AsPointer(py_scene, "Scene")) ||
	     !(v3d      = PyC_RNA_AsPointer(py_view3d, "SpaceView3D")) ||
	     !(ar       = PyC_RNA_AsPointer(py_region, "Region"))))
	{
		return NULL;
	}

	fx = GPU_fx_compositor_create();

	fx_settings = v3d->fx_settings;  /* full copy */

	ED_view3d_draw_offscreen_init(bmain, scene, v3d);

	rv3d_mats = ED_view3d_mats_rv3d_backup(ar->regiondata);

	GPU_offscreen_bind(self->ofs, true); /* bind */

	ED_view3d_draw_offscreen(
	        bmain, scene, v3d, ar, GPU_offscreen_width(self->ofs), GPU_offscreen_height(self->ofs),
	        (float(*)[4])py_mat_modelview->matrix, (float(*)[4])py_mat_projection->matrix,
	        false, true, true, "",
	        fx, &fx_settings,
	        self->ofs);

	GPU_fx_compositor_destroy(fx);
	GPU_offscreen_unbind(self->ofs, true); /* unbind */

	ED_view3d_mats_rv3d_restore(ar->regiondata, rv3d_mats);
	MEM_freeN(rv3d_mats);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_offscreen_free_doc,
"free()\n"
"\n"
"   Free the offscreen object\n"
"   The framebuffer, texture and render objects will no longer be accessible.\n"
);
static PyObject *pygpu_offscreen_free(BPy_GPUOffScreen *self)
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self);

	GPU_offscreen_free(self->ofs);
	self->ofs = NULL;
	Py_RETURN_NONE;
}

static void BPy_GPUOffScreen__tp_dealloc(BPy_GPUOffScreen *self)
{
	if (self->ofs)
		GPU_offscreen_free(self->ofs);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef bpy_gpu_offscreen_getseters[] = {
	{(char *)"color_texture", (getter)pygpu_offscreen_color_texture_get, (setter)NULL, pygpu_offscreen_color_texture_doc, NULL},
	{(char *)"width", (getter)pygpu_offscreen_width_get, (setter)NULL, pygpu_offscreen_width_doc, NULL},
	{(char *)"height", (getter)pygpu_offscreen_height_get, (setter)NULL, pygpu_offscreen_height_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

static struct PyMethodDef bpy_gpu_offscreen_methods[] = {
	{"bind", (PyCFunction)pygpu_offscreen_bind, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_bind_doc},
	{"unbind", (PyCFunction)pygpu_offscreen_unbind, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_unbind_doc},
	{"draw_view3d", (PyCFunction)pygpu_offscreen_draw_view3d, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_draw_view3d_doc},
	{"free", (PyCFunction)pygpu_offscreen_free, METH_NOARGS, pygpu_offscreen_free_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(py_gpu_offscreen_doc,
".. class:: GPUOffscreen"
"\n"
"   This object gives access to off screen buffers.\n"
);
static PyTypeObject BPy_GPUOffScreen_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GPUOffScreen",                              /* tp_name */
	sizeof(BPy_GPUOffScreen),                      /* tp_basicsize */
	0,                                           /* tp_itemsize */
	/* methods */
	(destructor)BPy_GPUOffScreen__tp_dealloc,      /* tp_dealloc */
	NULL,                                        /* tp_print */
	NULL,                                        /* tp_getattr */
	NULL,                                        /* tp_setattr */
	NULL,                                        /* tp_compare */
	NULL,                                        /* tp_repr */
	NULL,                                        /* tp_as_number */
	NULL,                                        /* tp_as_sequence */
	NULL,                                        /* tp_as_mapping */
	NULL,                                        /* tp_hash */
	NULL,                                        /* tp_call */
	NULL,                                        /* tp_str */
	NULL,                                        /* tp_getattro */
	NULL,                                        /* tp_setattro */
	NULL,                                        /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                          /* tp_flags */
	py_gpu_offscreen_doc,                        /* Documentation string */
	NULL,                                        /* tp_traverse */
	NULL,                                        /* tp_clear */
	NULL,                                        /* tp_richcompare */
	0,                                           /* tp_weaklistoffset */
	NULL,                                        /* tp_iter */
	NULL,                                        /* tp_iternext */
	bpy_gpu_offscreen_methods,                   /* tp_methods */
	NULL,                                        /* tp_members */
	bpy_gpu_offscreen_getseters,                 /* tp_getset */
	NULL,                                        /* tp_base */
	NULL,                                        /* tp_dict */
	NULL,                                        /* tp_descr_get */
	NULL,                                        /* tp_descr_set */
	0,                                           /* tp_dictoffset */
	0,                                           /* tp_init */
	NULL,                                        /* tp_alloc */
	NULL,                                        /* tp_new */
	(freefunc)0,                                 /* tp_free */
	NULL,                                        /* tp_is_gc */
	NULL,                                        /* tp_bases */
	NULL,                                        /* tp_mro */
	NULL,                                        /* tp_cache */
	NULL,                                        /* tp_subclasses */
	NULL,                                        /* tp_weaklist */
	(destructor) NULL                            /* tp_del */
};

/* -------------------------------------------------------------------- */
/* GPU offscreen methods */

static PyObject *BPy_GPU_OffScreen_CreatePyObject(GPUOffScreen *ofs)
{
	BPy_GPUOffScreen *self;
	self = PyObject_New(BPy_GPUOffScreen, &BPy_GPUOffScreen_Type);
	self->ofs = ofs;
	return (PyObject *)self;
}

PyDoc_STRVAR(pygpu_offscreen_new_doc,
"new(width, height, samples=0)\n"
"\n"
"   Return a GPUOffScreen.\n"
"\n"
"   :param width: Horizontal dimension of the buffer.\n"
"   :type width: int`\n"
"   :param height: Vertical dimension of the buffer.\n"
"   :type height: int`\n"
"   :param samples: OpenGL samples to use for MSAA or zero to disable.\n"
"   :type samples: int\n"
"   :return: Newly created off-screen buffer.\n"
"   :rtype: :class:`gpu.GPUOffscreen`\n"
);
static PyObject *pygpu_offscreen_new(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
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

	ofs = GPU_offscreen_create(width, height, samples, err_out);

	if (ofs == NULL) {
		PyErr_Format(PyExc_RuntimeError,
		             "gpu.offscreen.new(...) failed with '%s'",
		             err_out[0] ? err_out : "unknown error");
		return NULL;
	}

	return BPy_GPU_OffScreen_CreatePyObject(ofs);
}

static struct PyMethodDef BPy_GPU_offscreen_methods[] = {
	{"new", (PyCFunction)pygpu_offscreen_new, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_new_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_GPU_offscreen_doc,
"This module provides access to offscreen rendering functions."
);
static PyModuleDef BPy_GPU_offscreen_module_def = {
	PyModuleDef_HEAD_INIT,
	"gpu.offscreen",                             /* m_name */
	BPy_GPU_offscreen_doc,                       /* m_doc */
	0,                                           /* m_size */
	BPy_GPU_offscreen_methods,                   /* m_methods */
	NULL,                                        /* m_reload */
	NULL,                                        /* m_traverse */
	NULL,                                        /* m_clear */
	NULL,                                        /* m_free */
};

PyObject *BPyInit_gpu_offscreen(void)
{
	PyObject *submodule;

	/* Register the 'GPUOffscreen' class */
	if (PyType_Ready(&BPy_GPUOffScreen_Type)) {
		return NULL;
	}

	submodule = PyModule_Create(&BPy_GPU_offscreen_module_def);

#define MODULE_TYPE_ADD(s, t) \
	PyModule_AddObject(s, t.tp_name, (PyObject *)&t); Py_INCREF((PyObject *)&t)

	MODULE_TYPE_ADD(submodule, BPy_GPUOffScreen_Type);

#undef MODULE_TYPE_ADD

	return submodule;
}

#undef BPY_GPU_OFFSCREEN_CHECK_OBJ
