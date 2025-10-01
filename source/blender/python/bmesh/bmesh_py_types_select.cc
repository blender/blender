/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the types for 'BMesh.select_history'
 * sequence and iterator.
 *
 * select_history is very loosely based on pythons set() type,
 * since items can only exist once. however they do have an order.
 */

#include <Python.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "bmesh.hh"

#include "bmesh_py_types.hh"
#include "bmesh_py_types_select.hh"

#include "../generic/python_utildefines.hh"

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmeditselseq_active_doc,
    "The last selected element or None (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMVert`, "
    ":class:`bmesh.types.BMEdge` or :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmeditselseq_active_get(BPy_BMEditSelSeq *self, void * /*closure*/)
{
  BMEditSelection *ese;
  BPY_BM_CHECK_OBJ(self);

  if ((ese = static_cast<BMEditSelection *>(self->bm->selected.last))) {
    return BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
  }

  Py_RETURN_NONE;
}

static PyGetSetDef bpy_bmeditselseq_getseters[] = {
    {"active",
     (getter)bpy_bmeditselseq_active_get,
     (setter) nullptr,
     bpy_bmeditselseq_active_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmeditselseq_validate_doc,
    ".. method:: validate()\n"
    "\n"
    "   Ensures all elements in the selection history are selected.\n");
static PyObject *bpy_bmeditselseq_validate(BPy_BMEditSelSeq *self)
{
  BPY_BM_CHECK_OBJ(self);
  BM_select_history_validate(self->bm);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmeditselseq_clear_doc,
    ".. method:: clear()\n"
    "\n"
    "   Empties the selection history.\n");
static PyObject *bpy_bmeditselseq_clear(BPy_BMEditSelSeq *self)
{
  BPY_BM_CHECK_OBJ(self);
  BM_select_history_clear(self->bm);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmeditselseq_add_doc,
    ".. method:: add(element)\n"
    "\n"
    "   Add an element to the selection history (no action taken if its already added).\n");
static PyObject *bpy_bmeditselseq_add(BPy_BMEditSelSeq *self, BPy_BMElem *value)
{
  const char *error_prefix = "select_history.add(...)";
  BPY_BM_CHECK_OBJ(self);

  if ((BPy_BMVert_Check(value) || BPy_BMEdge_Check(value) || BPy_BMFace_Check(value)) == false) {
    PyErr_Format(PyExc_TypeError,
                 "%s: expected a BMVert/BMedge/BMFace not a %.200s",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_SOURCE_OBJ(self->bm, error_prefix, value);

  BM_select_history_store(self->bm, value->ele);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmeditselseq_remove_doc,
    ".. method:: remove(element)\n"
    "\n"
    "   Remove an element from the selection history.\n");
static PyObject *bpy_bmeditselseq_remove(BPy_BMEditSelSeq *self, BPy_BMElem *value)
{
  const char *error_prefix = "select_history.remove(...)";
  BPY_BM_CHECK_OBJ(self);

  if ((BPy_BMVert_Check(value) || BPy_BMEdge_Check(value) || BPy_BMFace_Check(value)) == false) {
    PyErr_Format(PyExc_TypeError,
                 "%s: expected a BMVert/BMedge/BMFace not a %.200s",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_SOURCE_OBJ(self->bm, error_prefix, value);

  if (BM_select_history_remove(self->bm, value->ele) == false) {
    PyErr_Format(PyExc_ValueError, "%s: element not found in selection history", error_prefix);
    return nullptr;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmeditselseq_discard_doc,
    ".. method:: discard(element)\n"
    "\n"
    "   Discard an element from the selection history.\n"
    "\n"
    "   Like remove but doesn't raise an error when the elements not in the selection list.\n");
static PyObject *bpy_bmeditselseq_discard(BPy_BMEditSelSeq *self, BPy_BMElem *value)
{
  const char *error_prefix = "select_history.discard()";
  BPY_BM_CHECK_OBJ(self);

  if ((BPy_BMVert_Check(value) || BPy_BMEdge_Check(value) || BPy_BMFace_Check(value)) == false) {
    PyErr_Format(PyExc_TypeError,
                 "%s: expected a BMVert/BMedge/BMFace not a %.200s",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_SOURCE_OBJ(self->bm, error_prefix, value);

  BM_select_history_remove(self->bm, value->ele);

  Py_RETURN_NONE;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_bmeditselseq_methods[] = {
    {"validate",
     (PyCFunction)bpy_bmeditselseq_validate,
     METH_NOARGS,
     bpy_bmeditselseq_validate_doc},
    {"clear", (PyCFunction)bpy_bmeditselseq_clear, METH_NOARGS, bpy_bmeditselseq_clear_doc},

    {"add", (PyCFunction)bpy_bmeditselseq_add, METH_O, bpy_bmeditselseq_add_doc},
    {"remove", (PyCFunction)bpy_bmeditselseq_remove, METH_O, bpy_bmeditselseq_remove_doc},
    {"discard", (PyCFunction)bpy_bmeditselseq_discard, METH_O, bpy_bmeditselseq_discard_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/* Sequences
 * ========= */

static Py_ssize_t bpy_bmeditselseq_length(BPy_BMEditSelSeq *self)
{
  BPY_BM_CHECK_INT(self);

  return BLI_listbase_count(&self->bm->selected);
}

static PyObject *bpy_bmeditselseq_subscript_int(BPy_BMEditSelSeq *self, Py_ssize_t keynum)
{
  BMEditSelection *ese;

  BPY_BM_CHECK_OBJ(self);

  if (keynum < 0) {
    ese = static_cast<BMEditSelection *>(BLI_rfindlink(&self->bm->selected, -1 - keynum));
  }
  else {
    ese = static_cast<BMEditSelection *>(BLI_findlink(&self->bm->selected, keynum));
  }

  if (ese) {
    return BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
  }

  PyErr_Format(PyExc_IndexError, "BMElemSeq[index]: index %d out of range", keynum);
  return nullptr;
}

static PyObject *bpy_bmeditselseq_subscript_slice(BPy_BMEditSelSeq *self,
                                                  Py_ssize_t start,
                                                  Py_ssize_t stop)
{
  int count = 0;

  PyObject *list;
  BMEditSelection *ese;

  BPY_BM_CHECK_OBJ(self);

  list = PyList_New(0);

  /* First loop up-until the start. */
  for (ese = static_cast<BMEditSelection *>(self->bm->selected.first); ese; ese = ese->next) {
    if (count == start) {
      break;
    }
    count++;
  }

  /* Add items until stop. */
  for (; ese; ese = ese->next) {
    PyList_APPEND(list, BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head));
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
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    return bpy_bmeditselseq_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return nullptr;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "BMElemSeq[slice]: slice steps not supported");
      return nullptr;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return bpy_bmeditselseq_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }

    Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

    /* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
    if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
      return nullptr;
    }
    if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
      return nullptr;
    }

    if (start < 0 || stop < 0) {
      /* only get the length for negative values */
      const Py_ssize_t len = bpy_bmeditselseq_length(self);
      if (start < 0) {
        start += len;
        CLAMP_MIN(start, 0);
      }
      if (stop < 0) {
        stop += len;
        CLAMP_MIN(stop, 0);
      }
    }

    if (stop - start <= 0) {
      return PyList_New(0);
    }

    return bpy_bmeditselseq_subscript_slice(self, start, stop);
  }

  PyErr_SetString(PyExc_AttributeError, "BMElemSeq[key]: invalid key, key must be an int");
  return nullptr;
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
    /*sq_length*/ (lenfunc)bpy_bmeditselseq_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* Only set this so `PySequence_Check()` returns True. */
    /*sq_item*/ (ssizeargfunc)bpy_bmeditselseq_subscript_int,
    /*was_sq_slice*/ nullptr,
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr,
    /*sq_contains*/ (objobjproc)bpy_bmeditselseq_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods bpy_bmeditselseq_as_mapping = {
    /*mp_length*/ (lenfunc)bpy_bmeditselseq_length,
    /*mp_subscript*/ (binaryfunc)bpy_bmeditselseq_subscript,
    /*mp_ass_subscript*/ (objobjargproc) nullptr,
};

/* Iterator
 * -------- */

static PyObject *bpy_bmeditselseq_iter(BPy_BMEditSelSeq *self)
{
  BPy_BMEditSelIter *py_iter;

  BPY_BM_CHECK_OBJ(self);
  py_iter = (BPy_BMEditSelIter *)BPy_BMEditSelIter_CreatePyObject(self->bm);
  py_iter->ese = static_cast<BMEditSelection *>(self->bm->selected.first);
  return (PyObject *)py_iter;
}

static PyObject *bpy_bmeditseliter_next(BPy_BMEditSelIter *self)
{
  BMEditSelection *ese = self->ese;
  if (ese == nullptr) {
    PyErr_SetNone(PyExc_StopIteration);
    return nullptr;
  }

  self->ese = ese->next;
  return BPy_BMElem_CreatePyObject(self->bm, &ese->ele->head);
}

PyTypeObject BPy_BMEditSelSeq_Type;
PyTypeObject BPy_BMEditSelIter_Type;

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

void BPy_BM_init_types_select()
{
  BPy_BMEditSelSeq_Type.tp_basicsize = sizeof(BPy_BMEditSelSeq);
  BPy_BMEditSelIter_Type.tp_basicsize = sizeof(BPy_BMEditSelIter);

  BPy_BMEditSelSeq_Type.tp_name = "BMEditSelSeq";
  BPy_BMEditSelIter_Type.tp_name = "BMEditSelIter";

  BPy_BMEditSelSeq_Type.tp_doc = nullptr; /* todo */
  BPy_BMEditSelIter_Type.tp_doc = nullptr;

  BPy_BMEditSelSeq_Type.tp_repr = (reprfunc) nullptr;
  BPy_BMEditSelIter_Type.tp_repr = (reprfunc) nullptr;

  BPy_BMEditSelSeq_Type.tp_getset = bpy_bmeditselseq_getseters;
  BPy_BMEditSelIter_Type.tp_getset = nullptr;

  BPy_BMEditSelSeq_Type.tp_methods = bpy_bmeditselseq_methods;
  BPy_BMEditSelIter_Type.tp_methods = nullptr;

  BPy_BMEditSelSeq_Type.tp_as_sequence = &bpy_bmeditselseq_as_sequence;

  BPy_BMEditSelSeq_Type.tp_as_mapping = &bpy_bmeditselseq_as_mapping;

  BPy_BMEditSelSeq_Type.tp_iter = (getiterfunc)bpy_bmeditselseq_iter;

  /* Only 1 iterator so far. */
  BPy_BMEditSelIter_Type.tp_iternext = (iternextfunc)bpy_bmeditseliter_next;

  BPy_BMEditSelSeq_Type.tp_dealloc = nullptr;   //(destructor)bpy_bmeditselseq_dealloc;
  BPy_BMEditSelIter_Type.tp_dealloc = nullptr;  //(destructor)bpy_bmvert_dealloc;

  BPy_BMEditSelSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMEditSelIter_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  PyType_Ready(&BPy_BMEditSelSeq_Type);
  PyType_Ready(&BPy_BMEditSelIter_Type);
}

/* utility function */

int BPy_BMEditSel_Assign(BPy_BMesh *self, PyObject *value)
{
  const char *error_prefix = "BMesh.select_history = value";
  BPY_BM_CHECK_INT(self);

  BMesh *bm = self->bm;

  Py_ssize_t value_num;
  BMElem **value_array = static_cast<BMElem **>(
      BPy_BMElem_PySeq_As_Array(&bm,
                                value,
                                0,
                                PY_SSIZE_T_MAX,
                                &value_num,
                                BM_VERT | BM_EDGE | BM_FACE,
                                true,
                                true,
                                error_prefix));

  if (value_array == nullptr) {
    return -1;
  }

  BM_select_history_clear(bm);

  for (Py_ssize_t i = 0; i < value_num; i++) {
    BM_select_history_store_notest(bm, value_array[i]);
  }

  PyMem_FREE(value_array);
  return 0;
}
