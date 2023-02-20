/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup mathutils
 *
 * This file defines the 'mathutils.kdtree' module, a general purpose module to access
 * blenders kdtree for 3d spatial lookups.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_kdtree.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "mathutils.h"
#include "mathutils_kdtree.h" /* own include */

#include "BLI_strict_flags.h"

typedef struct {
  PyObject_HEAD
  KDTree_3d *obj;
  uint maxsize;
  uint count;
  uint count_balance; /* size when we last balanced */
} PyKDTree;

/* -------------------------------------------------------------------- */
/* Utility helper functions */

static void kdtree_nearest_to_py_tuple(const KDTreeNearest_3d *nearest, PyObject *py_retval)
{
  BLI_assert(nearest->index >= 0);
  BLI_assert(PyTuple_GET_SIZE(py_retval) == 3);

  PyTuple_SET_ITEMS(py_retval,
                    Vector_CreatePyObject(nearest->co, 3, NULL),
                    PyLong_FromLong(nearest->index),
                    PyFloat_FromDouble(nearest->dist));
}

static PyObject *kdtree_nearest_to_py(const KDTreeNearest_3d *nearest)
{
  PyObject *py_retval;

  py_retval = PyTuple_New(3);

  kdtree_nearest_to_py_tuple(nearest, py_retval);

  return py_retval;
}

static PyObject *kdtree_nearest_to_py_and_check(const KDTreeNearest_3d *nearest)
{
  PyObject *py_retval;

  py_retval = PyTuple_New(3);

  if (nearest->index != -1) {
    kdtree_nearest_to_py_tuple(nearest, py_retval);
  }
  else {
    PyC_Tuple_Fill(py_retval, Py_None);
  }

  return py_retval;
}

/* -------------------------------------------------------------------- */
/* KDTree */

/* annoying since arg parsing won't check overflow */
#define UINT_IS_NEG(n) ((n) > INT_MAX)

static int PyKDTree__tp_init(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  uint maxsize;
  const char *keywords[] = {"size", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I:KDTree", (char **)keywords, &maxsize)) {
    return -1;
  }

  if (UINT_IS_NEG(maxsize)) {
    PyErr_SetString(PyExc_ValueError, "negative 'size' given");
    return -1;
  }

  self->obj = BLI_kdtree_3d_new(maxsize);
  self->maxsize = maxsize;
  self->count = 0;
  self->count_balance = 0;

  return 0;
}

static void PyKDTree__tp_dealloc(PyKDTree *self)
{
  BLI_kdtree_3d_free(self->obj);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(py_kdtree_insert_doc,
             ".. method:: insert(co, index)\n"
             "\n"
             "   Insert a point into the KDTree.\n"
             "\n"
             "   :arg co: Point 3d position.\n"
             "   :type co: float triplet\n"
             "   :arg index: The index of the point.\n"
             "   :type index: int\n");
static PyObject *py_kdtree_insert(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_co;
  float co[3];
  int index;
  const char *keywords[] = {"co", "index", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Oi:insert", (char **)keywords, &py_co, &index)) {
    return NULL;
  }

  if (mathutils_array_parse(co, 3, 3, py_co, "insert: invalid 'co' arg") == -1) {
    return NULL;
  }

  if (index < 0) {
    PyErr_SetString(PyExc_ValueError, "negative index given");
    return NULL;
  }

  if (self->count >= self->maxsize) {
    PyErr_SetString(PyExc_RuntimeError, "Trying to insert more items than KDTree has room for");
    return NULL;
  }

  BLI_kdtree_3d_insert(self->obj, index, co);
  self->count++;

  Py_RETURN_NONE;
}

PyDoc_STRVAR(py_kdtree_balance_doc,
             ".. method:: balance()\n"
             "\n"
             "   Balance the tree.\n"
             "\n"
             ".. note::\n"
             "\n"
             "   This builds the entire tree, avoid calling after each insertion.\n");
static PyObject *py_kdtree_balance(PyKDTree *self)
{
  BLI_kdtree_3d_balance(self->obj);
  self->count_balance = self->count;
  Py_RETURN_NONE;
}

struct PyKDTree_NearestData {
  PyObject *py_filter;
  bool is_error;
};

static int py_find_nearest_cb(void *user_data, int index, const float co[3], float dist_sq)
{
  UNUSED_VARS(co, dist_sq);

  struct PyKDTree_NearestData *data = user_data;

  PyObject *py_args = PyTuple_New(1);
  PyTuple_SET_ITEM(py_args, 0, PyLong_FromLong(index));
  PyObject *result = PyObject_CallObject(data->py_filter, py_args);
  Py_DECREF(py_args);

  if (result) {
    bool use_node;
    const int ok = PyC_ParseBool(result, &use_node);
    Py_DECREF(result);
    if (ok) {
      return (int)use_node;
    }
  }

  data->is_error = true;
  return -1;
}

PyDoc_STRVAR(py_kdtree_find_doc,
             ".. method:: find(co, filter=None)\n"
             "\n"
             "   Find nearest point to ``co``.\n"
             "\n"
             "   :arg co: 3d coordinates.\n"
             "   :type co: float triplet\n"
             "   :arg filter: function which takes an index and returns True for indices to "
             "include in the search.\n"
             "   :type filter: callable\n"
             "   :return: Returns (:class:`Vector`, index, distance).\n"
             "   :rtype: :class:`tuple`\n");
static PyObject *py_kdtree_find(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_co, *py_filter = NULL;
  float co[3];
  KDTreeNearest_3d nearest;
  const char *keywords[] = {"co", "filter", NULL};

  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "O|$O:find", (char **)keywords, &py_co, &py_filter)) {
    return NULL;
  }

  if (mathutils_array_parse(co, 3, 3, py_co, "find: invalid 'co' arg") == -1) {
    return NULL;
  }

  if (self->count != self->count_balance) {
    PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find()");
    return NULL;
  }

  nearest.index = -1;

  if (py_filter == NULL) {
    BLI_kdtree_3d_find_nearest(self->obj, co, &nearest);
  }
  else {
    struct PyKDTree_NearestData data = {0};

    data.py_filter = py_filter;
    data.is_error = false;

    BLI_kdtree_3d_find_nearest_cb(self->obj, co, py_find_nearest_cb, &data, &nearest);

    if (data.is_error) {
      return NULL;
    }
  }

  return kdtree_nearest_to_py_and_check(&nearest);
}

