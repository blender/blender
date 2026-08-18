/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup mathutils
 *
 * This file defines the 'mathutils.kdtree' module, a general purpose module to access
 * blenders kdtree for 2D/3D spatial lookups.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_kdtree.hh"
#include "BLI_utildefines.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"

#include "mathutils.hh"
#include "mathutils_kdtree.hh" /* own include */

#include "BLI_strict_flags.hh" /* IWYU pragma: keep. Keep last. */

namespace blender {

struct PyKDTree {
  PyObject_HEAD
  /* Used for 2D/3D KDTrees. */
  void *obj;
  uint maxsize;
  uint count;
  uint count_balance; /* size when we last balanced */
  int dimensions;
};

/* -------------------------------------------------------------------- */
/** \name Utility helper functions
 * \{ */

/**
 * Access the tree, callers must pass a `CoordT` matching #PyKDTree::dimensions.
 */
template<typename CoordT> static KDTree<CoordT> *pykdtree_tree_get(PyKDTree *self)
{
  /* Null when `__init__` never ran or raised an error, `dimensions` is unset in that case. */
  BLI_assert(self->obj == nullptr || CoordT::type_length == self->dimensions);
  return reinterpret_cast<KDTree<CoordT> *>(self->obj);
}

template<typename CoordT>
static void kdtree_nearest_to_py_tuple(const KDTreeNearest<CoordT> *nearest, PyObject *py_retval)
{
  BLI_assert(nearest->index >= 0);
  BLI_assert(PyTuple_GET_SIZE(py_retval) == 3);

  PyTuple_SET_ITEMS(py_retval,
                    Vector_CreatePyObject(nearest->co, CoordT::type_length, nullptr),
                    PyLong_FromLong(nearest->index),
                    PyFloat_FromDouble(nearest->dist));
}

template<typename CoordT>
static PyObject *kdtree_nearest_to_py(const KDTreeNearest<CoordT> *nearest)
{
  PyObject *py_retval = PyTuple_New(3);

  kdtree_nearest_to_py_tuple<CoordT>(nearest, py_retval);

  return py_retval;
}

template<typename CoordT>
static PyObject *kdtree_nearest_to_py_and_check(const KDTreeNearest<CoordT> *nearest)
{
  PyObject *py_retval = PyTuple_New(3);

  if (nearest->index != -1) {
    kdtree_nearest_to_py_tuple<CoordT>(nearest, py_retval);
  }
  else {
    PyC_Tuple_Fill(py_retval, Py_None);
  }

  return py_retval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree
 * \{ */

/* annoying since arg parsing won't check overflow */
#define UINT_IS_NEG(n) ((n) > INT_MAX)

static int PyKDTree__tp_init(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  uint maxsize;
  int dimensions = 3;
  const char *keywords[] = {
      "size",
      "dimensions",
      nullptr,
  };

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "I"  /* `size` */
                                   "|$" /* Optional, keyword only arguments. */
                                   "i"  /* `dimensions` */
                                   ":KDTree",
                                   const_cast<char **>(keywords),
                                   &maxsize,
                                   &dimensions))
  {
    return -1;
  }

  if (UINT_IS_NEG(maxsize)) {
    PyErr_SetString(PyExc_ValueError, "negative 'size' given");
    return -1;
  }

  if (dimensions == 2) {
    self->obj = kdtree_new<float2>(maxsize);
  }
  else if (dimensions == 3) {
    self->obj = kdtree_new<float3>(maxsize);
  }
  else {
    PyErr_SetString(PyExc_ValueError, "dimensions must be 2 or 3");
    return -1;
  }

  self->maxsize = maxsize;
  self->count = 0;
  /* Initialize `uint-max` to avoid crashes on unbalanced trees. */
  self->count_balance = uint(-1);
  self->dimensions = dimensions;

  return 0;
}

static void PyKDTree__tp_dealloc(PyKDTree *self)
{
  if (self->dimensions == 2) {
    kdtree_free<float2>(pykdtree_tree_get<float2>(self));
  }
  else {
    kdtree_free<float3>(pykdtree_tree_get<float3>(self));
  }

  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject *>(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Methods: Insert
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_insert_doc,
    ".. method:: insert(co, index)\n"
    "\n"
    "   Insert a point into the KDTree.\n"
    "\n"
    "   :param co: Point position. Can be 2D or 3D, based on the KDTree dimensions.\n"
    "   :type co: Sequence[float]\n"
    "   :param index: The index of the point (must be non-negative).\n"
    "   :type index: int\n");
static PyObject *py_kdtree_insert(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_co;
  float co[3];
  int index;
  const char *keywords[] = {"co", "index", nullptr};

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "O" /* `co` */
                                   "i" /* `index` */
                                   ":insert",
                                   const_cast<char **>(keywords),
                                   &py_co,
                                   &index))
  {
    return nullptr;
  }

