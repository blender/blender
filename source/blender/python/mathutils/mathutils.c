/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#ifndef MATH_STANDALONE
#  include "BLI_dynstr.h"
#endif

PyDoc_STRVAR(
    M_Mathutils_doc,
    "This module provides access to math operations.\n"
    "\n"
    ".. note::\n"
    "\n"
    "   Classes, methods and attributes that accept vectors also accept other numeric sequences,\n"
    "   such as tuples, lists.\n"
    "\n"
    "The :mod:`mathutils` module provides the following classes:\n"
    "\n"
    "- :class:`Color`,\n"
    "- :class:`Euler`,\n"
    "- :class:`Matrix`,\n"
    "- :class:`Quaternion`,\n"
    "- :class:`Vector`,\n");
static int mathutils_array_parse_fast(float *array,
                                      int size,
                                      PyObject *value_fast,
                                      const char *error_prefix)
{
  PyObject *item;
  PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);

  int i;

  i = size;
  do {
    i--;
    if (((array[i] = PyFloat_AsDouble(item = value_fast_items[i])) == -1.0f) && PyErr_Occurred()) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s: sequence index %d expected a number, "
                   "found '%.200s' type, ",
                   error_prefix,
                   i,
                   Py_TYPE(item)->tp_name);
      size = -1;
      break;
    }
  } while (i);

  return size;
}

Py_hash_t mathutils_array_hash(const float *array, size_t array_len)
{
  int i;
  Py_uhash_t x; /* Unsigned for defined overflow behavior. */
  Py_hash_t y;
  Py_uhash_t mult;
  Py_ssize_t len;

  mult = _PyHASH_MULTIPLIER;
  len = array_len;
  x = 0x345678UL;
  i = 0;
  while (--len >= 0) {
    y = _Py_HashDouble(NULL, (double)(array[i++]));
    if (y == -1) {
      return -1;
    }
    x = (x ^ y) * mult;
    /* the cast might truncate len; that doesn't change hash stability */
    mult += (Py_hash_t)(82520UL + len + len);
  }
  x += 97531UL;
  if (x == (Py_uhash_t)-1) {
    x = -2;
  }
  return x;
}

int mathutils_array_parse(
    float *array, int array_num_min, int array_num_max, PyObject *value, const char *error_prefix)
{
  const uint flag = array_num_max;
  int num;

  array_num_max &= ~MU_ARRAY_FLAGS;

#if 1 /* approx 6x speedup for mathutils types */

  if ((num = VectorObject_Check(value) ? ((VectorObject *)value)->vec_num : 0) ||
      (num = EulerObject_Check(value) ? 3 : 0) || (num = QuaternionObject_Check(value) ? 4 : 0) ||
      (num = ColorObject_Check(value) ? 3 : 0))
  {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }

    if (flag & MU_ARRAY_SPILL) {
      CLAMP_MAX(num, array_num_max);
    }

    if (num > array_num_max || num < array_num_min) {
      if (array_num_max == array_num_min) {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence length is %d, expected %d",
                     error_prefix,
                     num,
                     array_num_max);
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence length is %d, expected [%d - %d]",
                     error_prefix,
                     num,
                     array_num_min,
                     array_num_max);
      }
      return -1;
    }

    memcpy(array, ((const BaseMathObject *)value)->data, num * sizeof(float));
  }
  else
#endif
  {
    PyObject *value_fast = NULL;

    /* non list/tuple cases */
    if (!(value_fast = PySequence_Fast(value, error_prefix))) {
      /* PySequence_Fast sets the error */
      return -1;
    }

    num = PySequence_Fast_GET_SIZE(value_fast);

    if (flag & MU_ARRAY_SPILL) {
      CLAMP_MAX(num, array_num_max);
    }

    if (num > array_num_max || num < array_num_min) {
      if (array_num_max == array_num_min) {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence length is %d, expected %d",
                     error_prefix,
                     num,
                     array_num_max);
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence length is %d, expected [%d - %d]",
                     error_prefix,
                     num,
                     array_num_min,
                     array_num_max);
      }
      Py_DECREF(value_fast);
      return -1;
    }

    num = mathutils_array_parse_fast(array, num, value_fast, error_prefix);
    Py_DECREF(value_fast);
  }

  if (num != -1) {
    if (flag & MU_ARRAY_ZERO) {
      const int array_num_left = array_num_max - num;
      if (array_num_left) {
        memset(&array[num], 0, sizeof(float) * array_num_left);
      }
    }
  }

  return num;
}

