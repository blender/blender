/*
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
 */

/** \file
 * \ingroup pymathutils
 */

#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

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
    "   such as tuples, lists."
    "\n\n"
    "Submodules:\n"
    "\n"
    ".. toctree::\n"
    "   :maxdepth: 1\n"
    "\n"
    "   mathutils.geometry.rst\n"
    "   mathutils.bvhtree.rst\n"
    "   mathutils.kdtree.rst\n"
    "   mathutils.interpolate.rst\n"
    "   mathutils.noise.rst\n"
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
    if (((array[i] = PyFloat_AsDouble((item = value_fast_items[i]))) == -1.0f) &&
        PyErr_Occurred()) {
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

/**
 * helper function that returns a Python ``__hash__``.
 *
 * \note consistent with the equivalent tuple of floats (CPython's 'tuplehash')
 */
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
    y = _Py_HashDouble((double)(array[i++]));
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

/* helper function returns length of the 'value', -1 on error */
int mathutils_array_parse(
    float *array, int array_min, int array_max, PyObject *value, const char *error_prefix)
{
  const unsigned int flag = array_max;
  int size;

  array_max &= ~MU_ARRAY_FLAGS;

#if 1 /* approx 6x speedup for mathutils types */

  if ((size = VectorObject_Check(value) ? ((VectorObject *)value)->size : 0) ||
      (size = EulerObject_Check(value) ? 3 : 0) ||
      (size = QuaternionObject_Check(value) ? 4 : 0) ||
      (size = ColorObject_Check(value) ? 3 : 0)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }

    if (flag & MU_ARRAY_SPILL) {
      CLAMP_MAX(size, array_max);
    }

    if (size > array_max || size < array_min) {
      if (array_max == array_min) {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence size is %d, expected %d",
                     error_prefix,
                     size,
                     array_max);
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence size is %d, expected [%d - %d]",
                     error_prefix,
                     size,
                     array_min,
                     array_max);
      }
      return -1;
    }

    memcpy(array, ((BaseMathObject *)value)->data, size * sizeof(float));
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

    size = PySequence_Fast_GET_SIZE(value_fast);

    if (flag & MU_ARRAY_SPILL) {
      CLAMP_MAX(size, array_max);
    }

    if (size > array_max || size < array_min) {
      if (array_max == array_min) {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence size is %d, expected %d",
                     error_prefix,
                     size,
                     array_max);
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "%.200s: sequence size is %d, expected [%d - %d]",
                     error_prefix,
                     size,
                     array_min,
                     array_max);
      }
      Py_DECREF(value_fast);
      return -1;
    }

    size = mathutils_array_parse_fast(array, size, value_fast, error_prefix);
    Py_DECREF(value_fast);
  }

  if (size != -1) {
    if (flag & MU_ARRAY_ZERO) {
      int size_left = array_max - size;
      if (size_left) {
        memset(&array[size], 0, sizeof(float) * size_left);
      }
    }
  }

  return size;
}

/* on error, -1 is returned and no allocation is made */
int mathutils_array_parse_alloc(float **array,
                                int array_min,
                                PyObject *value,
                                const char *error_prefix)
{
  int size;

#if 1 /* approx 6x speedup for mathutils types */

  if ((size = VectorObject_Check(value) ? ((VectorObject *)value)->size : 0) ||
      (size = EulerObject_Check(value) ? 3 : 0) ||
      (size = QuaternionObject_Check(value) ? 4 : 0) ||
      (size = ColorObject_Check(value) ? 3 : 0)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }

    if (size < array_min) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s: sequence size is %d, expected > %d",
                   error_prefix,
                   size,
                   array_min);
      return -1;
    }

    *array = PyMem_Malloc(size * sizeof(float));
    memcpy(*array, ((BaseMathObject *)value)->data, size * sizeof(float));
    return size;
  }
  else
#endif
  {
    PyObject *value_fast = NULL;
    // *array = NULL;
    int ret;

    /* non list/tuple cases */
    if (!(value_fast = PySequence_Fast(value, error_prefix))) {
      /* PySequence_Fast sets the error */
      return -1;
    }

    size = PySequence_Fast_GET_SIZE(value_fast);

    if (size < array_min) {
      Py_DECREF(value_fast);
      PyErr_Format(PyExc_ValueError,
                   "%.200s: sequence size is %d, expected > %d",
                   error_prefix,
                   size,
                   array_min);
      return -1;
    }

    *array = PyMem_Malloc(size * sizeof(float));

    ret = mathutils_array_parse_fast(*array, size, value_fast, error_prefix);
    Py_DECREF(value_fast);

    if (ret == -1) {
      PyMem_Free(*array);
    }

    return ret;
  }
}

