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

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "BKE_deform.h"
#include "BKE_library.h"

#include "bmesh_py_types_meshdata.h"


/* Mesh BMTexPoly
 * ************** */

#define BPy_BMTexPoly_Check(v)  (Py_TYPE(v) == &BPy_BMTexPoly_Type)

typedef struct BPy_BMTexPoly {
	PyObject_VAR_HEAD
	MTexPoly *data;
} BPy_BMTexPoly;

extern PyObject *pyrna_id_CreatePyObject(ID *id);
extern bool      pyrna_id_FromPyObject(PyObject *obj, ID **id);

PyDoc_STRVAR(bpy_bmtexpoly_image_doc,
"Image or None.\n\n:type: :class:`bpy.types.Image`"
);
static PyObject *bpy_bmtexpoly_image_get(BPy_BMTexPoly *self, void *UNUSED(closure))
{
	return pyrna_id_CreatePyObject((ID *)self->data->tpage);
}

static int bpy_bmtexpoly_image_set(BPy_BMTexPoly *self, PyObject *value, void *UNUSED(closure))
{
	ID *id;

	if (value == Py_None) {
		id = NULL;
	}
	else if (pyrna_id_FromPyObject(value, &id) && id && GS(id->name) == ID_IM) {
		/* pass */
	}
	else {
		PyErr_Format(PyExc_KeyError, "BMTexPoly.image = x"
		             "expected an image or None, not '%.200s'",
		             Py_TYPE(value)->tp_name);
		return -1;
	}

	id_lib_extern(id);
	self->data->tpage = (struct Image *)id;

	return 0;
}

static PyGetSetDef bpy_bmtexpoly_getseters[] = {
	/* attributes match rna_def_mtpoly  */
	{(char *)"image", (getter)bpy_bmtexpoly_image_get, (setter)bpy_bmtexpoly_image_set, (char *)bpy_bmtexpoly_image_doc, NULL},

	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyTypeObject BPy_BMTexPoly_Type = {{{0}}}; /* bm.loops.layers.uv.active */

static void bm_init_types_bmtexpoly(void)
{
	BPy_BMTexPoly_Type.tp_basicsize = sizeof(BPy_BMTexPoly);

	BPy_BMTexPoly_Type.tp_name = "BMTexPoly";

	BPy_BMTexPoly_Type.tp_doc = NULL; // todo

	BPy_BMTexPoly_Type.tp_getset = bpy_bmtexpoly_getseters;

	BPy_BMTexPoly_Type.tp_flags = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_BMTexPoly_Type);
}

int BPy_BMTexPoly_AssignPyObject(struct MTexPoly *mtpoly, PyObject *value)
{
	if (UNLIKELY(!BPy_BMTexPoly_Check(value))) {
		PyErr_Format(PyExc_TypeError, "expected BMTexPoly, not a %.200s", Py_TYPE(value)->tp_name);
		return -1;
	}
	else {
		*((MTexPoly *)mtpoly) = *(((BPy_BMTexPoly *)value)->data);
		return 0;
	}
}

PyObject *BPy_BMTexPoly_CreatePyObject(struct MTexPoly *mtpoly)
{
	BPy_BMTexPoly *self = PyObject_New(BPy_BMTexPoly, &BPy_BMTexPoly_Type);
	self->data = mtpoly;
	return (PyObject *)self;
}

/* --- End Mesh BMTexPoly --- */

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
		case true:
			self->data->flag |= flag;
			return 0;
		case false:
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
		*((MLoopUV *)mloopuv) = *(((BPy_BMLoopUV *)value)->data);
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

/* Mesh Vert Skin
 * ************ */

#define BPy_BMVertSkin_Check(v)  (Py_TYPE(v) == &BPy_BMVertSkin_Type)

typedef struct BPy_BMVertSkin {
	PyObject_VAR_HEAD
	MVertSkin *data;
} BPy_BMVertSkin;