int mathutils_array_parse_alloc(float **array,
                                int array_num,
                                PyObject *value,
                                const char *error_prefix)
{
  int num;

#if 1 /* approx 6x speedup for mathutils types */

  if ((num = VectorObject_Check(value) ? ((VectorObject *)value)->vec_num : 0) ||
      (num = EulerObject_Check(value) ? 3 : 0) || (num = QuaternionObject_Check(value) ? 4 : 0) ||
      (num = ColorObject_Check(value) ? 3 : 0))
  {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }

    if (num < array_num) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s: sequence size is %d, expected > %d",
                   error_prefix,
                   num,
                   array_num);
      return -1;
    }

    *array = PyMem_Malloc(num * sizeof(float));
    memcpy(*array, ((const BaseMathObject *)value)->data, num * sizeof(float));
    return num;
  }

#endif

  PyObject *value_fast = NULL;
  // *array = NULL;
  int ret;

  /* non list/tuple cases */
  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  num = PySequence_Fast_GET_SIZE(value_fast);

  if (num < array_num) {
    Py_DECREF(value_fast);
    PyErr_Format(PyExc_ValueError,
                 "%.200s: sequence size is %d, expected > %d",
                 error_prefix,
                 num,
                 array_num);
    return -1;
  }

  *array = PyMem_Malloc(num * sizeof(float));

  ret = mathutils_array_parse_fast(*array, num, value_fast, error_prefix);
  Py_DECREF(value_fast);

  if (ret == -1) {
    PyMem_Free(*array);
  }

  return ret;
}

int mathutils_array_parse_alloc_v(float **array,
                                  int array_dim,
                                  PyObject *value,
                                  const char *error_prefix)
{
  PyObject *value_fast;
  const int array_dim_flag = array_dim;
  int i, num;

  /* non list/tuple cases */
  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  num = PySequence_Fast_GET_SIZE(value_fast);

  if (num != 0) {
    PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
    float *fp;

    array_dim &= ~MU_ARRAY_FLAGS;

    fp = *array = PyMem_Malloc(num * array_dim * sizeof(float));

    for (i = 0; i < num; i++, fp += array_dim) {
      PyObject *item = value_fast_items[i];

      if (mathutils_array_parse(fp, array_dim, array_dim_flag, item, error_prefix) == -1) {
        PyMem_Free(*array);
        *array = NULL;
        num = -1;
        break;
      }
    }
  }

  Py_DECREF(value_fast);
  return num;
}

int mathutils_int_array_parse(int *array, int array_dim, PyObject *value, const char *error_prefix)
{
  int size, i;
  PyObject *value_fast, **value_fast_items, *item;

  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  if ((size = PySequence_Fast_GET_SIZE(value_fast)) != array_dim) {
    PyErr_Format(PyExc_ValueError,
                 "%.200s: sequence size is %d, expected %d",
                 error_prefix,
                 size,
                 array_dim);
    Py_DECREF(value_fast);
    return -1;
  }

  value_fast_items = PySequence_Fast_ITEMS(value_fast);
  i = size;
  while (i > 0) {
    i--;
    if (((array[i] = PyC_Long_AsI32(item = value_fast_items[i])) == -1) && PyErr_Occurred()) {
      PyErr_Format(PyExc_TypeError, "%.200s: sequence index %d expected an int", error_prefix, i);
      size = -1;
      break;
    }
  }
  Py_DECREF(value_fast);

  return size;
}