  if (mathutils_array_parse(
          co, self->dimensions, self->dimensions, py_co, "insert: invalid 'co' arg") == -1)
  {
    return nullptr;
  }

  if (index < 0) {
    PyErr_SetString(PyExc_ValueError, "negative index given");
    return nullptr;
  }

  if (self->count >= self->maxsize) {
    PyErr_SetString(PyExc_RuntimeError, "Trying to insert more items than KDTree has room for");
    return nullptr;
  }

  if (self->dimensions == 2) {
    kdtree_insert<float2>(pykdtree_tree_get<float2>(self), index, co);
  }
  else {
    kdtree_insert<float3>(pykdtree_tree_get<float3>(self), index, co);
  }
  self->count++;

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Methods: Balance
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_balance_doc,
    ".. method:: balance()\n"
    "\n"
    "   Balance the tree.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      This builds the entire tree, avoid calling after each insertion.\n");
static PyObject *py_kdtree_balance(PyKDTree *self)
{
  if (self->dimensions == 2) {
    kdtree_balance<float2>(pykdtree_tree_get<float2>(self));
  }
  else {
    kdtree_balance<float3>(pykdtree_tree_get<float3>(self));
  }

  self->count_balance = self->count;
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Methods: Find
 * \{ */

struct PyKDTree_NearestData {
  PyObject *py_filter;
  bool is_error;
};

static int py_find_nearest_cb(void *user_data, int index)
{
  PyKDTree_NearestData *data = static_cast<PyKDTree_NearestData *>(user_data);

  PyObject *py_args = PyTuple_New(1);
  PyTuple_SET_ITEM(py_args, 0, PyLong_FromLong(index));
  PyObject *result = PyObject_CallObject(data->py_filter, py_args);
  Py_DECREF(py_args);

  if (result) {
    bool use_node;
    const int ok = PyC_ParseBool(result, &use_node);
    Py_DECREF(result);
    if (ok) {
      return int(use_node);
    }
  }

  data->is_error = true;
  return -1;
}

template<typename CoordT>
static PyObject *py_kdtree_find_impl(PyKDTree *self, const float *co, PyObject *py_filter)
{
  KDTree<CoordT> *tree = pykdtree_tree_get<CoordT>(self);
  KDTreeNearest<CoordT> nearest;
  nearest.index = -1;

  if (py_filter == Py_None) {
    kdtree_find_nearest<CoordT>(tree, co, &nearest);
  }
  else {
    PyKDTree_NearestData data{py_filter, false};

    kdtree_find_nearest_cb<CoordT>(
        tree, co, &nearest, [&](int index, const CoordT & /*co_nearest*/, float /*dist_sq*/) {
          return py_find_nearest_cb(&data, index);
        });

    if (data.is_error) {
      return nullptr;
    }
  }

  return kdtree_nearest_to_py_and_check<CoordT>(&nearest);
}

PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_find_doc,
    ".. method:: find(co, *, filter=None)\n"
    "\n"
    "   Find nearest point to ``co``.\n"
    "\n"
    "   :param co: Point position. Can be 2D or 3D, based on the KDTree dimensions.\n"
    "   :type co: Sequence[float]\n"
    "   :param filter: function which takes an index and returns True for indices to "
    "include in the search.\n"
    "   :type filter: Callable[[int], bool] | None\n"
    "   :return: Returns (position, index, distance),\n"
    "      or (None, None, None) when no match is found.\n"
    "   :rtype: tuple[:class:`Vector`, int, float] | tuple[None, None, None]\n");
static PyObject *py_kdtree_find(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_co, *py_filter = Py_None;
  float co[3];
  const char *keywords[] = {"co", "filter", nullptr};

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "O"  /* `co` */
                                   "|$" /* Optional, keyword only arguments. */
                                   "O"  /* `filter` */
                                   ":find",
                                   const_cast<char **>(keywords),
                                   &py_co,
                                   &py_filter))
  {
    return nullptr;
  }

  if (mathutils_array_parse(
          co, self->dimensions, self->dimensions, py_co, "find: invalid 'co' arg") == -1)
  {
    return nullptr;
  }

  if (self->count != self->count_balance) {
    PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find()");
    return nullptr;
  }

  if (self->dimensions == 2) {
    return py_kdtree_find_impl<float2>(self, co, py_filter);
  }
  return py_kdtree_find_impl<float3>(self, co, py_filter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Methods: Find N
 * \{ */

template<typename CoordT>
static PyObject *py_kdtree_find_n_impl(PyKDTree *self, const float *co, uint n)
{
  KDTreeNearest<CoordT> *nearest = MEM_new_array_uninitialized<KDTreeNearest<CoordT>>(n, __func__);

  const int found = kdtree_find_nearest_n<CoordT>(pykdtree_tree_get<CoordT>(self), co, nearest, n);

  PyObject *py_list = PyList_New(found);

  for (int i = 0; i < found; i++) {
    PyList_SET_ITEM(py_list, i, kdtree_nearest_to_py<CoordT>(&nearest[i]));
  }

  MEM_delete(nearest);

  return py_list;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_find_n_doc,
    ".. method:: find_n(co, n)\n"
    "\n"
    "   Find nearest ``n`` points to ``co``.\n"
    "\n"
    "   :param co: Point position. Can be 2D or 3D, based on the KDTree dimensions.\n"
    "   :type co: Sequence[float]\n"
    "   :param n: Number of points to find.\n"
    "   :type n: int\n"
    "   :return: Returns a list of tuples (position, index, distance).\n"
    "   :rtype: list[tuple[:class:`Vector`, int, float]]\n");
static PyObject *py_kdtree_find_n(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_co;
  float co[3];
  uint n;
  const char *keywords[] = {"co", "n", nullptr};

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "O" /* `co` */
                                   "I" /* `n` */
                                   ":find_n",
                                   const_cast<char **>(keywords),
                                   &py_co,
                                   &n))
  {
    return nullptr;
  }

  if (mathutils_array_parse(
          co, self->dimensions, self->dimensions, py_co, "find_n: invalid 'co' arg") == -1)
  {
    return nullptr;
  }

  if (UINT_IS_NEG(n)) {
    PyErr_SetString(PyExc_RuntimeError, "negative 'n' given");
    return nullptr;
  }

  if (self->count != self->count_balance) {
    PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find_n()");
    return nullptr;
  }

  if (self->dimensions == 2) {
    return py_kdtree_find_n_impl<float2>(self, co, n);
  }
  return py_kdtree_find_n_impl<float3>(self, co, n);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Methods: Find Range
 * \{ */

template<typename CoordT>
static PyObject *py_kdtree_find_range_impl(PyKDTree *self, const float *co, float radius)
{
  KDTreeNearest<CoordT> *nearest = nullptr;
  const int found = kdtree_range_search<CoordT>(
      pykdtree_tree_get<CoordT>(self), co, &nearest, radius);

  PyObject *py_list = PyList_New(found);

  for (int i = 0; i < found; i++) {
    PyList_SET_ITEM(py_list, i, kdtree_nearest_to_py<CoordT>(&nearest[i]));
  }

  if (nearest) {
    MEM_delete(nearest);
  }

  return py_list;
}

PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_find_range_doc,
    ".. method:: find_range(co, radius)\n"
    "\n"
    "   Find all points within ``radius`` of ``co``.\n"
    "\n"
    "   :param co: Point position. Can be 2D or 3D, based on the KDTree dimensions.\n"
    "   :type co: Sequence[float]\n"
    "   :param radius: Maximum distance to search for points.\n"
    "   :type radius: float\n"
    "   :return: Returns a list of tuples (position, index, distance).\n"
    "   :rtype: list[tuple[:class:`Vector`, int, float]]\n");
static PyObject *py_kdtree_find_range(PyKDTree *self, PyObject *args, PyObject *kwargs)
{
  PyObject *py_co;
  float co[3];
  float radius;

  const char *keywords[] = {"co", "radius", nullptr};

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "O" /* `co` */
                                   "f" /* `radius` */
                                   ":find_range",
                                   const_cast<char **>(keywords),
                                   &py_co,
                                   &radius))
  {
    return nullptr;
  }

  if (mathutils_array_parse(
          co, self->dimensions, self->dimensions, py_co, "find_range: invalid 'co' arg") == -1)
  {
    return nullptr;
  }

  if (radius < 0.0f) {
    PyErr_SetString(PyExc_RuntimeError, "negative radius given");
    return nullptr;
  }

  if (self->count != self->count_balance) {
    PyErr_SetString(PyExc_RuntimeError, "KDTree must be balanced before calling find_range()");
    return nullptr;
  }

  if (self->dimensions == 2) {
    return py_kdtree_find_range_impl<float2>(self, co, radius);
  }
  return py_kdtree_find_range_impl<float3>(self, co, radius);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Type: Get/Set Item Implementation
 * \{ */

/* `KDTree.dimensions`. */
PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_dimensions_doc,
    "KDTree dimensions.\n"
    "\n"
    ":type: int\n");