PyDoc_STRVAR(bpy_bmvertskin_radius_doc,
"Vert skin radii (as a 2D Vector).\n\n:type: :class:`mathutils.Vector`"
);
static PyObject *bpy_bmvertskin_radius_get(BPy_BMVertSkin *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject(self->data->radius, 2, Py_WRAP, NULL);
}

static int bpy_bmvertskin_radius_set(BPy_BMVertSkin *self, PyObject *value, void *UNUSED(closure))
{
	float tvec[2];
	if (mathutils_array_parse(tvec, 2, 2, value, "BMVertSkin.radius") != -1) {
		copy_v2_v2(self->data->radius, tvec);
		return 0;
	}
	else {
		return -1;
	}
}

PyDoc_STRVAR(bpy_bmvertskin_flag__use_root_doc,
"Use as root vertex.\n\n:type: boolean"
);
PyDoc_STRVAR(bpy_bmvertskin_flag__use_loose_doc,
"Use loose vertex.\n\n:type: boolean"
);

static PyObject *bpy_bmvertskin_flag_get(BPy_BMVertSkin *self, void *flag_p)
{
	const int flag = GET_INT_FROM_POINTER(flag_p);
	return PyBool_FromLong(self->data->flag & flag);
}

static int bpy_bmvertskin_flag_set(BPy_BMVertSkin *self, PyObject *value, void *flag_p)
{
	const int flag = GET_INT_FROM_POINTER(flag_p);

	switch (PyLong_AsLong(value)) {
		case true:
			self->data->flag |= flag;
			return 0;
		case false:
			self->data->flag &= ~flag;
			return 0;
		default:
			PyErr_SetString(PyExc_TypeError,
			                "expected a boolean type 0/1");
			return -1;
	}
}

/* XXX Todo: Make root settable, currently the code to disable all other verts as roots sits within the modifier */
static PyGetSetDef bpy_bmvertskin_getseters[] = {
	/* attributes match rna_mesh_gen  */
	{(char *)"radius",    (getter)bpy_bmvertskin_radius_get, (setter)bpy_bmvertskin_radius_set, (char *)bpy_bmvertskin_radius_doc, NULL},
	{(char *)"use_root",  (getter)bpy_bmvertskin_flag_get,   (setter)NULL,					   (char *)bpy_bmvertskin_flag__use_root_doc,  (void *)MVERT_SKIN_ROOT},
	{(char *)"use_loose", (getter)bpy_bmvertskin_flag_get,   (setter)bpy_bmvertskin_flag_set,   (char *)bpy_bmvertskin_flag__use_loose_doc, (void *)MVERT_SKIN_LOOSE},

	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyTypeObject BPy_BMVertSkin_Type = {{{0}}}; /* bm.loops.layers.uv.active */

static void bm_init_types_bmvertskin(void)
{
	BPy_BMVertSkin_Type.tp_basicsize = sizeof(BPy_BMVertSkin);

	BPy_BMVertSkin_Type.tp_name = "BMVertSkin";

	BPy_BMVertSkin_Type.tp_doc = NULL; // todo

	BPy_BMVertSkin_Type.tp_getset = bpy_bmvertskin_getseters;

	BPy_BMVertSkin_Type.tp_flags = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_BMVertSkin_Type);
}

int BPy_BMVertSkin_AssignPyObject(struct MVertSkin *mvertskin, PyObject *value)
{
	if (UNLIKELY(!BPy_BMVertSkin_Check(value))) {
		PyErr_Format(PyExc_TypeError, "expected BMVertSkin, not a %.200s", Py_TYPE(value)->tp_name);
		return -1;
	}
	else {
		*((MVertSkin *)mvertskin) = *(((BPy_BMVertSkin *)value)->data);
		return 0;
	}
}

PyObject *BPy_BMVertSkin_CreatePyObject(struct MVertSkin *mvertskin)
{
	BPy_BMVertSkin *self = PyObject_New(BPy_BMVertSkin, &BPy_BMVertSkin_Type);
	self->data = mvertskin;
	return (PyObject *)self;
}