int mathutils_array_parse_alloc_vi(int **array,
                                   int array_dim,
                                   PyObject *value,
                                   const char *error_prefix)
{
  PyObject *value_fast;
  int i, size;

  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  size = PySequence_Fast_GET_SIZE(value_fast);

  if (size != 0) {
    PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
    int *ip;

    ip = *array = PyMem_Malloc(size * array_dim * sizeof(int));

    for (i = 0; i < size; i++, ip += array_dim) {
      PyObject *item = value_fast_items[i];

      if (mathutils_int_array_parse(ip, array_dim, item, error_prefix) == -1) {
        PyMem_Free(*array);
        *array = NULL;
        size = -1;
        break;
      }
    }
  }

  Py_DECREF(value_fast);
  return size;
}

int mathutils_array_parse_alloc_viseq(
    int **array, int **start_table, int **len_table, PyObject *value, const char *error_prefix)
{
  PyObject *value_fast, *subseq;
  int i, size, start, subseq_len;
  int *ip;

  *array = NULL;
  *start_table = NULL;
  *len_table = NULL;
  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  size = PySequence_Fast_GET_SIZE(value_fast);

  if (size != 0) {
    PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);

    *start_table = PyMem_Malloc(size * sizeof(int));
    *len_table = PyMem_Malloc(size * sizeof(int));

    /* First pass to set starts and len, and calculate size of array needed */
    start = 0;
    for (i = 0; i < size; i++) {
      subseq = value_fast_items[i];
      if ((subseq_len = (int)PySequence_Size(subseq)) == -1) {
        PyErr_Format(
            PyExc_ValueError, "%.200s: sequence expected to have subsequences", error_prefix);
        PyMem_Free(*start_table);
        PyMem_Free(*len_table);
        Py_DECREF(value_fast);
        *start_table = NULL;
        *len_table = NULL;
        return -1;
      }
      (*start_table)[i] = start;
      (*len_table)[i] = subseq_len;
      start += subseq_len;
    }

    ip = *array = PyMem_Malloc(start * sizeof(int));

    /* Second pass to parse the subsequences into array */
    for (i = 0; i < size; i++) {
      subseq = value_fast_items[i];
      subseq_len = (*len_table)[i];

      if (mathutils_int_array_parse(ip, subseq_len, subseq, error_prefix) == -1) {
        PyMem_Free(*array);
        PyMem_Free(*start_table);
        PyMem_Free(*len_table);
        *array = NULL;
        *len_table = NULL;
        *start_table = NULL;
        size = -1;
        break;
      }
      ip += subseq_len;
    }
  }

  Py_DECREF(value_fast);
  return size;
}

int mathutils_any_to_rotmat(float rmat[3][3], PyObject *value, const char *error_prefix)
{
  if (EulerObject_Check(value)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }

    eulO_to_mat3(rmat, ((const EulerObject *)value)->eul, ((const EulerObject *)value)->order);
    return 0;
  }
  if (QuaternionObject_Check(value)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }

    float tquat[4];
    normalize_qt_qt(tquat, ((const QuaternionObject *)value)->quat);
    quat_to_mat3(rmat, tquat);
    return 0;
  }
  if (MatrixObject_Check(value)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }
    if (((MatrixObject *)value)->row_num < 3 || ((MatrixObject *)value)->col_num < 3) {
      PyErr_Format(
          PyExc_ValueError, "%.200s: matrix must have minimum 3x3 dimensions", error_prefix);
      return -1;
    }

    matrix_as_3x3(rmat, (MatrixObject *)value);
    normalize_m3(rmat);
    return 0;
  }

  PyErr_Format(PyExc_TypeError,
               "%.200s: expected a Euler, Quaternion or Matrix type, "
               "found %.200s",
               error_prefix,
               Py_TYPE(value)->tp_name);
  return -1;
}

/* ----------------------------------MATRIX FUNCTIONS-------------------- */

/* Utility functions */

/* LomontRRDCompare4, Ever Faster Float Comparisons by Randy Dillon */
/* XXX We may want to use 'safer' BLI's compare_ff_relative ultimately?
 * LomontRRDCompare4() is an optimized version of Dawson's AlmostEqual2sComplement()
 * (see [1] and [2]).
 * Dawson himself now claims this is not a 'safe' thing to do
 * (pushing ULP method beyond its limits),
 * an recommends using work from [3] instead, which is done in BLI func...
 *
 * [1] http://www.randydillon.org/Papers/2007/everfast.htm
 * [2] http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
 * [3] https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 * instead.
 */