PyDoc_STRVAR(py_kdtree_find_n_doc,
             ".. method:: find_n(co, n)\n"
             "\n"
             "   Find nearest ``n`` points to ``co``.\n"
             "\n"
             "   :arg co: 3d coordinates.\n"
             "   :type co: float triplet\n"
             "   :arg n: Number of points to find.\n"
             "   :type n: int\n"
             "   :return: Returns a list of tuples (:class:`Vector`, index, distance).\n"
             "   :rtype: :class:`list`\n");
static PyObject *py_kdtree_find_n(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_list;
  PyObject *py_co;
  float co[3];
  KDTreeNearest_3d *nearest;
  uint n;
  int i, found;
  const char *keywords[] = {"co", "n", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OI:find_n", (char **)keywords, &py_co, &n)) {
    return NULL;
  }

  if (mathutils_array_parse(co, 3, 3, py_co, "find_n: invalid 'co' arg") == -1) {
    return NULL;
  }

  if (UINT_IS_NEG(n)) {
    PyErr_SetString(PyExc_RuntimeError, "negative 'n' given");
    return NULL;
  }

  if (self->count != self->count_balance) {
    PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find_n()");
    return NULL;
  }

  nearest = MEM_mallocN(sizeof(KDTreeNearest_3d) * n, __func__);

  found = BLI_kdtree_3d_find_nearest_n(self->obj, co, nearest, n);

  py_list = PyList_New(found);

  for (i = 0; i < found; i++) {
    PyList_SET_ITEM(py_list, i, kdtree_nearest_to_py(&nearest[i]));
  }

  MEM_freeN(nearest);

  return py_list;
}

PyDoc_STRVAR(py_kdtree_find_range_doc,
             ".. method:: find_range(co, radius)\n"
             "\n"
             "   Find all points within ``radius`` of ``co``.\n"
             "\n"
             "   :arg co: 3d coordinates.\n"
             "   :type co: float triplet\n"
             "   :arg radius: Distance to search for points.\n"
             "   :type radius: float\n"
             "   :return: Returns a list of tuples (:class:`Vector`, index, distance).\n"
             "   :rtype: :class:`list`\n");
