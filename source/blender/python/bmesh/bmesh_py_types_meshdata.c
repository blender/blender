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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_types_meshdata.c
 *  \ingroup pybmesh
 *
 * This file defines customdata types which can't be accessed as primitive
 * python types such as MDeformVert, MLoopUV, MTexPoly
 */

#include <Python.h>

#include "../mathutils/mathutils.h"

#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

/* Mesh Loop UV
 * ************ */

typedef struct BPy_BMLoopUV {
	PyObject_VAR_HEAD
	MLoopUV *data;
} BPy_BMLoopUV;

static PyObject *bpy_bmloopuv_uv_get(BPy_BMLoopUV *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject(self->data->uv, 2, Py_WRAP, NULL);
}

static int bpy_bmloopuv_uv_set(BPy_BMLoopUV *self, PyObject *value, void *UNUSED(closure))
{
	float tvec[2];
	if (mathutils_array_parse(tvec, 2, 2, value, "BMLoop.uv") != -1) {
		copy_v2_v2(self->data->uv, tvec);
		return 0;
	}
	else {
		return -1;
	}
}

static PyObject *bpy_bmloopuv_flag_get(BPy_BMLoopUV *self, void *flag_p)
{
	const int flag = GET_INT_FROM_POINTER(flag_p);
	return PyBool_FromLong(self->data->flag & flag);
}

static int bpy_bmloopuv_flag_set(BPy_BMLoopUV *self, PyObject *value, void *flag_p)
{
	const int flag = GET_INT_FROM_POINTER(flag_p);

	switch (PyLong_AsLong(value)) {
		case TRUE:
			self->data->flag |= flag;
			return 0;
		case FALSE:
			self->data->flag &= ~flag;
			return 0;
		default:
			PyErr_SetString(PyExc_TypeError,
			                "expected a boolean type 0/1");
			return -1;
	}
}

static PyGetSetDef bpy_bmloopuv_getseters[] = {
    /* attributes match rna_def_mloopuv  */
    {(char *)"uv",          (getter)bpy_bmloopuv_uv_get,   (setter)bpy_bmloopuv_uv_set,   (char *)NULL, NULL},
    {(char *)"pin_uv",      (getter)bpy_bmloopuv_flag_get, (setter)bpy_bmloopuv_flag_set, (char *)NULL, (void *)MLOOPUV_PINNED},
    {(char *)"select",      (getter)bpy_bmloopuv_flag_get, (setter)bpy_bmloopuv_flag_set, (char *)NULL, (void *)MLOOPUV_VERTSEL},
    {(char *)"select_edge", (getter)bpy_bmloopuv_flag_get, (setter)bpy_bmloopuv_flag_set, (char *)NULL, (void *)MLOOPUV_EDGESEL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

PyTypeObject BPy_BMLoopUV_Type = {{{0}}}; /* bm.loops.layers.uv.active */

static void bm_init_types_bmloopuv(void)
{
	BPy_BMLoopUV_Type.tp_basicsize = sizeof(BPy_BMLoopUV);

	BPy_BMLoopUV_Type.tp_name = "BMLoopUV";

	BPy_BMLoopUV_Type.tp_doc = NULL; // todo

	BPy_BMLoopUV_Type.tp_getset = bpy_bmloopuv_getseters;

	BPy_BMLoopUV_Type.tp_flags = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_BMLoopUV_Type);
}

PyObject *BPy_BMLoopUV_CreatePyObject(struct MLoopUV *data)
{
	BPy_BMLoopUV *self = PyObject_New(BPy_BMLoopUV, &BPy_BMLoopUV_Type);
	self->data = data;
	return (PyObject *)self;
}

/* --- End Mesh Loop UV --- */


/* call to init all types */
void BPy_BM_init_types_meshdata(void)
{
	bm_init_types_bmloopuv();
}