#define SIGNMASK(i) (-(int)(((uint)(i)) >> 31))

int EXPP_FloatsAreEqual(float af, float bf, int maxDiff)
{
  /* solid, fast routine across all platforms
   * with constant time behavior */
  const int ai = *(const int *)(&af);
  const int bi = *(const int *)(&bf);
  const int test = SIGNMASK(ai ^ bi);
  int diff, v1, v2;

  BLI_assert((0 == test) || (0xFFFFFFFF == test));
  diff = (ai ^ (test & 0x7fffffff)) - bi;
  v1 = maxDiff + diff;
  v2 = maxDiff - diff;
  return (v1 | v2) >= 0;
}

/*---------------------- EXPP_VectorsAreEqual -------------------------
 * Builds on EXPP_FloatsAreEqual to test vectors */

int EXPP_VectorsAreEqual(const float *vecA, const float *vecB, int size, int floatSteps)
{
  int x;
  for (x = 0; x < size; x++) {
    if (EXPP_FloatsAreEqual(vecA[x], vecB[x], floatSteps) == 0) {
      return 0;
    }
  }
  return 1;
}

#ifndef MATH_STANDALONE
PyObject *mathutils_dynstr_to_py(struct DynStr *ds)
{
  const int ds_len = BLI_dynstr_get_len(ds); /* space for \0 */
  char *ds_buf = PyMem_Malloc(ds_len + 1);
  PyObject *ret;
  BLI_dynstr_get_cstring_ex(ds, ds_buf);
  BLI_dynstr_free(ds);
  ret = PyUnicode_FromStringAndSize(ds_buf, ds_len);
  PyMem_Free(ds_buf);
  return ret;
}
#endif

/* Mathutils Callbacks */

/* For mathutils internal use only,
 * eventually should re-alloc but to start with we only have a few users. */
#define MATHUTILS_TOT_CB 17
static Mathutils_Callback *mathutils_callbacks[MATHUTILS_TOT_CB] = {NULL};

uchar Mathutils_RegisterCallback(Mathutils_Callback *cb)
{
  uchar i;

  /* find the first free slot */
  for (i = 0; mathutils_callbacks[i]; i++) {
    if (mathutils_callbacks[i] == cb) {
      /* already registered? */
      return i;
    }
  }

  BLI_assert(i + 1 < MATHUTILS_TOT_CB);

  mathutils_callbacks[i] = cb;
  return i;
}

int _BaseMathObject_CheckCallback(BaseMathObject *self)
{
  Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
  if (LIKELY(cb->check(self) != -1)) {
    return 0;
  }
  return -1;
}

int _BaseMathObject_ReadCallback(BaseMathObject *self)
{
  /* NOTE: use macros to check for NULL. */

  Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
  if (LIKELY(cb->get(self, self->cb_subtype) != -1)) {
    return 0;
  }

  if (!PyErr_Occurred()) {
    PyErr_Format(PyExc_RuntimeError, "%s read, user has become invalid", Py_TYPE(self)->tp_name);
  }
  return -1;
}

int _BaseMathObject_WriteCallback(BaseMathObject *self)
{
  Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
  if (LIKELY(cb->set(self, self->cb_subtype) != -1)) {
    return 0;
  }

  if (!PyErr_Occurred()) {
    PyErr_Format(PyExc_RuntimeError, "%s write, user has become invalid", Py_TYPE(self)->tp_name);
  }
  return -1;
}

int _BaseMathObject_ReadIndexCallback(BaseMathObject *self, int index)
{
  Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
  if (LIKELY(cb->get_index(self, self->cb_subtype, index) != -1)) {
    return 0;
  }

  if (!PyErr_Occurred()) {
    PyErr_Format(
        PyExc_RuntimeError, "%s read index, user has become invalid", Py_TYPE(self)->tp_name);
  }
  return -1;
}