static PyObject *py_kdtree_dimensions_get(PyKDTree *self, void * /*closure*/)
{
  return PyLong_FromLong(self->dimensions);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name KDTree Type: Get/Set Item Definitions
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyGetSetDef PyKDTree_getseters[] = {
    {"dimensions",
     reinterpret_cast<getter>(py_kdtree_dimensions_get),
     static_cast<setter>(nullptr),
     py_kdtree_dimensions_doc,
     nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef PyKDTree_methods[] = {
    {"insert",
     reinterpret_cast<PyCFunction>(py_kdtree_insert),
     METH_VARARGS | METH_KEYWORDS,
     py_kdtree_insert_doc},
    {"balance",
     reinterpret_cast<PyCFunction>(py_kdtree_balance),
     METH_NOARGS,
     py_kdtree_balance_doc},
    {"find",
     reinterpret_cast<PyCFunction>(py_kdtree_find),
     METH_VARARGS | METH_KEYWORDS,
     py_kdtree_find_doc},
    {"find_n",
     reinterpret_cast<PyCFunction>(py_kdtree_find_n),
     METH_VARARGS | METH_KEYWORDS,
     py_kdtree_find_n_doc},
    {"find_range",
     reinterpret_cast<PyCFunction>(py_kdtree_find_range),
     METH_VARARGS | METH_KEYWORDS,
     py_kdtree_find_range_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    py_KDtree_doc,
    ".. class:: KDTree(size, *, dimensions=3)\n"
    "\n"
    "   A KD-tree initialized to hold up to ``size`` items.\n"
    "\n"
    "   :param size: Maximum number of items.\n"
    "   :type size: int\n"
    "   :param dimensions: The dimensions of the tree (2 or 3).\n"
    "   :type dimensions: int\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      :meth:`KDTree.balance` must have been called before using any of the ``find`` "
    "methods.\n");
PyTypeObject PyKDTree_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "KDTree",
    /*tp_basicsize*/ sizeof(PyKDTree),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(PyKDTree__tp_dealloc),
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ py_KDtree_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ static_cast<PyMethodDef *>(PyKDTree_methods),
    /*tp_members*/ nullptr,
    /*tp_getset*/ PyKDTree_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ reinterpret_cast<initproc>(PyKDTree__tp_init),
    /*tp_alloc*/ static_cast<allocfunc>(PyType_GenericAlloc),
    /*tp_new*/ static_cast<newfunc>(PyType_GenericNew),
    /*tp_free*/ static_cast<freefunc>(nullptr),
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ static_cast<destructor>(nullptr),
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

PyDoc_STRVAR(
    /* Wrap. */
    py_kdtree_doc,
    "Generic 2D/3D KD-tree to perform spatial searches.");
static PyModuleDef kdtree_moduledef = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "mathutils.kdtree",
    /*m_doc*/ py_kdtree_doc,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyMODINIT_FUNC PyInit_mathutils_kdtree()
{
  /* Register the 'KDTree' class */
  if (PyType_Ready(&PyKDTree_Type)) {
    return nullptr;
  }

  PyObject *m = PyModule_Create(&kdtree_moduledef);

  if (m == nullptr) {
    return nullptr;
  }

  PyModule_AddType(m, &PyKDTree_Type);

  return m;
}

}  // namespace blender
