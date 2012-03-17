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

/** \file blender/python/bmesh/bmesh_py_types_customdata.c
 *  \ingroup pybmesh
 *
 * This file defines the types for 'BMesh.verts/edges/faces/loops.layers'
 * customdata layer access.
 */

#include <Python.h>

#include "BLI_string.h"
#include "BLI_math_vector.h"

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_types_customdata.h"
#include "bmesh_py_types_meshdata.h"

#include "../mathutils/mathutils.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

static CustomData *bpy_bm_customdata_get(BMesh *bm, char htype)
{
	switch (htype) {
		case BM_VERT:  return &bm->vdata;
		case BM_EDGE:  return &bm->edata;
		case BM_FACE:  return &bm->pdata;
		case BM_LOOP:  return &bm->ldata;
	}

	BLI_assert(0);
	return NULL;
}

static CustomDataLayer *bpy_bmlayeritem_get(BPy_BMLayerItem *self)
{
	CustomData *data = bpy_bm_customdata_get(self->bm, self->htype);
	return &data->layers[CustomData_get_layer_index_n(data, self->type, self->index)];
}

/* py-type definitions
 * ******************* */

/* getseters
 * ========= */

static PyObject *bpy_bmlayeraccess_collection_get(BPy_BMLayerAccess *self, void *flag)
{
	const int type = (int)GET_INT_FROM_POINTER(flag);

	BPY_BM_CHECK_OBJ(self);

	return BPy_BMLayerCollection_CreatePyObject(self->bm, self->htype, type);
}

static PyObject *bpy_bmlayeritem_name_get(BPy_BMLayerItem *self, void *UNUSED(flag))
{
	CustomDataLayer *layer;

	BPY_BM_CHECK_OBJ(self);

	layer = bpy_bmlayeritem_get(self);
	return PyUnicode_FromString(layer->name);
}

