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

/** \file blender/python/bmesh/bmesh_py_api.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh' module.
 */

#include <Python.h>

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_select.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "BKE_tessmesh.h"

#include "DNA_mesh_types.h"

#include "../generic/py_capi_utils.h"

#include "bmesh_py_api.h" /* own include */

static PyGetSetDef bpy_bmeditselseq_getseters[] = {
    // {(char *)"verts", (getter)bpy_bmeditselseq_get, (setter)NULL, (char *)bpy_bmesh_verts_doc, (void *)BM_VERTS_OF_MESH},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef bpy_bmeditselseq_methods[] = {
    // {"select_flush_mode", (PyCFunction)bpy_bmesh_select_flush_mode, METH_NOARGS, bpy_bmesh_select_flush_mode_doc},
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
	BPY_BM_CHECK_OBJ(self);

	if (keynum < 0) keynum += bpy_bmeditselseq_length(self); /* only get length on negative value, may loop entire seq */
	if (keynum >= 0) {
		BMEditSelection *ese = BLI_findlink(&self->bm->selected, keynum);
		if (ese) {
			return BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
		}
	}

	PyErr_Format(PyExc_IndexError,
	             "BMElemSeq[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *bpy_bmeditselseq_subscript_slice(BPy_BMEditSelSeq *self, Py_ssize_t start, Py_ssize_t stop)
{
	int count = 0;
	int ok;

	PyObject *list;
	PyObject *item;
	BMEditSelection *ese;

	BPY_BM_CHECK_OBJ(self);

	list = PyList_New(0);

	ese = self->bm->selected.first;

	ok = (ese != NULL);

	if (UNLIKELY(ok == FALSE)) {
		return list;
	}

	/* first loop up-until the start */
	for (ok = TRUE; ok; ok = ((ese = ese->next) != NULL)) {
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
	/* dont need error check here */
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
				if (stop < 0) start += len;
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
		BMEditSelection *ese_test;
		BMElem *ele;

		ele = value_bm_ele->ele;
		for (ese_test = self->bm->selected.first; ese_test; ese_test = ese_test->next) {
			if (ele == ese_test->ele) {
				return 1;
			}
		}
	}

	return 0;
}

static PySequenceMethods bpy_bmeditselseq_as_sequence = {
    (lenfunc)bpy_bmeditselseq_length,                  /* sq_length */
    NULL,                                        /* sq_concat */
    NULL,                                        /* sq_repeat */
    (ssizeargfunc)bpy_bmeditselseq_subscript_int,      /* sq_item */ /* Only set this so PySequence_Check() returns True */
    NULL,                                        /* sq_slice */
    (ssizeobjargproc)NULL,                       /* sq_ass_item */
    NULL,                                        /* *was* sq_ass_slice */
    (objobjproc)bpy_bmeditselseq_contains,             /* sq_contains */
    (binaryfunc) NULL,                           /* sq_inplace_concat */
    (ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bmeditselseq_as_mapping = {
    (lenfunc)bpy_bmeditselseq_length,                  /* mp_length */
    (binaryfunc)bpy_bmeditselseq_subscript,            /* mp_subscript */
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
		PyErr_SetString(PyExc_StopIteration,
		                "bpy_bmiter_next stop");
		return NULL;
	}
	else {
		self->ese = ese->next;
		return (PyObject *)BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
	}
}

PyTypeObject BPy_BMEditSelSeq_Type     = {{{0}}};
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

void BPy_BM_init_select_types(void)
{
	BPy_BMEditSelSeq_Type.tp_basicsize     = sizeof(BPy_BMEditSelSeq);
	BPy_BMEditSelIter_Type.tp_basicsize    = sizeof(BPy_BMEditSelIter);

	BPy_BMEditSelSeq_Type.tp_name  = "BMEditSelSeq";
	BPy_BMEditSelIter_Type.tp_name = "BMEditSelIter";

	BPy_BMEditSelSeq_Type.tp_doc   = NULL; // todo
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
