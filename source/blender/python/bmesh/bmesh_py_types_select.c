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

/** \file blender/python/bmesh/bmesh_py_types_select.c
 *  \ingroup pybmesh
 *
 * This file defines the types for 'BMesh.select_history'
 * sequence and iterator.
 *
 * select_history is very loosely based on pythons set() type,
 * since items can only exist once. however they do have an order.
 */

#include <Python.h>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_types_select.h"



#include "../generic/py_capi_utils.h"

#include "bmesh_py_api.h" /* own include */

PyDoc_STRVAR(bpy_bmeditselseq_active_doc,
"The last selected element or None (read-only).\n\n:type: :class:`BMVert`, :class:`BMEdge` or :class:`BMFace`"
);
static PyObject *bpy_bmeditselseq_active_get(BPy_BMEditSelSeq *self, void *UNUSED(closure))
{
	BMEditSelection *ese;
	BPY_BM_CHECK_OBJ(self);

	if ((ese = self->bm->selected.last)) {
		return BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
	}
	else {
		Py_RETURN_NONE;
	}
}

static PyGetSetDef bpy_bmeditselseq_getseters[] = {
	{(char *)"active", (getter)bpy_bmeditselseq_active_get, (setter)NULL, (char *)bpy_bmeditselseq_active_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

PyDoc_STRVAR(bpy_bmeditselseq_validate_doc,
".. method:: validate()\n"
"\n"
"   Ensures all elements in the selection history are selected.\n"
);
static PyObject *bpy_bmeditselseq_validate(BPy_BMEditSelSeq *self)
{
	BPY_BM_CHECK_OBJ(self);
	BM_select_history_validate(self->bm);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmeditselseq_clear_doc,
".. method:: clear()\n"
"\n"
"   Empties the selection history.\n"
);
static PyObject *bpy_bmeditselseq_clear(BPy_BMEditSelSeq *self)
{
	BPY_BM_CHECK_OBJ(self);
	BM_select_history_clear(self->bm);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmeditselseq_add_doc,
".. method:: add(element)\n"
"\n"
"   Add an element to the selection history (no action taken if its already added).\n"
);
static PyObject *bpy_bmeditselseq_add(BPy_BMEditSelSeq *self, BPy_BMElem *value)
{
	BPY_BM_CHECK_OBJ(self);

	if ((BPy_BMVert_Check(value) ||
	     BPy_BMEdge_Check(value) ||
	     BPy_BMFace_Check(value)) == false)
	{
		PyErr_Format(PyExc_TypeError,
		             "Expected a BMVert/BMedge/BMFace not a %.200s", Py_TYPE(value)->tp_name);
		return NULL;
	}

	BPY_BM_CHECK_SOURCE_OBJ(value, self->bm, "select_history.add()");

	BM_select_history_store(self->bm, value->ele);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmeditselseq_remove_doc,
".. method:: remove(element)\n"
"\n"
"   Remove an element from the selection history.\n"
);
static PyObject *bpy_bmeditselseq_remove(BPy_BMEditSelSeq *self, BPy_BMElem *value)
{
	BPY_BM_CHECK_OBJ(self);

	if ((BPy_BMVert_Check(value) ||
	     BPy_BMEdge_Check(value) ||
	     BPy_BMFace_Check(value)) == false)
	{
		PyErr_Format(PyExc_TypeError,
		             "Expected a BMVert/BMedge/BMFace not a %.200s", Py_TYPE(value)->tp_name);
		return NULL;
	}

	BPY_BM_CHECK_SOURCE_OBJ(value, self->bm, "select_history.remove()");

	if (BM_select_history_remove(self->bm, value->ele) == false) {
		PyErr_SetString(PyExc_ValueError,
		                "Element not found in selection history");
		return NULL;
	}

	Py_RETURN_NONE;
}

static struct PyMethodDef bpy_bmeditselseq_methods[] = {
	{"validate", (PyCFunction)bpy_bmeditselseq_validate, METH_NOARGS, bpy_bmeditselseq_validate_doc},
	{"clear",    (PyCFunction)bpy_bmeditselseq_clear,    METH_NOARGS, bpy_bmeditselseq_clear_doc},

	{"add",      (PyCFunction)bpy_bmeditselseq_add,      METH_O,      bpy_bmeditselseq_add_doc},
	{"remove",   (PyCFunction)bpy_bmeditselseq_remove,   METH_O,      bpy_bmeditselseq_remove_doc},
	{NULL, NULL, 0, NULL}
};


/* Sequences
 * ========= */

static Py_ssize_t bpy_bmeditselseq_length(BPy_BMEditSelSeq *self)
{
	BPY_BM_CHECK_INT(self);

	return BLI_countlist(&self->bm->selected);
}

static PyObject *bpy_bmeditselseq_subscript_int(BPy_BMEditSelSeq *self, int keynum)
{
	BMEditSelection *ese;

	BPY_BM_CHECK_OBJ(self);

	if (keynum < 0) {
		ese = BLI_rfindlink(&self->bm->selected, -1 - keynum);
	}
	else {
		ese = BLI_findlink(&self->bm->selected, keynum);
	}

	if (ese) {
		return BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
	}
	else {
		PyErr_Format(PyExc_IndexError,
		             "BMElemSeq[index]: index %d out of range", keynum);
		return NULL;
	}
}

static PyObject *bpy_bmeditselseq_subscript_slice(BPy_BMEditSelSeq *self, Py_ssize_t start, Py_ssize_t stop)
{
	int count = 0;
	bool ok;

	PyObject *list;
	PyObject *item;
	BMEditSelection *ese;

	BPY_BM_CHECK_OBJ(self);

	list = PyList_New(0);

	ese = self->bm->selected.first;

	ok = (ese != NULL);

	if (UNLIKELY(ok == false)) {
		return list;
	}

	/* first loop up-until the start */
	for (ok = true; ok; ok = ((ese = ese->next) != NULL)) {
		if (count == start) {
			break;
		}
		count++;
	}

	/* add items until stop */
	while ((ese = ese->next)) {
		item = BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
		PyList_Append(list, item);
		Py_DECREF(item);

		count++;
		if (count == stop) {
			break;
		}
	}

	return list;
}

static PyObject *bpy_bmeditselseq_subscript(BPy_BMEditSelSeq *self, PyObject *key)
{
	/* don't need error check here */
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bmeditselseq_subscript_int(self, i);
	}
	else if (PySlice_Check(key)) {
		PySliceObject *key_slice = (PySliceObject *)key;
		Py_ssize_t step = 1;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError,
			                "BMElemSeq[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			return bpy_bmeditselseq_subscript_slice(self, 0, PY_SSIZE_T_MAX);
		}
		else {
			Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

			/* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
			if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) return NULL;
			if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop))    return NULL;

			if (start < 0 || stop < 0) {
				/* only get the length for negative values */
				Py_ssize_t len = bpy_bmeditselseq_length(self);
				if (start < 0) start += len;
				if (stop  < 0) stop  += len;
			}

			if (stop - start <= 0) {
				return PyList_New(0);
			}
			else {
				return bpy_bmeditselseq_subscript_slice(self, start, stop);
			}
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError,
		                "BMElemSeq[key]: invalid key, key must be an int");
		return NULL;
	}
}