/* parse an array of vectors */
int mathutils_array_parse_alloc_v(float **array,
                                  int array_dim,
                                  PyObject *value,
                                  const char *error_prefix)
{
  PyObject *value_fast;
  const int array_dim_flag = array_dim;
  int i, size;

  /* non list/tuple cases */
  if (!(value_fast = PySequence_Fast(value, error_prefix))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  size = PySequence_Fast_GET_SIZE(value_fast);

  if (size != 0) {
    PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
    float *fp;

    array_dim &= ~MU_ARRAY_FLAGS;

    fp = *array = PyMem_Malloc(size * array_dim * sizeof(float));

    for (i = 0; i < size; i++, fp += array_dim) {
      PyObject *item = value_fast_items[i];

      if (mathutils_array_parse(fp, array_dim, array_dim_flag, item, error_prefix) == -1) {
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

int mathutils_any_to_rotmat(float rmat[3][3], PyObject *value, const char *error_prefix)
{
  if (EulerObject_Check(value)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }
    else {
      eulO_to_mat3(rmat, ((EulerObject *)value)->eul, ((EulerObject *)value)->order);
      return 0;
    }
  }
  else if (QuaternionObject_Check(value)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }
    else {
      float tquat[4];
      normalize_qt_qt(tquat, ((QuaternionObject *)value)->quat);
      quat_to_mat3(rmat, tquat);
      return 0;
    }
  }
  else if (MatrixObject_Check(value)) {
    if (BaseMath_ReadCallback((BaseMathObject *)value) == -1) {
      return -1;
    }
    else if (((MatrixObject *)value)->num_row < 3 || ((MatrixObject *)value)->num_col < 3) {
      PyErr_Format(
          PyExc_ValueError, "%.200s: matrix must have minimum 3x3 dimensions", error_prefix);
      return -1;
    }
    else {
      matrix_as_3x3(rmat, (MatrixObject *)value);
      normalize_m3(rmat);
      return 0;
    }
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "%.200s: expected a Euler, Quaternion or Matrix type, "
                 "found %.200s",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return -1;
  }
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
#define SIGNMASK(i) (-(int)(((unsigned int)(i)) >> 31))

int EXPP_FloatsAreEqual(float af, float bf, int maxDiff)
{
  /* solid, fast routine across all platforms
   * with constant time behavior */
  int ai = *(int *)(&af);
  int bi = *(int *)(&bf);
  int test = SIGNMASK(ai ^ bi);
  int diff, v1, v2;

  assert((0 == test) || (0xFFFFFFFF == test));
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
/* dynstr as python string utility functions, frees 'ds'! */
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

unsigned char Mathutils_RegisterCallback(Mathutils_Callback *cb)
{
  unsigned char i;

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

/* use macros to check for NULL */
int _BaseMathObject_ReadCallback(BaseMathObject *self)
{
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

/* BaseMathObject generic functions for all mathutils types */
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

void BaseMathObject_dealloc(BaseMathObject *self)
{
  /* only free non wrapped */
  if ((self->flag & BASE_MATH_FLAG_IS_WRAP) == 0) {
    PyMem_Free(self->data);
  }

  if (self->cb_user) {
    PyObject_GC_UnTrack(self);
    BaseMathObject_clear(self);
  }

  Py_TYPE(self)->tp_free(self);  // PyObject_DEL(self); // breaks subtypes
}

/*----------------------------MODULE INIT-------------------------*/
static struct PyMethodDef M_Mathutils_methods[] = {
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef M_Mathutils_module_def = {
    PyModuleDef_HEAD_INIT,
    "mathutils",         /* m_name */
    M_Mathutils_doc,     /* m_doc */
    0,                   /* m_size */
    M_Mathutils_methods, /* m_methods */
    NULL,                /* m_reload */
    NULL,                /* m_traverse */
    NULL,                /* m_clear */
    NULL,                /* m_free */
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
  PyModule_AddObject(mod, vector_Type.tp_name, (PyObject *)&vector_Type);
  PyModule_AddObject(mod, matrix_Type.tp_name, (PyObject *)&matrix_Type);
  PyModule_AddObject(mod, euler_Type.tp_name, (PyObject *)&euler_Type);
  PyModule_AddObject(mod, quaternion_Type.tp_name, (PyObject *)&quaternion_Type);
  PyModule_AddObject(mod, color_Type.tp_name, (PyObject *)&color_Type);

  /* submodule */
  PyModule_AddObject(mod, "geometry", (submodule = PyInit_mathutils_geometry()));
  /* XXX, python doesn't do imports with this usefully yet
   * 'from mathutils.geometry import PolyFill'
   * ...fails without this. */
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);

  PyModule_AddObject(mod, "interpolate", (submodule = PyInit_mathutils_interpolate()));
  /* XXX, python doesnt do imports with this usefully yet
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