/* --- End Mesh Vert Skin --- */

/* Mesh Loop Color
 * *************** */

/* This simply provides a color wrapper for
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

static Mathutils_Callback mathutils_bmloopcol_cb = {
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


/* Mesh Deform Vert
 * **************** */

/**
 * This is python type wraps a deform vert as a python dictionary,
 * hiding the #MDeformWeight on access, since the mapping is very close, eg:
 *
 * C:
 *     weight = defvert_find_weight(dv, group_nr);
 *     defvert_remove_group(dv, dw)
 *
 * Py:
 *     weight = dv[group_nr]
 *     del dv[group_nr]
 *
 * \note: there is nothing BMesh specific here,
 * its only that BMesh is the only part of blender that uses a hand written api like this.
 * This type could eventually be used to access lattice weights.
 *
 * \note: Many of blender-api's dict-like-wrappers act like ordered dicts,
 * This is intentional _not_ ordered, the weights can be in any order and it wont matter,
 * the order should not be used in the api in any meaningful way (as with a python dict)
 * only expose as mapping, not a sequence.
 */

#define BPy_BMDeformVert_Check(v)  (Py_TYPE(v) == &BPy_BMDeformVert_Type)

typedef struct BPy_BMDeformVert {
	PyObject_VAR_HEAD
	MDeformVert *data;
} BPy_BMDeformVert;


/* Mapping Protocols
 * ================= */

static int bpy_bmdeformvert_len(BPy_BMDeformVert *self)
{
	return self->data->totweight;
}

static PyObject *bpy_bmdeformvert_subscript(BPy_BMDeformVert *self, PyObject *key)
{
	if (PyIndex_Check(key)) {
		int i;
		i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred()) {
			return NULL;
		}
		else {
			MDeformWeight *dw = defvert_find_index(self->data, i);

			if (dw == NULL) {
				PyErr_SetString(PyExc_KeyError, "BMDeformVert[key] = x: "
				                "key not found");
				return NULL;
			}
			else {
				return PyFloat_FromDouble(dw->weight);
			}
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "BMDeformVert keys must be integers, not %.200s",
		             Py_TYPE(key)->tp_name);
		return NULL;
	}
}

static int bpy_bmdeformvert_ass_subscript(BPy_BMDeformVert *self, PyObject *key, PyObject *value)
{
	if (PyIndex_Check(key)) {
		int i;

		i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred()) {
			return -1;
		}

		if (value) {
			/* dvert[group_index] = 0.5 */
			if (i < 0) {
				PyErr_SetString(PyExc_KeyError, "BMDeformVert[key] = x: "
				                "weight keys can't be negative");
				return -1;
			}
			else {
				MDeformWeight *dw = defvert_verify_index(self->data, i);
				const float f = PyFloat_AsDouble(value);
				if (f == -1 && PyErr_Occurred()) { // parsed key not a number
					PyErr_SetString(PyExc_TypeError,
					                "BMDeformVert[key] = x: "
					                "assigned value not a number");
					return -1;
				}

				dw->weight = CLAMPIS(f, 0.0f, 1.0f);
			}
		}
		else {
			/* del dvert[group_index] */
			MDeformWeight *dw = defvert_find_index(self->data, i);

			if (dw == NULL) {
				PyErr_SetString(PyExc_KeyError, "del BMDeformVert[key]: "
				                "key not found");
			}
			defvert_remove_group(self->data, dw);
		}

		return 0;

	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "BMDeformVert keys must be integers, not %.200s",
		             Py_TYPE(key)->tp_name);
		return -1;
	}
}

static int bpy_bmdeformvert_contains(BPy_BMDeformVert *self, PyObject *value)
{
	const int key = PyLong_AsSsize_t(value);

	if (key == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError,
		                "BMDeformVert.__contains__: expected an int");
		return -1;
	}

	return (defvert_find_index(self->data, key) != NULL) ? 1 : 0;
}