int _BaseMathObject_WriteIndexCallback(BaseMathObject *self, int index)
{
  Mathutils_Callback *cb = mathutils_callbacks[self->cb_type];
  if (LIKELY(cb->set_index(self, self->cb_subtype, index) != -1)) {
    return 0;
  }

  if (!PyErr_Occurred()) {
    PyErr_Format(
        PyExc_RuntimeError, "%s write index, user has become invalid", Py_TYPE(self)->tp_name);
  }
  return -1;
}

void _BaseMathObject_RaiseFrozenExc(const BaseMathObject *self)
{
  PyErr_Format(PyExc_TypeError, "%s is frozen (immutable)", Py_TYPE(self)->tp_name);
}

void _BaseMathObject_RaiseNotFrozenExc(const BaseMathObject *self)
{
  PyErr_Format(
      PyExc_TypeError, "%s is not frozen (mutable), call freeze first", Py_TYPE(self)->tp_name);
}

/* #BaseMathObject generic functions for all mathutils types. */

char BaseMathObject_owner_doc[] = "The item this is wrapping or None  (read-only).";
PyObject *BaseMathObject_owner_get(BaseMathObject *self, void *UNUSED(closure))
{
  PyObject *ret = self->cb_user ? self->cb_user : Py_None;
  return Py_INCREF_RET(ret);
}

char BaseMathObject_is_wrapped_doc[] =
    "True when this object wraps external data (read-only).\n\n:type: boolean";
PyObject *BaseMathObject_is_wrapped_get(BaseMathObject *self, void *UNUSED(closure))
{
  return PyBool_FromLong((self->flag & BASE_MATH_FLAG_IS_WRAP) != 0);
}

char BaseMathObject_is_frozen_doc[] =
    "True when this object has been frozen (read-only).\n\n:type: boolean";
PyObject *BaseMathObject_is_frozen_get(BaseMathObject *self, void *UNUSED(closure))
{
  return PyBool_FromLong((self->flag & BASE_MATH_FLAG_IS_FROZEN) != 0);
}

char BaseMathObject_is_valid_doc[] =
    "True when the owner of this data is valid.\n\n:type: boolean";
PyObject *BaseMathObject_is_valid_get(BaseMathObject *self, void *UNUSED(closure))
{
  return PyBool_FromLong(BaseMath_CheckCallback(self) == 0);
}

char BaseMathObject_freeze_doc[] =
    ".. function:: freeze()\n"
    "\n"
    "   Make this object immutable.\n"
    "\n"
    "   After this the object can be hashed, used in dictionaries & sets.\n"
    "\n"
    "   :return: An instance of this object.\n";
PyObject *BaseMathObject_freeze(BaseMathObject *self)
{
  if ((self->flag & BASE_MATH_FLAG_IS_WRAP) || (self->cb_user != NULL)) {
    PyErr_SetString(PyExc_TypeError, "Cannot freeze wrapped/owned data");
    return NULL;
  }

  self->flag |= BASE_MATH_FLAG_IS_FROZEN;

  return Py_INCREF_RET((PyObject *)self);
}

int BaseMathObject_traverse(BaseMathObject *self, visitproc visit, void *arg)
{
  Py_VISIT(self->cb_user);
  return 0;
}

int BaseMathObject_clear(BaseMathObject *self)
{
  Py_CLEAR(self->cb_user);
  return 0;
}

/** Only to validate assumptions when debugging. */
#ifndef NDEBUG
static bool BaseMathObject_is_tracked(BaseMathObject *self)
{
  PyObject *cb_user = self->cb_user;
  self->cb_user = (void *)(uintptr_t)-1;
  bool is_tracked = PyObject_GC_IsTracked((PyObject *)self);
  self->cb_user = cb_user;
  return is_tracked;
}
#endif /* NDEBUG */