static int bpy_bmeditselseq_contains(BPy_BMEditSelSeq *self, PyObject *value)
{
	BPy_BMElem *value_bm_ele;

	BPY_BM_CHECK_INT(self);

	value_bm_ele = (BPy_BMElem *)value;
	if (value_bm_ele->bm == self->bm) {
		return BM_select_history_check(self->bm, value_bm_ele->ele);
	}

	return 0;
}

static PySequenceMethods bpy_bmeditselseq_as_sequence = {
	(lenfunc)bpy_bmeditselseq_length,            /* sq_length */
	NULL,                                        /* sq_concat */
	NULL,                                        /* sq_repeat */
	(ssizeargfunc)bpy_bmeditselseq_subscript_int,/* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,                                        /* sq_slice */
	(ssizeobjargproc)NULL,                       /* sq_ass_item */
	NULL,                                        /* *was* sq_ass_slice */
	(objobjproc)bpy_bmeditselseq_contains,       /* sq_contains */
	(binaryfunc) NULL,                           /* sq_inplace_concat */
	(ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bmeditselseq_as_mapping = {
	(lenfunc)bpy_bmeditselseq_length,            /* mp_length */
	(binaryfunc)bpy_bmeditselseq_subscript,      /* mp_subscript */
	(objobjargproc)NULL,                         /* mp_ass_subscript */
};


/* Iterator
 * -------- */

static PyObject *bpy_bmeditselseq_iter(BPy_BMEditSelSeq *self)
{
	BPy_BMEditSelIter *py_iter;

	BPY_BM_CHECK_OBJ(self);
	py_iter = (BPy_BMEditSelIter *)BPy_BMEditSelIter_CreatePyObject(self->bm);
	py_iter->ese = self->bm->selected.first;
	return (PyObject *)py_iter;
}

static PyObject *bpy_bmeditseliter_next(BPy_BMEditSelIter *self)
{
	BMEditSelection *ese = self->ese;
	if (ese == NULL) {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
	else {
		self->ese = ese->next;
		return (PyObject *)BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
	}
}

PyTypeObject BPy_BMEditSelSeq_Type  = {{{0}}};
PyTypeObject BPy_BMEditSelIter_Type = {{{0}}};


PyObject *BPy_BMEditSel_CreatePyObject(BMesh *bm)
{
	BPy_BMEditSelSeq *self = PyObject_New(BPy_BMEditSelSeq, &BPy_BMEditSelSeq_Type);
	self->bm = bm;
	/* caller must initialize 'iter' member */
	return (PyObject *)self;
}

PyObject *BPy_BMEditSelIter_CreatePyObject(BMesh *bm)
{
	BPy_BMEditSelIter *self = PyObject_New(BPy_BMEditSelIter, &BPy_BMEditSelIter_Type);
	self->bm = bm;
	/* caller must initialize 'iter' member */
	return (PyObject *)self;
}

void BPy_BM_init_types_select(void)
{
	BPy_BMEditSelSeq_Type.tp_basicsize     = sizeof(BPy_BMEditSelSeq);
	BPy_BMEditSelIter_Type.tp_basicsize    = sizeof(BPy_BMEditSelIter);

	BPy_BMEditSelSeq_Type.tp_name  = "BMEditSelSeq";
	BPy_BMEditSelIter_Type.tp_name = "BMEditSelIter";

	BPy_BMEditSelSeq_Type.tp_doc   = NULL; /* todo */
	BPy_BMEditSelIter_Type.tp_doc  = NULL;

	BPy_BMEditSelSeq_Type.tp_repr  = (reprfunc)NULL;
	BPy_BMEditSelIter_Type.tp_repr = (reprfunc)NULL;

	BPy_BMEditSelSeq_Type.tp_getset     = bpy_bmeditselseq_getseters;
	BPy_BMEditSelIter_Type.tp_getset = NULL;

	BPy_BMEditSelSeq_Type.tp_methods     = bpy_bmeditselseq_methods;
	BPy_BMEditSelIter_Type.tp_methods = NULL;

	BPy_BMEditSelSeq_Type.tp_as_sequence = &bpy_bmeditselseq_as_sequence;

	BPy_BMEditSelSeq_Type.tp_as_mapping = &bpy_bmeditselseq_as_mapping;

	BPy_BMEditSelSeq_Type.tp_iter = (getiterfunc)bpy_bmeditselseq_iter;

	/* only 1 iteratir so far */
	BPy_BMEditSelIter_Type.tp_iternext = (iternextfunc)bpy_bmeditseliter_next;

	BPy_BMEditSelSeq_Type.tp_dealloc     = NULL; //(destructor)bpy_bmeditselseq_dealloc;
	BPy_BMEditSelIter_Type.tp_dealloc = NULL; //(destructor)bpy_bmvert_dealloc;

	BPy_BMEditSelSeq_Type.tp_flags     = Py_TPFLAGS_DEFAULT;
	BPy_BMEditSelIter_Type.tp_flags    = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_BMEditSelSeq_Type);
	PyType_Ready(&BPy_BMEditSelIter_Type);
}


/* utility function */

/**
 * \note doesn't actually check selection.
 */
int BPy_BMEditSel_Assign(BPy_BMesh *self, PyObject *value)
{
	BMesh *bm;
	Py_ssize_t value_len;
	Py_ssize_t i;
	BMElem **value_array = NULL;

	BPY_BM_CHECK_INT(self);

	bm = self->bm;

	value_array = BPy_BMElem_PySeq_As_Array(&bm, value, 0, PY_SSIZE_T_MAX,
	                                        &value_len, BM_VERT | BM_EDGE | BM_FACE,
	                                        true, true, "BMesh.select_history = value");

	if (value_array == NULL) {
		return -1;
	}

	BM_select_history_clear(bm);

	for (i = 0; i < value_len; i++) {
		BM_select_history_store_notest(bm, value_array[i]);
	}

	PyMem_FREE(value_array);
	return 0;
}