/* only defined for __contains__ */
static PySequenceMethods bpy_bmdeformvert_as_sequence = {
	(lenfunc)bpy_bmdeformvert_len,               /* sq_length */
	NULL,                                        /* sq_concat */
	NULL,                                        /* sq_repeat */

	/* note: if this is set PySequence_Check() returns True,
	 * but in this case we dont want to be treated as a seq */
	NULL,                                        /* sq_item */

	NULL,                                        /* sq_slice */
	NULL,                                        /* sq_ass_item */
	NULL,                                        /* *was* sq_ass_slice */
	(objobjproc)bpy_bmdeformvert_contains,       /* sq_contains */
	(binaryfunc) NULL,                           /* sq_inplace_concat */
	(ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bmdeformvert_as_mapping = {
	(lenfunc)bpy_bmdeformvert_len,
	(binaryfunc)bpy_bmdeformvert_subscript,
	(objobjargproc)bpy_bmdeformvert_ass_subscript
};

/* Methods
 * ======= */

PyDoc_STRVAR(bpy_bmdeformvert_keys_doc,
".. method:: keys()\n"
"\n"
"   Return the group indices used by this vertex\n"
"   (matching pythons dict.keys() functionality).\n"
"\n"
"   :return: the deform group this vertex uses\n"
"   :rtype: list of ints\n"
);
static PyObject *bpy_bmdeformvert_keys(BPy_BMDeformVert *self)
{
	PyObject *ret;
	int i;
	MDeformWeight *dw = self->data->dw;

	ret = PyList_New(self->data->totweight);
	for (i = 0; i < self->data->totweight; i++, dw++) {
		PyList_SET_ITEM(ret, i, PyLong_FromLong(dw->def_nr));
	}

	return ret;
}

PyDoc_STRVAR(bpy_bmdeformvert_values_doc,
".. method:: values()\n"
"\n"
"   Return the weights of the deform vertex\n"
"   (matching pythons dict.values() functionality).\n"
"\n"
"   :return: The weights that influence this vertex\n"
"   :rtype: list of floats\n"
);
static PyObject *bpy_bmdeformvert_values(BPy_BMDeformVert *self)
{
	PyObject *ret;
	int i;
	MDeformWeight *dw = self->data->dw;

	ret = PyList_New(self->data->totweight);
	for (i = 0; i < self->data->totweight; i++, dw++) {
		PyList_SET_ITEM(ret, i, PyFloat_FromDouble(dw->weight));
	}

	return ret;
}

PyDoc_STRVAR(bpy_bmdeformvert_items_doc,
".. method:: items()\n"
"\n"
"   Return (group, weight) pairs for this vertex\n"
"   (matching pythons dict.items() functionality).\n"
"\n"
"   :return: (key, value) pairs for each deform weight of this vertex.\n"
"   :rtype: list of tuples\n"
);
static PyObject *bpy_bmdeformvert_items(BPy_BMDeformVert *self)
{
	PyObject *ret;
	PyObject *item;
	int i;
	MDeformWeight *dw = self->data->dw;

	ret = PyList_New(self->data->totweight);
	for (i = 0; i < self->data->totweight; i++, dw++) {
		item = PyTuple_New(2);

		PyTuple_SET_ITEM(item, 0, PyLong_FromLong(dw->def_nr));
		PyTuple_SET_ITEM(item, 1, PyFloat_FromDouble(dw->weight));

		PyList_SET_ITEM(ret, i, item);
	}

	return ret;
}

PyDoc_STRVAR(bpy_bmdeformvert_get_doc,
".. method:: get(key, default=None)\n"
"\n"
"   Returns the deform weight matching the key or default\n"
"   when not found (matches pythons dictionary function of the same name).\n"
"\n"
"   :arg key: The key associated with deform weight.\n"
"   :type key: int\n"
"   :arg default: Optional argument for the value to return if\n"
"      *key* is not found.\n"
"   :type default: Undefined\n"
);
static PyObject *bpy_bmdeformvert_get(BPy_BMDeformVert *self, PyObject *args)
{
	int key;
	PyObject *def = Py_None;

	if (!PyArg_ParseTuple(args, "i|O:get", &key, &def)) {
		return NULL;
	}
	else {
		MDeformWeight *dw = defvert_find_index(self->data, key);

		if (dw) {
			return PyFloat_FromDouble(dw->weight);
		}
		else {
			return Py_INCREF(def), def;
		}
	}
}


PyDoc_STRVAR(bpy_bmdeformvert_clear_doc,
".. method:: clear()\n"
"\n"
"   Clears all weights.\n"
);
static PyObject *bpy_bmdeformvert_clear(BPy_BMDeformVert *self)
{
	defvert_clear(self->data);

	Py_RETURN_NONE;
}

static struct PyMethodDef bpy_bmdeformvert_methods[] = {
	{"keys",    (PyCFunction)bpy_bmdeformvert_keys,    METH_NOARGS,  bpy_bmdeformvert_keys_doc},
	{"values",  (PyCFunction)bpy_bmdeformvert_values,  METH_NOARGS,  bpy_bmdeformvert_values_doc},
	{"items",   (PyCFunction)bpy_bmdeformvert_items,   METH_NOARGS,  bpy_bmdeformvert_items_doc},
	{"get",     (PyCFunction)bpy_bmdeformvert_get,     METH_VARARGS, bpy_bmdeformvert_get_doc},
	/* BMESH_TODO pop, popitem, update */
	{"clear",   (PyCFunction)bpy_bmdeformvert_clear,   METH_NOARGS,  bpy_bmdeformvert_clear_doc},
	{NULL, NULL, 0, NULL}
};

PyTypeObject BPy_BMDeformVert_Type = {{{0}}}; /* bm.loops.layers.uv.active */

static void bm_init_types_bmdvert(void)
{
	BPy_BMDeformVert_Type.tp_basicsize = sizeof(BPy_BMDeformVert);

	BPy_BMDeformVert_Type.tp_name = "BMDeformVert";

	BPy_BMDeformVert_Type.tp_doc = NULL; // todo

	BPy_BMDeformVert_Type.tp_as_sequence = &bpy_bmdeformvert_as_sequence;
	BPy_BMDeformVert_Type.tp_as_mapping = &bpy_bmdeformvert_as_mapping;

	BPy_BMDeformVert_Type.tp_methods = bpy_bmdeformvert_methods;

	BPy_BMDeformVert_Type.tp_flags = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_BMDeformVert_Type);
}

int BPy_BMDeformVert_AssignPyObject(struct MDeformVert *dvert, PyObject *value)
{
	if (UNLIKELY(!BPy_BMDeformVert_Check(value))) {
		PyErr_Format(PyExc_TypeError, "expected BMDeformVert, not a %.200s", Py_TYPE(value)->tp_name);
		return -1;
	}
	else {
		MDeformVert *dvert_src = ((BPy_BMDeformVert *)value)->data;
		if (LIKELY(dvert != dvert_src)) {
			defvert_copy(dvert, dvert_src);
		}
		return 0;
	}
}

PyObject *BPy_BMDeformVert_CreatePyObject(struct MDeformVert *dvert)
{
	BPy_BMDeformVert *self = PyObject_New(BPy_BMDeformVert, &BPy_BMDeformVert_Type);
	self->data = dvert;
	return (PyObject *)self;
}

/* --- End Mesh Deform Vert --- */


/* call to init all types */
void BPy_BM_init_types_meshdata(void)
{
	bm_init_types_bmtexpoly();
	bm_init_types_bmloopuv();
	bm_init_types_bmloopcol();
	bm_init_types_bmdvert();
	bm_init_types_bmvertskin();
}