static PyGetSetDef bpy_bmlayeraccess_getseters[] = {
    {(char *)"deform", (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_MDEFORMVERT},

    {(char *)"float",  (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_PROP_FLT},
    {(char *)"int",    (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_PROP_INT},
    {(char *)"string", (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_PROP_STR},

    {(char *)"tex",   (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_MTEXPOLY},
    {(char *)"uv",    (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_MLOOPUV},
    {(char *)"color", (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_MLOOPCOL},

    {(char *)"shape",        (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_SHAPEKEY},
    {(char *)"bevel_weight", (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_BWEIGHT},
    {(char *)"crease",       (getter)bpy_bmlayeraccess_collection_get, (setter)NULL, (char *)NULL, (void *)CD_CREASE},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeritem_getseters[] = {
    /* BMESH_TODO, make writeable */
    {(char *)"name", (getter)bpy_bmlayeritem_name_get, (setter)NULL, (char *)NULL, NULL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};


/* Methods
 * ======= */

/* BMLayerCollection
 * ----------------- */

PyDoc_STRVAR(bpy_bmlayercollection_keys_doc,
".. method:: keys()\n"
"\n"
"   Return the identifiers of collection members\n"
"   (matching pythons dict.keys() functionality).\n"
"\n"
"   :return: the identifiers for each member of this collection.\n"
"   :rtype: list of strings\n"
);
static PyObject *bpy_bmlayercollection_keys(BPy_BMLayerCollection *self)
{
	PyObject *ret = PyList_New(0);
	PyObject *item;
	int index;
	CustomData *data;

	BPY_BM_CHECK_OBJ(self);

	data = bpy_bm_customdata_get(self->bm, self->htype);
	index = CustomData_get_layer_index(data, self->type);

	ret = PyList_New(0);

	if (index != -1) {
		int tot = CustomData_number_of_layers(data, self->type);
		for ( ; tot-- > 0; index++) {
			item = PyUnicode_FromString(data->layers[index].name);
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
	}

	return ret;
}

PyDoc_STRVAR(bpy_bmlayercollection_values_doc,
".. method:: items()\n"
"\n"
"   Return the identifiers of collection members\n"
"   (matching pythons dict.items() functionality).\n"
"\n"
"   :return: (key, value) pairs for each member of this collection.\n"
"   :rtype: list of tuples\n"
);
static PyObject *bpy_bmlayercollection_values(BPy_BMLayerCollection *self)
{
	PyObject *ret;
	PyObject *item;
	int index;
	CustomData *data;

	BPY_BM_CHECK_OBJ(self);

	data = bpy_bm_customdata_get(self->bm, self->htype);
	index = CustomData_get_layer_index(data, self->type);

	ret = PyList_New(0);

	if (index != -1) {
		int tot = CustomData_number_of_layers(data, self->type);
		for ( ; tot-- > 0; index++) {
			item = PyTuple_New(2);
			PyTuple_SET_ITEM(item, 0, PyUnicode_FromString(data->layers[index].name));
			PyTuple_SET_ITEM(item, 1, BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index));
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
	}

	return ret;
}

PyDoc_STRVAR(bpy_bmlayercollection_items_doc,
".. method:: values()\n"
"\n"
"   Return the values of collection\n"
"   (matching pythons dict.values() functionality).\n"
"\n"
"   :return: the members of this collection.\n"
"   :rtype: list\n"
);
static PyObject *bpy_bmlayercollection_items(BPy_BMLayerCollection *self)
{
	PyObject *ret;
	PyObject *item;
	int index;
	CustomData *data;

	BPY_BM_CHECK_OBJ(self);

	data = bpy_bm_customdata_get(self->bm, self->htype);
	index = CustomData_get_layer_index(data, self->type);

	ret = PyList_New(0);

	if (index != -1) {
		int tot = CustomData_number_of_layers(data, self->type);
		for ( ; tot-- > 0; index++) {
			item = BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
	}

	return ret;
}

PyDoc_STRVAR(bpy_bmlayercollection_get_doc,
".. method:: get(key, default=None)\n"
"\n"
"   Returns the value of the layer matching the key or default\n"
"   when not found (matches pythons dictionary function of the same name).\n"
"\n"
"   :arg key: The key associated with the layer.\n"
"   :type key: string\n"
"   :arg default: Optional argument for the value to return if\n"
"      *key* is not found.\n"
"   :type default: Undefined\n"
);
static PyObject *bpy_bmlayercollection_get(BPy_BMLayerCollection *self, PyObject *args)
{
	const char *key;
	PyObject *def = Py_None;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
		return NULL;
	}
	else {
		CustomData *data;
		int index;

		data = bpy_bm_customdata_get(self->bm, self->htype);
		index = CustomData_get_named_layer_index(data, self->type, key);

		if (index != -1) {
			return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
		}
	}

	return Py_INCREF(def), def;
}

static struct PyMethodDef bpy_bmelemseq_methods[] = {
    {"keys",     (PyCFunction)bpy_bmlayercollection_keys,     METH_NOARGS,  bpy_bmlayercollection_keys_doc},
    {"values",   (PyCFunction)bpy_bmlayercollection_values,   METH_NOARGS,  bpy_bmlayercollection_values_doc},
    {"items",    (PyCFunction)bpy_bmlayercollection_items,    METH_NOARGS,  bpy_bmlayercollection_items_doc},
    {"get",      (PyCFunction)bpy_bmlayercollection_get,      METH_VARARGS, bpy_bmlayercollection_get_doc},

    /* for later! */
#if 0

	{"new",    (PyCFunction)bpy_bmlayercollection_new,    METH_O, bpy_bmlayercollection_new_doc},
    {"remove", (PyCFunction)bpy_bmlayercollection_new,    METH_O, bpy_bmlayercollection_remove_doc},
#endif
    {NULL, NULL, 0, NULL}
};



/* Sequences
 * ========= */

static Py_ssize_t bpy_bmlayercollection_length(BPy_BMLayerCollection *self)
{
	CustomData *data;

	BPY_BM_CHECK_INT(self);

	data = bpy_bm_customdata_get(self->bm, self->htype);

	return CustomData_number_of_layers(data, self->type);
}

static PyObject *bpy_bmlayercollection_subscript_str(BPy_BMLayerCollection *self, const char *keyname)
{
	CustomData *data;
	int index;

	BPY_BM_CHECK_OBJ(self);

	data = bpy_bm_customdata_get(self->bm, self->htype);
	index = CustomData_get_named_layer_index(data, self->type, keyname);

	if (index != -1) {
		return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
	}
	else {
		PyErr_Format(PyExc_KeyError,
		             "BMLayerCollection[key]: key \"%.200s\" not found", keyname);
		return NULL;
	}
}

static PyObject *bpy_bmlayercollection_subscript_int(BPy_BMLayerCollection *self, int keynum)
{
	Py_ssize_t len;
	BPY_BM_CHECK_OBJ(self);

	len = bpy_bmlayercollection_length(self);

	if (keynum < 0) keynum += len;
	if (keynum >= 0) {
		if (keynum < len) {
			return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, keynum);
		}
	}

	PyErr_Format(PyExc_IndexError,
	             "BMLayerCollection[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *bpy_bmlayercollection_subscript_slice(BPy_BMLayerCollection *self, Py_ssize_t start, Py_ssize_t stop)
{
	Py_ssize_t len = bpy_bmlayercollection_length(self);
	int count = 0;

	PyObject *tuple;

	BPY_BM_CHECK_OBJ(self);

	if (start >= start) start = len - 1;
	if (stop >= stop)   stop  = len - 1;

	tuple = PyTuple_New(stop - start);

	for (count = start; count < stop; count++) {
		PyTuple_SET_ITEM(tuple, count - start, BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, count));
	}

	return tuple;
}

static PyObject *bpy_bmlayercollection_subscript(BPy_BMLayerCollection *self, PyObject *key)
{
	/* dont need error check here */
	if (PyUnicode_Check(key)) {
		return bpy_bmlayercollection_subscript_str(self, _PyUnicode_AsString(key));
	}
	else if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bmlayercollection_subscript_int(self, i);
	}
	else if (PySlice_Check(key)) {
		PySliceObject *key_slice = (PySliceObject *)key;
		Py_ssize_t step = 1;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError,
			                "BMLayerCollection[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			return bpy_bmlayercollection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
		}
		else {
			Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

			/* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
			if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) return NULL;
			if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop))    return NULL;

			if (start < 0 || stop < 0) {
				/* only get the length for negative values */
				Py_ssize_t len = bpy_bmlayercollection_length(self);
				if (start < 0) start += len;
				if (stop < 0) start += len;
			}

			if (stop - start <= 0) {
				return PyTuple_New(0);
			}
			else {
				return bpy_bmlayercollection_subscript_slice(self, start, stop);
			}
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError,
		                "BMLayerCollection[key]: invalid key, key must be an int");
		return NULL;
	}
}

static int bpy_bmlayercollection_contains(BPy_BMLayerCollection *self, PyObject *value)
{
	const char *keyname = _PyUnicode_AsString(value);
	CustomData *data;
	int index;

	BPY_BM_CHECK_INT(self);

	if (keyname == NULL) {
		PyErr_SetString(PyExc_TypeError,
		                "BMLayerCollection.__contains__: expected a string");
		return -1;
	}

	data = bpy_bm_customdata_get(self->bm, self->htype);
	index = CustomData_get_named_layer_index(data, self->type, keyname);

	return (index != -1) ? 1 : 0;
}

static PySequenceMethods bpy_bmlayercollection_as_sequence = {
    (lenfunc)bpy_bmlayercollection_length,       /* sq_length */
    NULL,                                        /* sq_concat */
    NULL,                                        /* sq_repeat */
    (ssizeargfunc)bpy_bmlayercollection_subscript_int, /* sq_item */ /* Only set this so PySequence_Check() returns True */
    NULL,                                        /* sq_slice */
    (ssizeobjargproc)NULL,                       /* sq_ass_item */
    NULL,                                        /* *was* sq_ass_slice */
    (objobjproc)bpy_bmlayercollection_contains,  /* sq_contains */
    (binaryfunc) NULL,                           /* sq_inplace_concat */
    (ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bmlayercollection_as_mapping = {
    (lenfunc)bpy_bmlayercollection_length,       /* mp_length */
    (binaryfunc)bpy_bmlayercollection_subscript, /* mp_subscript */
    (objobjargproc)NULL,                         /* mp_ass_subscript */
};

/* Iterator
 * -------- */

static PyObject *bpy_bmlayercollection_iter(BPy_BMLayerCollection *self)
{
	/* fake it with a list iterator */
	PyObject *ret;
	PyObject *iter = NULL;

	BPY_BM_CHECK_OBJ(self);

	ret = bpy_bmlayercollection_subscript_slice(self, 0, PY_SSIZE_T_MIN);

	if (ret) {
		iter = PyObject_GetIter(ret);
		Py_DECREF(ret);
	}

	return iter;
}

PyTypeObject BPy_BMLayerAccess_Type     = {{{0}}}; /* bm.verts.layers */
PyTypeObject BPy_BMLayerCollection_Type = {{{0}}}; /* bm.verts.layers.uv */
PyTypeObject BPy_BMLayerItem_Type       = {{{0}}}; /* bm.verts.layers.uv["UVMap"] */


PyObject *BPy_BMLayerAccess_CreatePyObject(BMesh *bm, const char htype)
{
	BPy_BMLayerAccess *self = PyObject_New(BPy_BMLayerAccess, &BPy_BMLayerAccess_Type);
	self->bm = bm;
	self->htype = htype;
	return (PyObject *)self;
}

PyObject *BPy_BMLayerCollection_CreatePyObject(BMesh *bm, const char htype, int type)
{
	BPy_BMLayerCollection *self = PyObject_New(BPy_BMLayerCollection, &BPy_BMLayerCollection_Type);
	self->bm = bm;
	self->htype = htype;
	self->type = type;
	return (PyObject *)self;
}

PyObject *BPy_BMLayerItem_CreatePyObject(BMesh *bm, const char htype, int type, int index)
{
	BPy_BMLayerItem *self = PyObject_New(BPy_BMLayerItem, &BPy_BMLayerItem_Type);
	self->bm = bm;
	self->htype = htype;
	self->type = type;
	self->index = index;
	return (PyObject *)self;
}


void BPy_BM_init_types_customdata(void)
{
	BPy_BMLayerAccess_Type.tp_basicsize     = sizeof(BPy_BMLayerAccess);
	BPy_BMLayerCollection_Type.tp_basicsize = sizeof(BPy_BMLayerCollection);
	BPy_BMLayerItem_Type.tp_basicsize       = sizeof(BPy_BMLayerItem);

	BPy_BMLayerAccess_Type.tp_name     = "BMLayerAccess";
	BPy_BMLayerCollection_Type.tp_name = "BMLayerCollection";
	BPy_BMLayerItem_Type.tp_name       = "BMLayerItem";

	BPy_BMLayerAccess_Type.tp_doc     = NULL; // todo
	BPy_BMLayerCollection_Type.tp_doc = NULL;
	BPy_BMLayerItem_Type.tp_doc       = NULL;

	BPy_BMLayerAccess_Type.tp_repr  = (reprfunc)NULL;
	BPy_BMLayerCollection_Type.tp_repr = (reprfunc)NULL;
	BPy_BMLayerItem_Type.tp_repr = (reprfunc)NULL;

	BPy_BMLayerAccess_Type.tp_getset     = bpy_bmlayeraccess_getseters;
	BPy_BMLayerCollection_Type.tp_getset = NULL;
	BPy_BMLayerItem_Type.tp_getset       = bpy_bmlayeritem_getseters;


//	BPy_BMLayerAccess_Type.tp_methods     = bpy_bmeditselseq_methods;
	BPy_BMLayerCollection_Type.tp_methods = bpy_bmelemseq_methods;

	BPy_BMLayerCollection_Type.tp_as_sequence = &bpy_bmlayercollection_as_sequence;

	BPy_BMLayerCollection_Type.tp_as_mapping = &bpy_bmlayercollection_as_mapping;

	BPy_BMLayerCollection_Type.tp_iter = (getiterfunc)bpy_bmlayercollection_iter;

	BPy_BMLayerAccess_Type.tp_dealloc     = NULL; //(destructor)bpy_bmeditselseq_dealloc;
	BPy_BMLayerCollection_Type.tp_dealloc = NULL; //(destructor)bpy_bmvert_dealloc;
	BPy_BMLayerItem_Type.tp_dealloc       = NULL; //(destructor)bpy_bmvert_dealloc;



	BPy_BMLayerAccess_Type.tp_flags     = Py_TPFLAGS_DEFAULT;
	BPy_BMLayerCollection_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMLayerItem_Type.tp_flags       = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_BMLayerAccess_Type);
	PyType_Ready(&BPy_BMLayerCollection_Type);
	PyType_Ready(&BPy_BMLayerItem_Type);
}


/* Per Element Get/Set
 * ******************* */

/**
 * helper function for get/set, NULL return means the error is set
*/
static void *bpy_bmlayeritem_ptr_get(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer)
{
	void *value;
	BMElem *ele = py_ele->ele;
	CustomData *data;

	/* error checking */
	if (UNLIKELY(!BPy_BMLayerItem_Check(py_layer))) {
		PyErr_SetString(PyExc_AttributeError,
		                "BMElem[key]: invalid key, must be a BMLayerItem");
		return NULL;
	}
	else if (UNLIKELY(py_ele->bm != py_layer->bm)) {
		PyErr_SetString(PyExc_ValueError,
		                "BMElem[layer]: layer is from another mesh");
		return NULL;
	}
	else if (UNLIKELY(ele->head.htype != py_layer->htype)) {
		char namestr_1[32], namestr_2[32];
		PyErr_Format(PyExc_ValueError,
		             "Layer/Element type mismatch, expected %.200s got layer type %.200s",
		             BPy_BMElem_StringFromHType_ex(ele->head.htype, namestr_1),
		             BPy_BMElem_StringFromHType_ex(py_layer->htype, namestr_2));
		return NULL;
	}

	data = bpy_bm_customdata_get(py_layer->bm, py_layer->htype);

	value = CustomData_bmesh_get_n(data, ele->head.data, py_layer->type, py_layer->index);

	if (UNLIKELY(value == NULL)) {
		/* this should be fairly unlikely but possible if layers move about after we get them */
		PyErr_SetString(PyExc_KeyError,
		             "BMElem[key]: layer not found");
		return NULL;
	}
	else {
		return value;
	}
}


/**
 *\brief BMElem.__getitem__()
 *
 * assume all error checks are done, eg:
 *
 *     uv = vert[uv_layer]
 */
PyObject *BPy_BMLayerItem_GetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer)
{
	void *value = bpy_bmlayeritem_ptr_get(py_ele, py_layer);
	PyObject *ret;

	if (UNLIKELY(value == NULL)) {
		return NULL;
	}

	switch (py_layer->type) {
		case CD_MDEFORMVERT:
		{
			ret = Py_NotImplemented; /* TODO */
			Py_INCREF(ret);
			break;
		}
		case CD_PROP_FLT:
		{
			ret = PyFloat_FromDouble(*(float *)value);
			break;
		}
		case CD_PROP_INT:
		{
			ret = PyLong_FromSsize_t((Py_ssize_t)(*(int *)value));
			break;
		}
		case CD_PROP_STR:
		{
			MStringProperty *mstring = value;
			ret = PyBytes_FromStringAndSize(mstring->s, BLI_strnlen(mstring->s, sizeof(mstring->s)));
			break;
		}
		case CD_MTEXPOLY:
		{
			ret = Py_NotImplemented; /* TODO */
			Py_INCREF(ret);
			break;
		}
		case CD_MLOOPUV:
		{
			ret = BPy_BMLoopUV_CreatePyObject(value);
			break;
		}
		case CD_MLOOPCOL:
		{
			ret = Py_NotImplemented; /* TODO */
			Py_INCREF(ret);
			break;
		}
		case CD_SHAPEKEY:
		{
			ret = Vector_CreatePyObject((float *)value, 3, Py_WRAP, NULL);
			break;
		}
		case CD_BWEIGHT:
		{
			ret = PyFloat_FromDouble(*(float *)value);
			break;
		}
		case CD_CREASE:
		{
			ret = PyFloat_FromDouble(*(float *)value);
			break;
		}
		default:
		{
			ret = Py_NotImplemented; /* TODO */
			Py_INCREF(ret);
			break;
		}
	}

	return ret;
}

int BPy_BMLayerItem_SetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer, PyObject *py_value)
{
	int ret = 0;
	void *value = bpy_bmlayeritem_ptr_get(py_ele, py_layer);

	if (UNLIKELY(value == NULL)) {
		return -1;
	}

	switch (py_layer->type) {
		case CD_MDEFORMVERT:
		{
			PyErr_SetString(PyExc_AttributeError, "readonly"); /* could make this writeable later */
			ret = -1;
			break;
		}
		case CD_PROP_FLT:
		{
			float tmp_val = PyFloat_AsDouble(py_value);
			if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
				PyErr_Format(PyExc_TypeError, "expected a float, not a %.200s", Py_TYPE(py_value)->tp_name);
				ret = -1;
			}
			else {
				*(float *)value = tmp_val;
			}
			break;
		}
		case CD_PROP_INT:
		{
			int tmp_val = PyLong_AsSsize_t(py_value);
			if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
				PyErr_Format(PyExc_TypeError, "expected an int, not a %.200s", Py_TYPE(py_value)->tp_name);
				ret = -1;
			}
			else {
				*(int *)value = tmp_val;
			}
			break;
		}
		case CD_PROP_STR:
		{
			MStringProperty *mstring = value;
			const char *tmp_val = PyBytes_AsString(py_value);
			if (UNLIKELY(tmp_val == NULL)) {
				PyErr_Format(PyExc_TypeError, "expected bytes, not a %.200s", Py_TYPE(py_value)->tp_name);
				ret = -1;
			}
			else {
				BLI_strncpy(mstring->s, tmp_val, sizeof(mstring->s));
			}
			break;
		}
		case CD_MTEXPOLY:
		{
			PyErr_SetString(PyExc_AttributeError, "readonly"); /* could make this writeable later */
			ret = -1;
			break;
		}
		case CD_MLOOPUV:
		{
			PyErr_SetString(PyExc_AttributeError, "readonly"); /* could make this writeable later */
			ret = -1;
			break;
		}
		case CD_MLOOPCOL:
		{
			PyErr_SetString(PyExc_AttributeError, "readonly");
			ret = -1;
			break;
		}
		case CD_SHAPEKEY:
		{
			float tmp_val[3];
			if (UNLIKELY(mathutils_array_parse(tmp_val, 3, 3, py_value, "BMVert[shape] = value") == -1)) {
				ret = -1;
			}
			else {
				copy_v3_v3((float *)value,tmp_val);
			}
			break;
		}
		case CD_BWEIGHT:
		{
			float tmp_val = PyFloat_AsDouble(py_value);
			if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
				PyErr_Format(PyExc_TypeError, "expected a float, not a %.200s", Py_TYPE(py_value)->tp_name);
				ret = -1;
			}
			else {
				*(float *)value = CLAMPIS(tmp_val, 0.0f, 1.0f);
			}
			break;
		}
		case CD_CREASE:
		{
			float tmp_val = PyFloat_AsDouble(py_value);
			if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
				PyErr_Format(PyExc_TypeError, "expected a float, not a %.200s", Py_TYPE(py_value)->tp_name);
				ret = -1;
			}
			else {
				*(float *)value = CLAMPIS(tmp_val, 0.0f, 1.0f);
			}
			break;
		}
		default:
		{
			PyErr_SetString(PyExc_AttributeError, "readonly / unsupported type");
			ret = -1;
			break;
		}
	}

	return ret;
}
