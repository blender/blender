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

#define BPy_BMLoopUV_Check(v)  (Py_TYPE(v) == &BPy_BMLoopUV_Type)

typedef struct BPy_BMLoopUV {
	PyObject_VAR_HEAD
	MLoopUV *data;
} BPy_BMLoopUV;

PyDoc_STRVAR(bpy_bmloopuv_uv_doc,
"Loops UV (as a 2D Vector).\n\n:type: :class:`mathutils.Vector`"
);
static PyObject *bpy_bmloopuv_uv_get(BPy_BMLoopUV *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject(self->data->uv, 2, Py_WRAP, NULL);
}

static int bpy_bmloopuv_uv_set(BPy_BMLoopUV *self, PyObject *value, void *UNUSED(closure))
{
	float tvec[2];
	if (mathutils_array_parse(tvec, 2, 2, value, "BMLoopUV.uv") != -1) {
		copy_v2_v2(self->data->uv, tvec);
		return 0;
	}
	else {
		return -1;
	}
}

PyDoc_STRVAR(bpy_bmloopuv_flag__pin_uv_doc,
"UV pin state.\n\n:type: boolean"
);
PyDoc_STRVAR(bpy_bmloopuv_flag__select_doc,
"UV select state.\n\n:type: boolean"
);
PyDoc_STRVAR(bpy_bmloopuv_flag__select_edge_doc,
"UV edge select state.\n\n:type: boolean"
);


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
    {(char *)"uv",          (getter)bpy_bmloopuv_uv_get,   (setter)bpy_bmloopuv_uv_set,   (char *)bpy_bmloopuv_uv_doc, NULL},
    {(char *)"pin_uv",      (getter)bpy_bmloopuv_flag_get, (setter)bpy_bmloopuv_flag_set, (char *)bpy_bmloopuv_flag__pin_uv_doc, (void *)MLOOPUV_PINNED},
    {(char *)"select",      (getter)bpy_bmloopuv_flag_get, (setter)bpy_bmloopuv_flag_set, (char *)bpy_bmloopuv_flag__select_doc, (void *)MLOOPUV_VERTSEL},
    {(char *)"select_edge", (getter)bpy_bmloopuv_flag_get, (setter)bpy_bmloopuv_flag_set, (char *)bpy_bmloopuv_flag__select_edge_doc, (void *)MLOOPUV_EDGESEL},

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

int BPy_BMLoopUV_AssignPyObject(struct MLoopUV *mloopuv, PyObject *value)
{
	if (UNLIKELY(!BPy_BMLoopUV_Check(value))) {
		PyErr_Format(PyExc_TypeError, "expected BMLoopUV, not a %.200s", Py_TYPE(value)->tp_name);
		return -1;
	}
	else {
		*((MLoopUV *)mloopuv) = *((MLoopUV *)((BPy_BMLoopUV *)value)->data);
		return 0;
	}
}

PyObject *BPy_BMLoopUV_CreatePyObject(struct MLoopUV *mloopuv)
{
	BPy_BMLoopUV *self = PyObject_New(BPy_BMLoopUV, &BPy_BMLoopUV_Type);
	self->data = mloopuv;
	return (PyObject *)self;
}

/* --- End Mesh Loop UV --- */

/* Mesh Loop Color
 * *************** */

/* This simply provices a color wrapper for
 * color which uses mathutils callbacks for mathutils.Color
 */

#define MLOOPCOL_FROM_CAPSULE(color_capsule)  \
	((MLoopCol *)PyCapsule_GetPointer(color_capsule, NULL))

static void mloopcol_to_float(const MLoopCol *mloopcol, float col_r[3])
{
	rgb_uchar_to_float(col_r, (unsigned char *)&mloopcol->r);
}

static void mloopcol_from_float(MLoopCol *mloopcol, const float col[3])
{
	rgb_float_to_uchar((unsigned char *)&mloopcol->r, col);
}

static unsigned char mathutils_bmloopcol_cb_index = -1;

static int mathutils_bmloopcol_check(BaseMathObject *UNUSED(bmo))
{
	/* always ok */
	return 0;
}

static int mathutils_bmloopcol_get(BaseMathObject *bmo, int UNUSED(subtype))
{
	MLoopCol *mloopcol = MLOOPCOL_FROM_CAPSULE(bmo->cb_user);
	mloopcol_to_float(mloopcol, bmo->data);
	return 0;
}

static int mathutils_bmloopcol_set(BaseMathObject *bmo, int UNUSED(subtype))
{
	MLoopCol *mloopcol = MLOOPCOL_FROM_CAPSULE(bmo->cb_user);
	mloopcol_from_float(mloopcol, bmo->data);
	return 0;
}

static int mathutils_bmloopcol_get_index(BaseMathObject *bmo, int subtype, int UNUSED(index))
{
	/* lazy, avoid repeteing the case statement */
	if (mathutils_bmloopcol_get(bmo, subtype) == -1)
		return -1;
	return 0;
}

static int mathutils_bmloopcol_set_index(BaseMathObject *bmo, int subtype, int index)
{
	const float f = bmo->data[index];

	/* lazy, avoid repeteing the case statement */
	if (mathutils_bmloopcol_get(bmo, subtype) == -1)
		return -1;

	bmo->data[index] = f;
	return mathutils_bmloopcol_set(bmo, subtype);
}

Mathutils_Callback mathutils_bmloopcol_cb = {
	mathutils_bmloopcol_check,
	mathutils_bmloopcol_get,
	mathutils_bmloopcol_set,
	mathutils_bmloopcol_get_index,
	mathutils_bmloopcol_set_index
};

static void bm_init_types_bmloopcol(void)
{
	/* pass */
	mathutils_bmloopcol_cb_index = Mathutils_RegisterCallback(&mathutils_bmloopcol_cb);
}

int BPy_BMLoopColor_AssignPyObject(struct MLoopCol *mloopcol, PyObject *value)
{
	float tvec[3];
	if (mathutils_array_parse(tvec, 3, 3, value, "BMLoopCol") != -1) {
		mloopcol_from_float(mloopcol, tvec);
		return 0;
	}
	else {
		return -1;
	}
}

PyObject *BPy_BMLoopColor_CreatePyObject(struct MLoopCol *data)
{
	PyObject *color_capsule;
	color_capsule = PyCapsule_New(data, NULL, NULL);
	return Color_CreatePyObject_cb(color_capsule, mathutils_bmloopcol_cb_index, 0);
}

#undef MLOOPCOL_FROM_CAPSULE

/* --- End Mesh Loop Color --- */


/* call to init all types */
void BPy_BM_init_types_meshdata(void)
{
	bm_init_types_bmloopuv();
	bm_init_types_bmloopcol();
}