void BaseMathObject_dealloc(BaseMathObject *self)
{
  /* only free non wrapped */
  if ((self->flag & BASE_MATH_FLAG_IS_WRAP) == 0) {
    PyMem_Free(self->data);
  }

  if (self->cb_user) {
    BLI_assert(BaseMathObject_is_tracked(self) == true);
    PyObject_GC_UnTrack(self);
    BaseMathObject_clear(self);
  }
  else if (!BaseMathObject_CheckExact(self)) {
    /* Sub-classed types get an extra track (in Pythons internal `subtype_dealloc` function). */
    BLI_assert(BaseMathObject_is_tracked(self) == true);
    PyObject_GC_UnTrack(self);
    BLI_assert(BaseMathObject_is_tracked(self) == false);
  }

  Py_TYPE(self)->tp_free(self);  // PyObject_DEL(self); /* breaks sub-types. */
}

int BaseMathObject_is_gc(BaseMathObject *self)
{
  return self->cb_user != NULL;
}

PyObject *_BaseMathObject_new_impl(PyTypeObject *root_type, PyTypeObject *base_type)
{
  PyObject *obj;
  if (ELEM(base_type, NULL, root_type)) {
    obj = _PyObject_GC_New(root_type);
    if (obj) {
      BLI_assert(BaseMathObject_is_tracked((BaseMathObject *)obj) == false);
    }
  }
  else {
    /* Calls Generic allocation function which always tracks
     * (because `root_type` is flagged for GC). */
    obj = base_type->tp_alloc(base_type, 0);
    if (obj) {
      BLI_assert(BaseMathObject_is_tracked((BaseMathObject *)obj) == true);
      PyObject_GC_UnTrack(obj);
      BLI_assert(BaseMathObject_is_tracked((BaseMathObject *)obj) == false);
    }
  }

  return obj;
}

/*----------------------------MODULE INIT-------------------------*/
static struct PyMethodDef M_Mathutils_methods[] = {
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef M_Mathutils_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "mathutils",
    /*m_doc*/ M_Mathutils_doc,
    /*m_size*/ 0,
    /*m_methods*/ M_Mathutils_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

/* submodules only */
#include "mathutils_geometry.h"
#include "mathutils_interpolate.h"
#ifndef MATH_STANDALONE
#  include "mathutils_bvhtree.h"
#  include "mathutils_kdtree.h"
#  include "mathutils_noise.h"
#endif

PyMODINIT_FUNC PyInit_mathutils(void)
{
  PyObject *mod;
  PyObject *submodule;
  PyObject *sys_modules = PyImport_GetModuleDict();

  if (PyType_Ready(&vector_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&matrix_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&matrix_access_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&euler_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&quaternion_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&color_Type) < 0) {
    return NULL;
  }

  mod = PyModule_Create(&M_Mathutils_module_def);

  /* each type has its own new() function */
  PyModule_AddType(mod, &vector_Type);
  PyModule_AddType(mod, &matrix_Type);
  PyModule_AddType(mod, &euler_Type);
  PyModule_AddType(mod, &quaternion_Type);
  PyModule_AddType(mod, &color_Type);

  /* submodule */
  PyModule_AddObject(mod, "geometry", (submodule = PyInit_mathutils_geometry()));
  /* XXX, python doesn't do imports with this usefully yet
   * 'from mathutils.geometry import PolyFill'
   * ...fails without this. */
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "interpolate", (submodule = PyInit_mathutils_interpolate()));
  /* XXX, python doesn't do imports with this usefully yet
   * 'from mathutils.geometry import PolyFill'
   * ...fails without this. */
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

#ifndef MATH_STANDALONE
  /* Noise submodule */
  PyModule_AddObject(mod, "noise", (submodule = PyInit_mathutils_noise()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  /* BVHTree submodule */
  PyModule_AddObject(mod, "bvhtree", (submodule = PyInit_mathutils_bvhtree()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  /* KDTree_3d submodule */
  PyModule_AddObject(mod, "kdtree", (submodule = PyInit_mathutils_kdtree()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
#endif

  mathutils_matrix_row_cb_index = Mathutils_RegisterCallback(&mathutils_matrix_row_cb);
  mathutils_matrix_col_cb_index = Mathutils_RegisterCallback(&mathutils_matrix_col_cb);
  mathutils_matrix_translation_cb_index = Mathutils_RegisterCallback(
      &mathutils_matrix_translation_cb);

  return mod;
}