static PyObject *py_kdtree_find_range(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_list;
  PyObject *py_co;
  float co[3];
  KDTreeNearest_3d *nearest = NULL;
  float radius;
  int i, found;

  const char *keywords[] = {"co", "radius", NULL};

  if (!PyArg_ParseTupleAndKeywords(
          args, kwargs, "Of:find_range", (char **)keywords, &py_co, &radius)) {
    return NULL;
  }

  if (mathutils_array_parse(co, 3, 3, py_co, "find_range: invalid 'co' arg") == -1) {
    return NULL;
  }

  if (radius < 0.0f) {
    PyErr_SetString(PyExc_RuntimeError, "negative radius given");
    return NULL;
  }

  if (self->count != self->count_balance) {
    PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find_range()");
    return NULL;
  }

  found = BLI_kdtree_3d_range_search(self->obj, co, &nearest, radius);

  py_list = PyList_New(found);

  for (i = 0; i < found; i++) {
    PyList_SET_ITEM(py_list, i, kdtree_nearest_to_py(&nearest[i]));
  }

  if (nearest) {
    MEM_freeN(nearest);
  }

  return py_list;
}

static PyMethodDef PyKDTree_methods[] = {
    {"insert", (PyCFunction)py_kdtree_insert, METH_VARARGS | METH_KEYWORDS, py_kdtree_insert_doc},
    {"balance", (PyCFunction)py_kdtree_balance, METH_NOARGS, py_kdtree_balance_doc},
    {"find", (PyCFunction)py_kdtree_find, METH_VARARGS | METH_KEYWORDS, py_kdtree_find_doc},
    {"find_n", (PyCFunction)py_kdtree_find_n, METH_VARARGS | METH_KEYWORDS, py_kdtree_find_n_doc},
    {"find_range",
     (PyCFunction)py_kdtree_find_range,
     METH_VARARGS | METH_KEYWORDS,
     py_kdtree_find_range_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(py_KDtree_doc,
             "KdTree(size) -> new kd-tree initialized to hold ``size`` items.\n"
             "\n"
             ".. note::\n"
             "\n"
             "   :class:`KDTree.balance` must have been called before using any of the ``find`` "
             "methods.\n");

PyTypeObject PyKDTree_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    /*tp_name*/ "KDTree",
    /*tp_basicsize*/ sizeof(PyKDTree),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)PyKDTree__tp_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ NULL,
    /*tp_setattr*/ NULL,
    /*tp_as_async*/ NULL,
    /*tp_repr*/ NULL,
    /*tp_as_number*/ NULL,
    /*tp_as_sequence*/ NULL,
    /*tp_as_mapping*/ NULL,
    /*tp_hash*/ NULL,
    /*tp_call*/ NULL,
    /*tp_str*/ NULL,
    /*tp_getattro*/ NULL,
    /*tp_setattro*/ NULL,
    /*tp_as_buffer*/ NULL,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ py_KDtree_doc,
    /*tp_traverse*/ NULL,
    /*tp_clear*/ NULL,
    /*tp_richcompare*/ NULL,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ NULL,
    /*tp_iternext*/ NULL,
    /*tp_methods*/ (struct PyMethodDef *)PyKDTree_methods,
    /*tp_members*/ NULL,
    /*tp_getset*/ NULL,
    /*tp_base*/ NULL,
    /*tp_dict*/ NULL,
    /*tp_descr_get*/ NULL,
    /*tp_descr_set*/ NULL,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)PyKDTree__tp_init,
    /*tp_alloc*/ (allocfunc)PyType_GenericAlloc,
    /*tp_new*/ (newfunc)PyType_GenericNew,
    /*tp_free*/ (freefunc)0,
    /*tp_is_gc*/ NULL,
    /*tp_bases*/ NULL,
    /*tp_mro*/ NULL,
    /*tp_cache*/ NULL,
    /*tp_subclasses*/ NULL,
    /*tp_weaklist*/ NULL,
    /*tp_del*/ (destructor)NULL,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ NULL,
    /*tp_vectorcall*/ NULL,
};

PyDoc_STRVAR(py_kdtree_doc, "Generic 3-dimensional kd-tree to perform spatial searches.");
static struct PyModuleDef kdtree_moduledef = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "mathutils.kdtree",
    /*m_doc*/ py_kdtree_doc,
    /*m_size*/ 0,
    /*m_methods*/ NULL,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyMODINIT_FUNC PyInit_mathutils_kdtree(void)
{
  PyObject *m = PyModule_Create(&kdtree_moduledef);

  if (m == NULL) {
    return NULL;
  }

  /* Register the 'KDTree' class */
  if (PyType_Ready(&PyKDTree_Type)) {
    return NULL;
  }
  PyModule_AddType(m, &PyKDTree_Type);

  return m;
}
