/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#include <algorithm>

#include <Python.h>

#include "mathutils.hh"

#include "BLI_math_base_safe.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.hh"

#ifndef MATH_STANDALONE
#  include "BLI_dynstr.h"
#endif

/**
 * Higher dimensions are supported, for many common operations
 * (dealing with vector/matrix multiply or handling as 3D locations)
 * stack memory is used with a fixed size - defined here.
 */
#define MAX_DIMENSIONS 4

/**
 * Swizzle axes get packed into a single value that is used as a closure. Each
 * axis uses SWIZZLE_BITS_PER_AXIS bits. The first bit (SWIZZLE_VALID_AXIS) is
 * used as a sentinel: if it is unset, the axis is not valid.
 */
#define SWIZZLE_BITS_PER_AXIS 3
#define SWIZZLE_VALID_AXIS 0x4
#define SWIZZLE_AXIS 0x3

static PyObject *Vector_copy(VectorObject *self);
static PyObject *Vector_deepcopy(VectorObject *self, PyObject *args);

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Row vector multiplication - (Vector * Matrix)
 * <pre>
 * [x][y][z] * [1][4][7]
 *             [2][5][8]
 *             [3][6][9]
 * </pre>
 * \note vector/matrix multiplication is not commutative.
 */
static int row_vector_multiplication(float r_vec[MAX_DIMENSIONS],
                                     VectorObject *vec,
                                     MatrixObject *mat)
{
  float vec_cpy[MAX_DIMENSIONS];
  int row, col, z = 0, vec_num = vec->vec_num;

  if (mat->row_num != vec_num) {
    if (mat->row_num == 4 && vec_num == 3) {
      vec_cpy[3] = 1.0f;
    }
    else {
      PyErr_SetString(PyExc_ValueError,
                      "vector * matrix: matrix column size "
                      "and the vector size must be the same");
      return -1;
    }
  }

  if (BaseMath_ReadCallback(vec) == -1 || BaseMath_ReadCallback(mat) == -1) {
    return -1;
  }

  memcpy(vec_cpy, vec->vec, vec_num * sizeof(float));

  r_vec[3] = 1.0f;
  /* Multiplication. */
  for (col = 0; col < mat->col_num; col++) {
    double dot = 0.0;
    for (row = 0; row < mat->row_num; row++) {
      dot += double(MATRIX_ITEM(mat, row, col) * vec_cpy[row]);
    }
    r_vec[z++] = float(dot);
  }
  return 0;
}

static PyObject *vec__apply_to_copy(PyObject *(*vec_func)(VectorObject *), VectorObject *self)
{
  PyObject *ret = Vector_copy(self);
  PyObject *ret_dummy = vec_func((VectorObject *)ret);
  if (ret_dummy) {
    Py_DECREF(ret_dummy);
    return ret;
  }
  /* error */
  Py_DECREF(ret);
  return nullptr;
}

/** \note #BaseMath_ReadCallback must be called beforehand. */
static PyObject *Vector_to_tuple_ex(VectorObject *self, int ndigits)
{
  PyObject *ret;
  int i;

  ret = PyTuple_New(self->vec_num);

  if (ndigits >= 0) {
    for (i = 0; i < self->vec_num; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round(double(self->vec[i]), ndigits)));
    }
  }
  else {
    for (i = 0; i < self->vec_num; i++) {
      PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->vec[i]));
    }
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: `__new__` / `mathutils.Vector()`
 * \{ */

/**
 * Supports 2D, 3D, and 4D vector objects both int and float values accepted.
 * Mixed float and integer values accepted. Integers are converted to float.
 */
static PyObject *Vector_vectorcall(PyObject *type,
                                   PyObject *const *args,
                                   const size_t nargsf,
                                   PyObject *kwnames)
{
  if (UNLIKELY(kwnames && PyDict_Size(kwnames))) {
    PyErr_SetString(PyExc_TypeError,
                    "Vector(): "
                    "takes no keyword args");
    return nullptr;
  }

  float *vec = nullptr;
  int vec_num = 3; /* Default to a 3D vector. */

  const size_t nargs = PyVectorcall_NARGS(nargsf);
  switch (nargs) {
    case 0: {
      vec = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));

      if (vec == nullptr) {
        PyErr_SetString(PyExc_MemoryError,
                        "Vector(): "
                        "problem allocating pointer space");
        return nullptr;
      }

      copy_vn_fl(vec, vec_num, 0.0f);
      break;
    }
    case 1: {
      if ((vec_num = mathutils_array_parse_alloc(&vec, 2, args[0], "mathutils.Vector()")) == -1) {
        return nullptr;
      }
      break;
    }
    default: {
      PyErr_Format(PyExc_TypeError,
                   "mathutils.Vector(): "
                   "takes at most 1 argument (%zd given)",
                   nargs);
      return nullptr;
    }
  }
  return Vector_CreatePyObject_alloc(vec, vec_num, (PyTypeObject *)type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Class Methods
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    C_Vector_Fill_doc,
    ".. classmethod:: Fill(size, fill=0.0, /)\n"
    "\n"
    "   Create a vector of length size with all values set to fill.\n"
    "\n"
    "   :arg size: The length of the vector to be created.\n"
    "   :type size: int\n"
    "   :arg fill: The value used to fill the vector.\n"
    "   :type fill: float\n");
static PyObject *C_Vector_Fill(PyObject *cls, PyObject *args)
{
  float *vec;
  int vec_num;
  float fill = 0.0f;

  if (!PyArg_ParseTuple(args, "i|f:Vector.Fill", &vec_num, &fill)) {
    return nullptr;
  }

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector(): invalid size");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));

  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.Fill(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  copy_vn_fl(vec, vec_num, fill);

  return Vector_CreatePyObject_alloc(vec, vec_num, (PyTypeObject *)cls);
}

PyDoc_STRVAR(
    /* Wrap. */
    C_Vector_Range_doc,
    ".. classmethod:: Range(start, stop, step=1, /)\n"
    "\n"
    "   Create a filled with a range of values.\n"
    "\n"
    "    This method can also be called with a single argument, "
    "in which case the argument is interpreted as ``stop`` and ``start`` defaults to 0.\n"
    "\n"
    "   :arg start: The start of the range used to fill the vector.\n"
    "   :type start: int\n"
    "   :arg stop: The end of the range used to fill the vector.\n"
    "   :type stop: int\n"
    "   :arg step: The step between successive values in the vector.\n"
    "   :type step: int\n");
static PyObject *C_Vector_Range(PyObject *cls, PyObject *args)
{
  float *vec;
  int stop, vec_num;
  int start = 0;
  int step = 1;

  if (!PyArg_ParseTuple(args, "i|ii:Vector.Range", &start, &stop, &step)) {
    return nullptr;
  }

  switch (PyTuple_GET_SIZE(args)) {
    case 1: {
      vec_num = start;
      start = 0;
      break;
    }
    case 2: {
      if (start >= stop) {
        PyErr_SetString(PyExc_RuntimeError,
                        "Start value is larger "
                        "than the stop value");
        return nullptr;
      }

      vec_num = stop - start;
      break;
    }
    default: {
      if (start >= stop) {
        PyErr_SetString(PyExc_RuntimeError,
                        "Start value is larger "
                        "than the stop value");
        return nullptr;
      }

      vec_num = (stop - start);

      if ((vec_num % step) != 0) {
        vec_num += step;
      }

      vec_num /= step;

      break;
    }
  }

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector(): invalid size");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));

  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.Range(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  range_vn_fl(vec, vec_num, float(start), float(step));

  return Vector_CreatePyObject_alloc(vec, vec_num, (PyTypeObject *)cls);
}

PyDoc_STRVAR(
    /* Wrap. */
    C_Vector_Linspace_doc,
    ".. classmethod:: Linspace(start, stop, size, /)\n"
    "\n"
    "   Create a vector of the specified size which is filled with linearly spaced "
    "values between start and stop values.\n"
    "\n"
    "   :arg start: The start of the range used to fill the vector.\n"
    "   :type start: int\n"
    "   :arg stop: The end of the range used to fill the vector.\n"
    "   :type stop: int\n"
    "   :arg size: The size of the vector to be created.\n"
    "   :type size: int\n");
static PyObject *C_Vector_Linspace(PyObject *cls, PyObject *args)
{
  float *vec;
  int vec_num;
  float start, end, step;

  if (!PyArg_ParseTuple(args, "ffi:Vector.Linspace", &start, &end, &vec_num)) {
    return nullptr;
  }

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector.Linspace(): invalid size");
    return nullptr;
  }

  step = (end - start) / float(vec_num - 1);

  vec = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));

  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.Linspace(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  range_vn_fl(vec, vec_num, start, step);

  return Vector_CreatePyObject_alloc(vec, vec_num, (PyTypeObject *)cls);
}

PyDoc_STRVAR(
    /* Wrap. */
    C_Vector_Repeat_doc,
    ".. classmethod:: Repeat(vector, size, /)\n"
    "\n"
    "   Create a vector by repeating the values in vector until the required size is reached.\n"
    "\n"
    "   :arg vector: The vector to draw values from.\n"
    "   :type vector: :class:`mathutils.Vector`\n"
    "   :arg size: The size of the vector to be created.\n"
    "   :type size: int\n");
static PyObject *C_Vector_Repeat(PyObject *cls, PyObject *args)
{
  float *vec;
  float *iter_vec = nullptr;
  int i, vec_num, value_num;
  PyObject *value;

  if (!PyArg_ParseTuple(args, "Oi:Vector.Repeat", &value, &vec_num)) {
    return nullptr;
  }

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector.Repeat(): invalid vec_num");
    return nullptr;
  }

  if ((value_num = mathutils_array_parse_alloc(
           &iter_vec, 2, value, "Vector.Repeat(vector, vec_num), invalid 'vector' arg")) == -1)
  {
    return nullptr;
  }

  if (iter_vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.Repeat(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));

  if (vec == nullptr) {
    PyMem_Free(iter_vec);
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.Repeat(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  i = 0;
  while (i < vec_num) {
    vec[i] = iter_vec[i % value_num];
    i++;
  }

  PyMem_Free(iter_vec);

  return Vector_CreatePyObject_alloc(vec, vec_num, (PyTypeObject *)cls);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Zero
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_zero_doc,
    ".. method:: zero()\n"
    "\n"
    "   Set all values to zero.\n");
static PyObject *Vector_zero(VectorObject *self)
{
  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return nullptr;
  }

  copy_vn_fl(self->vec, self->vec_num, 0.0f);

  if (BaseMath_WriteCallback(self) == -1) {
    return nullptr;
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Normalize
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_normalize_doc,
    ".. method:: normalize()\n"
    "\n"
    "   Normalize the vector, making the length of the vector always 1.0.\n"
    "\n"
    "   .. warning:: Normalizing a vector where all values are zero has no effect.\n"
    "\n"
    "   .. note:: Normalize works for vectors of all sizes,\n"
    "      however 4D Vectors w axis is left untouched.\n");
static PyObject *Vector_normalize(VectorObject *self)
{
  const int vec_num = (self->vec_num == 4 ? 3 : self->vec_num);
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  normalize_vn(self->vec, vec_num);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}
PyDoc_STRVAR(
    /* Wrap. */
    Vector_normalized_doc,
    ".. method:: normalized()\n"
    "\n"
    "   Return a new, normalized vector.\n"
    "\n"
    "   :return: a normalized copy of the vector\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_normalized(VectorObject *self)
{
  return vec__apply_to_copy(Vector_normalize, self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Resize
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_resize_doc,
    ".. method:: resize(size, /)\n"
    "\n"
    "   Resize the vector to have size number of elements.\n");
static PyObject *Vector_resize(VectorObject *self, PyObject *value)
{
  int vec_num;

  if (UNLIKELY(BaseMathObject_Prepare_ForResize(self, "Vector.resize()") == -1)) {
    /* An exception has been raised. */

    return nullptr;
  }

  if ((vec_num = PyC_Long_AsI32(value)) == -1) {
    PyErr_SetString(PyExc_TypeError,
                    "Vector.resize(size): "
                    "expected size argument to be an integer");
    return nullptr;
  }

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector.resize(): invalid size");
    return nullptr;
  }

  self->vec = static_cast<float *>(PyMem_Realloc(self->vec, (vec_num * sizeof(float))));
  if (self->vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.resize(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  /* If the vector has increased in length, set all new elements to 0.0f */
  if (vec_num > self->vec_num) {
    copy_vn_fl(self->vec + self->vec_num, vec_num - self->vec_num, 0.0f);
  }

  self->vec_num = vec_num;
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Vector_resized_doc,
    ".. method:: resized(size, /)\n"
    "\n"
    "   Return a resized copy of the vector with size number of elements.\n"
    "\n"
    "   :return: a new vector\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_resized(VectorObject *self, PyObject *value)
{
  int vec_num;
  float *vec;

  if ((vec_num = PyLong_AsLong(value)) == -1) {
    return nullptr;
  }

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector.resized(): invalid size");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));

  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.resized(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  copy_vn_fl(vec, vec_num, 0.0f);
  memcpy(vec, self->vec, self->vec_num * sizeof(float));

  return Vector_CreatePyObject_alloc(vec, vec_num, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    Vector_resize_2d_doc,
    ".. method:: resize_2d()\n"
    "\n"
    "   Resize the vector to 2D  (x, y).\n");
static PyObject *Vector_resize_2d(VectorObject *self)
{
  if (UNLIKELY(BaseMathObject_Prepare_ForResize(self, "Vector.resize_2d()") == -1)) {
    /* An exception has been raised. */
    return nullptr;
  }

  self->vec = static_cast<float *>(PyMem_Realloc(self->vec, sizeof(float[2])));
  if (self->vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.resize_2d(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  self->vec_num = 2;
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Vector_resize_3d_doc,
    ".. method:: resize_3d()\n"
    "\n"
    "   Resize the vector to 3D  (x, y, z).\n");
static PyObject *Vector_resize_3d(VectorObject *self)
{
  if (UNLIKELY(BaseMathObject_Prepare_ForResize(self, "Vector.resize_3d()") == -1)) {
    /* An exception has been raised. */
    return nullptr;
  }

  self->vec = static_cast<float *>(PyMem_Realloc(self->vec, sizeof(float[3])));
  if (self->vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.resize_3d(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  if (self->vec_num == 2) {
    self->vec[2] = 0.0f;
  }

  self->vec_num = 3;
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    Vector_resize_4d_doc,
    ".. method:: resize_4d()\n"
    "\n"
    "   Resize the vector to 4D (x, y, z, w).\n");
static PyObject *Vector_resize_4d(VectorObject *self)
{
  if (UNLIKELY(BaseMathObject_Prepare_ForResize(self, "Vector.resize_4d()") == -1)) {
    /* An exception has been raised. */
    return nullptr;
  }

  self->vec = static_cast<float *>(PyMem_Realloc(self->vec, sizeof(float[4])));
  if (self->vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector.resize_4d(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  if (self->vec_num == 2) {
    self->vec[2] = 0.0f;
    self->vec[3] = 1.0f;
  }
  else if (self->vec_num == 3) {
    self->vec[3] = 1.0f;
  }
  self->vec_num = 4;
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: To N-dimensions
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_to_2d_doc,
    ".. method:: to_2d()\n"
    "\n"
    "   Return a 2d copy of the vector.\n"
    "\n"
    "   :return: a new vector\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_to_2d(VectorObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return Vector_CreatePyObject(self->vec, 2, Py_TYPE(self));
}
PyDoc_STRVAR(
    /* Wrap. */
    Vector_to_3d_doc,
    ".. method:: to_3d()\n"
    "\n"
    "   Return a 3d copy of the vector.\n"
    "\n"
    "   :return: a new vector\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_to_3d(VectorObject *self)
{
  float tvec[3] = {0.0f};

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  memcpy(tvec, self->vec, sizeof(float) * std::min(self->vec_num, 3));
  return Vector_CreatePyObject(tvec, 3, Py_TYPE(self));
}
PyDoc_STRVAR(
    /* Wrap. */
    Vector_to_4d_doc,
    ".. method:: to_4d()\n"
    "\n"
    "   Return a 4d copy of the vector.\n"
    "\n"
    "   :return: a new vector\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_to_4d(VectorObject *self)
{
  float tvec[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  memcpy(tvec, self->vec, sizeof(float) * std::min(self->vec_num, 4));
  return Vector_CreatePyObject(tvec, 4, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: To Tuple
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_to_tuple_doc,
    ".. method:: to_tuple(precision=-1, /)\n"
    "\n"
    "   Return this vector as a tuple with a given precision.\n"
    "\n"
    "   :arg precision: The number to round the value to in [-1, 21].\n"
    "   :type precision: int\n"
    "   :return: the values of the vector rounded by *precision*\n"
    "   :rtype: tuple[float, ...]\n");
static PyObject *Vector_to_tuple(VectorObject *self, PyObject *args)
{
  int ndigits = -1;

  if (!PyArg_ParseTuple(args, "|i:to_tuple", &ndigits)) {
    return nullptr;
  }

  if (ndigits > 22 || ndigits < -1) {
    PyErr_SetString(PyExc_ValueError,
                    "Vector.to_tuple(precision): "
                    "precision must be between -1 and 21");
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return Vector_to_tuple_ex(self, ndigits);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: To Track Quaternion
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_to_track_quat_doc,
    ".. method:: to_track_quat(track='Z', up='Y', /)\n"
    "\n"
    "   Return a quaternion rotation from the vector and the track and up axis.\n"
    "\n"
    "   :arg track: Track axis string.\n"
    "   :type track: Literal['-', 'X', 'Y', 'Z', '-X', '-Y', '-Z']\n"
    "   :arg up: Up axis string.\n"
    "   :type up: Literal['X', 'Y', 'Z']\n"
    "   :return: rotation from the vector and the track and up axis.\n"
    "   :rtype: :class:`Quaternion`\n");
static PyObject *Vector_to_track_quat(VectorObject *self, PyObject *args)
{
  float vec[3], quat[4];
  const char *strack = nullptr;
  const char *sup = nullptr;
  short track = 2, up = 1;

  if (!PyArg_ParseTuple(args, "|ss:to_track_quat", &strack, &sup)) {
    return nullptr;
  }

  if (self->vec_num != 3) {
    PyErr_SetString(PyExc_TypeError,
                    "Vector.to_track_quat(): "
                    "only for 3D vectors");
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (strack) {
    const char *axis_err_msg = "only X, -X, Y, -Y, Z or -Z for track axis";

    if (strlen(strack) == 2) {
      if (strack[0] == '-') {
        switch (strack[1]) {
          case 'X': {
            track = 3;
            break;
          }
          case 'Y': {
            track = 4;
            break;
          }
          case 'Z': {
            track = 5;
            break;
          }
          default: {
            PyErr_SetString(PyExc_ValueError, axis_err_msg);
            return nullptr;
          }
        }
      }
      else {
        PyErr_SetString(PyExc_ValueError, axis_err_msg);
        return nullptr;
      }
    }
    else if (strlen(strack) == 1) {
      switch (strack[0]) {
        case '-':
        case 'X': {
          track = 0;
          break;
        }
        case 'Y': {
          track = 1;
          break;
        }
        case 'Z': {
          track = 2;
          break;
        }
        default: {
          PyErr_SetString(PyExc_ValueError, axis_err_msg);
          return nullptr;
        }
      }
    }
    else {
      PyErr_SetString(PyExc_ValueError, axis_err_msg);
      return nullptr;
    }
  }

  if (sup) {
    const char *axis_err_msg = "only X, Y or Z for up axis";
    if (strlen(sup) == 1) {
      switch (*sup) {
        case 'X': {
          up = 0;
          break;
        }
        case 'Y': {
          up = 1;
          break;
        }
        case 'Z': {
          up = 2;
          break;
        }
        default: {
          PyErr_SetString(PyExc_ValueError, axis_err_msg);
          return nullptr;
        }
      }
    }
    else {
      PyErr_SetString(PyExc_ValueError, axis_err_msg);
      return nullptr;
    }
  }

  if (track == up) {
    PyErr_SetString(PyExc_ValueError, "Can't have the same axis for track and up");
    return nullptr;
  }

  /* Flip vector around, since #vec_to_quat expect a vector from target to tracking object
   * and the python function expects the inverse (a vector to the target). */
  negate_v3_v3(vec, self->vec);

  vec_to_quat(quat, vec, track, up);

  return Quaternion_CreatePyObject(quat, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Orthogonal
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_orthogonal_doc,
    ".. method:: orthogonal()\n"
    "\n"
    "   Return a perpendicular vector.\n"
    "\n"
    "   :return: a new vector 90 degrees from this vector.\n"
    "   :rtype: :class:`Vector`\n"
    "\n"
    "   .. note:: the axis is undefined, only use when any orthogonal vector is acceptable.\n");
static PyObject *Vector_orthogonal(VectorObject *self)
{
  float vec[3];

  if (self->vec_num > 3) {
    PyErr_SetString(PyExc_TypeError,
                    "Vector.orthogonal(): "
                    "Vector must be 3D or 2D");
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (self->vec_num == 3) {
    ortho_v3_v3(vec, self->vec);
  }
  else {
    ortho_v2_v2(vec, self->vec);
  }

  return Vector_CreatePyObject(vec, self->vec_num, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Reflect
 *
 * `Vector.reflect(mirror)`: return a reflected vector on the mirror normal:
 * `vec - ((2 * dot(vec, mirror)) * mirror)`.
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_reflect_doc,
    ".. method:: reflect(mirror, /)\n"
    "\n"
    "   Return the reflection vector from the *mirror* argument.\n"
    "\n"
    "   :arg mirror: This vector could be a normal from the reflecting surface.\n"
    "   :type mirror: :class:`Vector`\n"
    "   :return: The reflected vector matching the size of this vector.\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_reflect(VectorObject *self, PyObject *value)
{
  int value_num;
  float mirror[3], vec[3];
  float reflect[3] = {0.0f};
  float tvec[MAX_DIMENSIONS];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if ((value_num = mathutils_array_parse(
           tvec, 2, 4, value, "Vector.reflect(other), invalid 'other' arg")) == -1)
  {
    return nullptr;
  }

  if (self->vec_num < 2 || self->vec_num > 4) {
    PyErr_SetString(PyExc_ValueError, "Vector must be 2D, 3D or 4D");
    return nullptr;
  }

  mirror[0] = tvec[0];
  mirror[1] = tvec[1];
  mirror[2] = (value_num > 2) ? tvec[2] : 0.0f;

  vec[0] = self->vec[0];
  vec[1] = self->vec[1];
  vec[2] = (value_num > 2) ? self->vec[2] : 0.0f;

  normalize_v3(mirror);
  reflect_v3_v3v3(reflect, vec, mirror);

  return Vector_CreatePyObject(reflect, self->vec_num, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Cross Product
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_cross_doc,
    ".. method:: cross(other, /)\n"
    "\n"
    "   Return the cross product of this vector and another.\n"
    "\n"
    "   :arg other: The other vector to perform the cross product with.\n"
    "   :type other: :class:`Vector`\n"
    "   :return: The cross product as a vector or a float when 2D vectors are used.\n"
    "   :rtype: :class:`Vector` | float\n"
    "\n"
    "   .. note:: both vectors must be 2D or 3D\n");
static PyObject *Vector_cross(VectorObject *self, PyObject *value)
{
  PyObject *ret;
  float tvec[3];

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (self->vec_num > 3) {
    PyErr_SetString(PyExc_ValueError, "Vector must be 2D or 3D");
    return nullptr;
  }

  if (mathutils_array_parse(
          tvec, self->vec_num, self->vec_num, value, "Vector.cross(other), invalid 'other' arg") ==
      -1)
  {
    return nullptr;
  }

  if (self->vec_num == 3) {
    ret = Vector_CreatePyObject(nullptr, 3, Py_TYPE(self));
    cross_v3_v3v3(((VectorObject *)ret)->vec, self->vec, tvec);
  }
  else {
    /* size == 2 */
    ret = PyFloat_FromDouble(cross_v2v2(self->vec, tvec));
  }
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Dot Product
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_dot_doc,
    ".. method:: dot(other, /)\n"
    "\n"
    "   Return the dot product of this vector and another.\n"
    "\n"
    "   :arg other: The other vector to perform the dot product with.\n"
    "   :type other: :class:`Vector`\n"
    "   :return: The dot product.\n"
    "   :rtype: float\n");
static PyObject *Vector_dot(VectorObject *self, PyObject *value)
{
  float *tvec;
  PyObject *ret;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse_alloc(
          &tvec, self->vec_num, value, "Vector.dot(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  ret = PyFloat_FromDouble(dot_vn_vn(self->vec, tvec, self->vec_num));
  PyMem_Free(tvec);
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Angle
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_angle_doc,
    ".. function:: angle(other, fallback=None, /)\n"
    "\n"
    "   Return the angle between two vectors.\n"
    "\n"
    "   :arg other: another vector to compare the angle with\n"
    "   :type other: :class:`Vector`\n"
    "   :arg fallback: return this when the angle can't be calculated (zero length vector),\n"
    "      (instead of raising a :exc:`ValueError`).\n"
    "   :type fallback: Any\n"
    "   :return: angle in radians or fallback when given\n"
    "   :rtype: float | Any\n");
static PyObject *Vector_angle(VectorObject *self, PyObject *args)
{
  const int vec_num = std::min(self->vec_num, 3); /* 4D angle makes no sense */
  float tvec[MAX_DIMENSIONS];
  PyObject *value;
  double dot = 0.0f, dot_self = 0.0f, dot_other = 0.0f;
  int x;
  PyObject *fallback = nullptr;

  if (!PyArg_ParseTuple(args, "O|O:angle", &value, &fallback)) {
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  /* don't use clamped size, rule of thumb is vector sizes must match,
   * even though n this case 'w' is ignored */
  if (mathutils_array_parse(
          tvec, self->vec_num, self->vec_num, value, "Vector.angle(other), invalid 'other' arg") ==
      -1)
  {
    return nullptr;
  }

  if (self->vec_num > 4) {
    PyErr_SetString(PyExc_ValueError, "Vector must be 2D, 3D or 4D");
    return nullptr;
  }

  for (x = 0; x < vec_num; x++) {
    dot_self += double(self->vec[x]) * double(self->vec[x]);
    dot_other += double(tvec[x]) * double(tvec[x]);
    dot += double(self->vec[x]) * double(tvec[x]);
  }

  if (!dot_self || !dot_other) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "Vector.angle(other): "
                    "zero length vectors have no valid angle");
    return nullptr;
  }

  return PyFloat_FromDouble(safe_acosf(dot / (sqrt(dot_self) * sqrt(dot_other))));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Angle Signed
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_angle_signed_doc,
    ".. function:: angle_signed(other, fallback=None, /)\n"
    "\n"
    "   Return the signed angle between two 2D vectors (clockwise is positive).\n"
    "\n"
    "   :arg other: another vector to compare the angle with\n"
    "   :type other: :class:`Vector`\n"
    "   :arg fallback: return this when the angle can't be calculated (zero length vector),\n"
    "      (instead of raising a :exc:`ValueError`).\n"
    "   :type fallback: Any\n"
    "   :return: angle in radians or fallback when given\n"
    "   :rtype: float | Any\n");
static PyObject *Vector_angle_signed(VectorObject *self, PyObject *args)
{
  float tvec[2];

  PyObject *value;
  PyObject *fallback = nullptr;

  if (!PyArg_ParseTuple(args, "O|O:angle_signed", &value, &fallback)) {
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(
          tvec, 2, 2, value, "Vector.angle_signed(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  if (self->vec_num != 2) {
    PyErr_SetString(PyExc_ValueError, "Vector must be 2D");
    return nullptr;
  }

  if (is_zero_v2(self->vec) || is_zero_v2(tvec)) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "Vector.angle_signed(other): "
                    "zero length vectors have no valid angle");
    return nullptr;
  }

  return PyFloat_FromDouble(angle_signed_v2v2(self->vec, tvec));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Rotation Difference
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_rotation_difference_doc,
    ".. function:: rotation_difference(other, /)\n"
    "\n"
    "   Returns a quaternion representing the rotational difference between this\n"
    "   vector and another.\n"
    "\n"
    "   :arg other: second vector.\n"
    "   :type other: :class:`Vector`\n"
    "   :return: the rotational difference between the two vectors.\n"
    "   :rtype: :class:`Quaternion`\n"
    "\n"
    "   .. note:: 2D vectors raise an :exc:`AttributeError`.\n");
static PyObject *Vector_rotation_difference(VectorObject *self, PyObject *value)
{
  float quat[4], vec_a[3], vec_b[3];

  if (self->vec_num < 3 || self->vec_num > 4) {
    PyErr_SetString(PyExc_ValueError,
                    "vec.difference(value): "
                    "expects both vectors to be size 3 or 4");
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse(
          vec_b, 3, MAX_DIMENSIONS, value, "Vector.difference(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  normalize_v3_v3(vec_a, self->vec);
  normalize_v3(vec_b);

  rotation_between_vecs_to_quat(quat, vec_a, vec_b);

  return Quaternion_CreatePyObject(quat, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Project
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_project_doc,
    ".. function:: project(other, /)\n"
    "\n"
    "   Return the projection of this vector onto the *other*.\n"
    "\n"
    "   :arg other: second vector.\n"
    "   :type other: :class:`Vector`\n"
    "   :return: the parallel projection vector\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_project(VectorObject *self, PyObject *value)
{
  const int vec_num = self->vec_num;
  float *tvec;
  double dot = 0.0f, dot2 = 0.0f;
  int x;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse_alloc(
          &tvec, vec_num, value, "Vector.project(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  /* get dot products */
  for (x = 0; x < vec_num; x++) {
    dot += double(self->vec[x] * tvec[x]);
    dot2 += double(tvec[x] * tvec[x]);
  }
  /* projection */
  dot /= dot2;
  for (x = 0; x < vec_num; x++) {
    tvec[x] *= float(dot);
  }
  return Vector_CreatePyObject_alloc(tvec, vec_num, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Linear Interpolation
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_lerp_doc,
    ".. function:: lerp(other, factor, /)\n"
    "\n"
    "   Returns the interpolation of two vectors.\n"
    "\n"
    "   :arg other: value to interpolate with.\n"
    "   :type other: :class:`Vector`\n"
    "   :arg factor: The interpolation value in [0.0, 1.0].\n"
    "   :type factor: float\n"
    "   :return: The interpolated vector.\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_lerp(VectorObject *self, PyObject *args)
{
  const int vec_num = self->vec_num;
  PyObject *value = nullptr;
  float fac;
  float *tvec;

  if (!PyArg_ParseTuple(args, "Of:lerp", &value, &fac)) {
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (mathutils_array_parse_alloc(
          &tvec, vec_num, value, "Vector.lerp(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  interp_vn_vn(tvec, self->vec, 1.0f - fac, vec_num);

  return Vector_CreatePyObject_alloc(tvec, vec_num, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Spherical Interpolation
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_slerp_doc,
    ".. function:: slerp(other, factor, fallback=None, /)\n"
    "\n"
    "   Returns the interpolation of two non-zero vectors (spherical coordinates).\n"
    "\n"
    "   :arg other: value to interpolate with.\n"
    "   :type other: :class:`Vector`\n"
    "   :arg factor: The interpolation value typically in [0.0, 1.0].\n"
    "   :type factor: float\n"
    "   :arg fallback: return this when the vector can't be calculated (zero length "
    "vector or direct opposites),\n"
    "      (instead of raising a :exc:`ValueError`).\n"
    "   :type fallback: Any\n"
    "   :return: The interpolated vector.\n"
    "   :rtype: :class:`Vector`\n");
static PyObject *Vector_slerp(VectorObject *self, PyObject *args)
{
  const int vec_num = self->vec_num;
  PyObject *value = nullptr;
  float fac, cosom, w[2];
  float self_vec[3], other_vec[3], ret_vec[3];
  float self_len_sq, other_len_sq;
  int x;
  PyObject *fallback = nullptr;

  if (!PyArg_ParseTuple(args, "Of|O:slerp", &value, &fac, &fallback)) {
    return nullptr;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  if (self->vec_num > 3) {
    PyErr_SetString(PyExc_ValueError, "Vector must be 2D or 3D");
    return nullptr;
  }

  if (mathutils_array_parse(
          other_vec, vec_num, vec_num, value, "Vector.slerp(other), invalid 'other' arg") == -1)
  {
    return nullptr;
  }

  self_len_sq = normalize_vn_vn(self_vec, self->vec, vec_num);
  other_len_sq = normalize_vn(other_vec, vec_num);

  /* use fallbacks for zero length vectors */
  if (UNLIKELY((self_len_sq < FLT_EPSILON) || (other_len_sq < FLT_EPSILON))) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "Vector.slerp(): "
                    "zero length vectors unsupported");
    return nullptr;
  }

  /* We have sane state, execute slerp */
  cosom = float(dot_vn_vn(self_vec, other_vec, vec_num));

  /* direct opposite, can't slerp */
  if (UNLIKELY(cosom < (-1.0f + FLT_EPSILON))) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "Vector.slerp(): "
                    "opposite vectors unsupported");
    return nullptr;
  }

  interp_dot_slerp(fac, cosom, w);

  for (x = 0; x < vec_num; x++) {
    ret_vec[x] = (w[0] * self_vec[x]) + (w[1] * other_vec[x]);
  }

  return Vector_CreatePyObject(ret_vec, vec_num, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Rotate
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_rotate_doc,
    ".. function:: rotate(other, /)\n"
    "\n"
    "   Rotate the vector by a rotation value.\n"
    "\n"
    "   .. note:: 2D vectors are a special case that can only be rotated by a 2x2 matrix.\n"
    "\n"
    "   :arg other: rotation component of mathutils value\n"
    "   :type other: :class:`Euler` | :class:`Quaternion` | :class:`Matrix`\n");
static PyObject *Vector_rotate(VectorObject *self, PyObject *value)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return nullptr;
  }

  if (self->vec_num == 2) {
    /* Special case for 2D Vector with 2x2 matrix, so we avoid resizing it to a 3x3. */
    float other_rmat[2][2];
    MatrixObject *pymat;
    if (!Matrix_Parse2x2(value, &pymat)) {
      return nullptr;
    }
    normalize_m2_m2(other_rmat, (const float (*)[2])pymat->matrix);
    /* Equivalent to a rotation along the Z axis. */
    mul_m2_v2(other_rmat, self->vec);
  }
  else {
    float other_rmat[3][3];

    if (mathutils_any_to_rotmat(other_rmat, value, "Vector.rotate(value)") == -1) {
      return nullptr;
    }

    mul_m3_v3(other_rmat, self->vec);
  }

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Negate
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_negate_doc,
    ".. method:: negate()\n"
    "\n"
    "   Set all values to their negative.\n");
static PyObject *Vector_negate(VectorObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  negate_vn(self->vec, self->vec_num);

  (void)BaseMath_WriteCallback(self); /* already checked for error */
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Methods: Copy/Deep-Copy
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_copy_doc,
    ".. function:: copy()\n"
    "\n"
    "   Returns a copy of this vector.\n"
    "\n"
    "   :return: A copy of the vector.\n"
    "   :rtype: :class:`Vector`\n"
    "\n"
    "   .. note:: use this to get a copy of a wrapped vector with\n"
    "      no reference to the original data.\n");
static PyObject *Vector_copy(VectorObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return Vector_CreatePyObject(self->vec, self->vec_num, Py_TYPE(self));
}
static PyObject *Vector_deepcopy(VectorObject *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return nullptr;
  }
  return Vector_copy(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: `__repr__` & `__str__`
 * \{ */

static PyObject *Vector_repr(VectorObject *self)
{
  PyObject *ret, *tuple;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  tuple = Vector_to_tuple_ex(self, -1);
  ret = PyUnicode_FromFormat("Vector(%R)", tuple);
  Py_DECREF(tuple);
  return ret;
}

#ifndef MATH_STANDALONE
static PyObject *Vector_str(VectorObject *self)
{
  int i;

  DynStr *ds;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  ds = BLI_dynstr_new();

  BLI_dynstr_append(ds, "<Vector (");

  for (i = 0; i < self->vec_num; i++) {
    BLI_dynstr_appendf(ds, i ? ", %.4f" : "%.4f", self->vec[i]);
  }

  BLI_dynstr_append(ds, ")>");

  return mathutils_dynstr_to_py(ds); /* frees ds */
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Buffer Protocol
 * \{ */

static int Vector_getbuffer(PyObject *obj, Py_buffer *view, int flags)
{
  VectorObject *self = (VectorObject *)obj;
  if (UNLIKELY(BaseMath_Prepare_ForBufferAccess(self, view, flags) == -1)) {
    return -1;
  }
  if (UNLIKELY(BaseMath_ReadCallback(self) == -1)) {
    return -1;
  }

  memset(view, 0, sizeof(*view));

  view->obj = (PyObject *)self;
  view->buf = (void *)self->vec;
  view->len = Py_ssize_t(self->vec_num * sizeof(float));
  view->itemsize = sizeof(float);
  view->ndim = 1;
  if ((flags & PyBUF_WRITABLE) == 0) {
    view->readonly = 1;
  }
  if (flags & PyBUF_FORMAT) {
    view->format = (char *)"f";
  }

  self->flag |= BASE_MATH_FLAG_HAS_BUFFER_VIEW;

  Py_INCREF(self);
  return 0;
}

static void Vector_releasebuffer(PyObject * /*exporter*/, Py_buffer *view)
{
  VectorObject *self = (VectorObject *)view->obj;
  self->flag &= ~BASE_MATH_FLAG_HAS_BUFFER_VIEW;

  if (view->readonly == 0) {
    if (UNLIKELY(BaseMath_WriteCallback(self) == -1)) {
      PyErr_Print();
    }
  }
}

static PyBufferProcs Vector_as_buffer = {
    (getbufferproc)Vector_getbuffer,
    (releasebufferproc)Vector_releasebuffer,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Rich Compare
 * \{ */

static PyObject *Vector_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
  VectorObject *vecA = nullptr, *vecB = nullptr;
  int result = 0;
  const double epsilon = 0.000001f;
  double lenA, lenB;

  if (!VectorObject_Check(objectA) || !VectorObject_Check(objectB)) {
    if (comparison_type == Py_NE) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
  }
  vecA = (VectorObject *)objectA;
  vecB = (VectorObject *)objectB;

  if (BaseMath_ReadCallback(vecA) == -1 || BaseMath_ReadCallback(vecB) == -1) {
    return nullptr;
  }

  if (vecA->vec_num != vecB->vec_num) {
    if (comparison_type == Py_NE) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
  }

  switch (comparison_type) {
    case Py_LT: {
      lenA = len_squared_vn(vecA->vec, vecA->vec_num);
      lenB = len_squared_vn(vecB->vec, vecB->vec_num);
      if (lenA < lenB) {
        result = 1;
      }
      break;
    }
    case Py_LE: {
      lenA = len_squared_vn(vecA->vec, vecA->vec_num);
      lenB = len_squared_vn(vecB->vec, vecB->vec_num);
      if (lenA < lenB) {
        result = 1;
      }
      else {
        result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
      }
      break;
    }
    case Py_EQ: {
      result = EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->vec_num, 1);
      break;
    }
    case Py_NE: {
      result = !EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->vec_num, 1);
      break;
    }
    case Py_GT: {
      lenA = len_squared_vn(vecA->vec, vecA->vec_num);
      lenB = len_squared_vn(vecB->vec, vecB->vec_num);
      if (lenA > lenB) {
        result = 1;
      }
      break;
    }
    case Py_GE: {
      lenA = len_squared_vn(vecA->vec, vecA->vec_num);
      lenB = len_squared_vn(vecB->vec, vecB->vec_num);
      if (lenA > lenB) {
        result = 1;
      }
      else {
        result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
      }
      break;
    }
    default: {
      printf("The result of the comparison could not be evaluated");
      break;
    }
  }
  if (result == 1) {
    Py_RETURN_TRUE;
  }

  Py_RETURN_FALSE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Hash (`__hash__`)
 * \{ */

static Py_hash_t Vector_hash(VectorObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  if (BaseMathObject_Prepare_ForHash(self) == -1) {
    return -1;
  }

  return mathutils_array_hash(self->vec, self->vec_num);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Sequence & Mapping Protocols Implementation
 * \{ */

/** Sequence length: `len(object)`. */
static Py_ssize_t Vector_len(VectorObject *self)
{
  return self->vec_num;
}

static PyObject *vector_item_internal(VectorObject *self, int i, const bool is_attr)
{
  if (i < 0) {
    i = self->vec_num - i;
  }

  if (i < 0 || i >= self->vec_num) {
    if (is_attr) {
      PyErr_Format(PyExc_AttributeError,
                   "Vector.%c: unavailable on %dd vector",
                   *(((const char *)"xyzw") + i),
                   self->vec_num);
    }
    else {
      PyErr_SetString(PyExc_IndexError, "vector[index]: out of range");
    }
    return nullptr;
  }

  if (BaseMath_ReadIndexCallback(self, i) == -1) {
    return nullptr;
  }

  return PyFloat_FromDouble(self->vec[i]);
}

/** Sequence accessor (get): `x = object[i]`. */
static PyObject *Vector_item(VectorObject *self, Py_ssize_t i)
{
  return vector_item_internal(self, i, false);
}

static int vector_ass_item_internal(VectorObject *self, int i, PyObject *value, const bool is_attr)
{
  float scalar;

  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return -1;
  }

  if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) {
    /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError,
                    "vector[index] = x: "
                    "assigned value not a number");
    return -1;
  }

  if (i < 0) {
    i = self->vec_num - i;
  }

  if (i < 0 || i >= self->vec_num) {
    if (is_attr) {
      PyErr_Format(PyExc_AttributeError,
                   "Vector.%c = x: unavailable on %dd vector",
                   *(((const char *)"xyzw") + i),
                   self->vec_num);
    }
    else {
      PyErr_SetString(PyExc_IndexError,
                      "vector[index] = x: "
                      "assignment index out of range");
    }
    return -1;
  }
  self->vec[i] = scalar;

  if (BaseMath_WriteIndexCallback(self, i) == -1) {
    return -1;
  }
  return 0;
}

/** Sequence accessor (set): `object[i] = x`. */
static int Vector_ass_item(VectorObject *self, Py_ssize_t i, PyObject *value)
{
  return vector_ass_item_internal(self, i, value, false);
}

/** Sequence slice accessor (get): `x = object[i:j]`. */
static PyObject *Vector_slice(VectorObject *self, int begin, int end)
{
  PyObject *tuple;
  int count;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  CLAMP(begin, 0, self->vec_num);
  if (end < 0) {
    end = self->vec_num + end + 1;
  }
  CLAMP(end, 0, self->vec_num);
  begin = std::min(begin, end);

  tuple = PyTuple_New(end - begin);
  for (count = begin; count < end; count++) {
    PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->vec[count]));
  }

  return tuple;
}

/** Sequence slice accessor (set): `object[i:j] = x`. */
static int Vector_ass_slice(VectorObject *self, int begin, int end, PyObject *seq)
{
  int vec_num = 0;
  float *vec = nullptr;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  CLAMP(begin, 0, self->vec_num);
  CLAMP(end, 0, self->vec_num);
  begin = std::min(begin, end);

  vec_num = (end - begin);
  if (mathutils_array_parse_alloc(&vec, vec_num, seq, "vector[begin:end] = [...]") == -1) {
    return -1;
  }

  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "vec[:] = seq: "
                    "problem allocating pointer space");
    return -1;
  }

  /* Parsed well - now set in vector. */
  memcpy(self->vec + begin, vec, vec_num * sizeof(float));

  PyMem_Free(vec);

  if (BaseMath_WriteCallback(self) == -1) {
    return -1;
  }

  return 0;
}

/** Sequence generic subscript (get): `x = object[...]`. */
static PyObject *Vector_subscript(VectorObject *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    if (i < 0) {
      i += self->vec_num;
    }
    return Vector_item(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->vec_num, &start, &stop, &step, &slicelength) < 0) {
      return nullptr;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return Vector_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with vectors");
    return nullptr;
  }

  PyErr_Format(
      PyExc_TypeError, "vector indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return nullptr;
}

/** Sequence generic subscript (set): `object[...] = x`. */
static int Vector_ass_subscript(VectorObject *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += self->vec_num;
    }
    return Vector_ass_item(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->vec_num, &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return Vector_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with vectors");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "vector indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Numeric Protocol Implementation
 * \{ */

/** Addition: `object + object`. */
static PyObject *Vector_add(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;
  float *vec = nullptr;

  if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
    PyErr_Format(PyExc_AttributeError,
                 "Vector addition: (%s + %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return nullptr;
  }
  vec1 = (VectorObject *)v1;
  vec2 = (VectorObject *)v2;

  if (BaseMath_ReadCallback(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1) {
    return nullptr;
  }

  /* VECTOR + VECTOR. */
  if (vec1->vec_num != vec2->vec_num) {
    PyErr_SetString(PyExc_AttributeError,
                    "Vector addition: "
                    "vectors must have the same dimensions for this operation");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec1->vec_num * sizeof(float)));
  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  add_vn_vnvn(vec, vec1->vec, vec2->vec, vec1->vec_num);

  return Vector_CreatePyObject_alloc(vec, vec1->vec_num, Py_TYPE(v1));
}

/** Addition in-place: `object += object`. */
static PyObject *Vector_iadd(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;

  if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
    PyErr_Format(PyExc_AttributeError,
                 "Vector addition: (%s += %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return nullptr;
  }
  vec1 = (VectorObject *)v1;
  vec2 = (VectorObject *)v2;

  if (vec1->vec_num != vec2->vec_num) {
    PyErr_SetString(PyExc_AttributeError,
                    "Vector addition: "
                    "vectors must have the same dimensions for this operation");
    return nullptr;
  }

  if (BaseMath_ReadCallback_ForWrite(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1) {
    return nullptr;
  }

  add_vn_vn(vec1->vec, vec2->vec, vec1->vec_num);

  (void)BaseMath_WriteCallback(vec1);
  Py_INCREF(v1);
  return v1;
}

/** Subtraction: `object - object`. */
static PyObject *Vector_sub(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;
  float *vec;

  if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
    PyErr_Format(PyExc_AttributeError,
                 "Vector subtraction: (%s - %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return nullptr;
  }
  vec1 = (VectorObject *)v1;
  vec2 = (VectorObject *)v2;

  if (BaseMath_ReadCallback(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1) {
    return nullptr;
  }

  if (vec1->vec_num != vec2->vec_num) {
    PyErr_SetString(PyExc_AttributeError,
                    "Vector subtraction: "
                    "vectors must have the same dimensions for this operation");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec1->vec_num * sizeof(float)));
  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector(): "
                    "problem allocating pointer space");
    return nullptr;
  }

  sub_vn_vnvn(vec, vec1->vec, vec2->vec, vec1->vec_num);

  return Vector_CreatePyObject_alloc(vec, vec1->vec_num, Py_TYPE(v1));
}

/** Subtraction in-place: `object -= object`. */
static PyObject *Vector_isub(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;

  if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
    PyErr_Format(PyExc_AttributeError,
                 "Vector subtraction: (%s -= %s) "
                 "invalid type for this operation",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return nullptr;
  }
  vec1 = (VectorObject *)v1;
  vec2 = (VectorObject *)v2;

  if (vec1->vec_num != vec2->vec_num) {
    PyErr_SetString(PyExc_AttributeError,
                    "Vector subtraction: "
                    "vectors must have the same dimensions for this operation");
    return nullptr;
  }

  if (BaseMath_ReadCallback_ForWrite(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1) {
    return nullptr;
  }

  sub_vn_vn(vec1->vec, vec2->vec, vec1->vec_num);

  (void)BaseMath_WriteCallback(vec1);
  Py_INCREF(v1);
  return v1;
}

/* Multiply internal implementation `object * object`, `object *= object`. */

int column_vector_multiplication(float r_vec[MAX_DIMENSIONS], VectorObject *vec, MatrixObject *mat)
{
  float vec_cpy[MAX_DIMENSIONS];
  int row, col, z = 0;

  if (mat->col_num != vec->vec_num) {
    if (mat->col_num == 4 && vec->vec_num == 3) {
      vec_cpy[3] = 1.0f;
    }
    else {
      PyErr_SetString(PyExc_ValueError,
                      "matrix * vector: "
                      "len(matrix.col) and len(vector) must be the same, "
                      "except for 4x4 matrix * 3D vector.");
      return -1;
    }
  }

  memcpy(vec_cpy, vec->vec, vec->vec_num * sizeof(float));

  r_vec[3] = 1.0f;

  for (row = 0; row < mat->row_num; row++) {
    double dot = 0.0f;
    for (col = 0; col < mat->col_num; col++) {
      dot += double(MATRIX_ITEM(mat, row, col) * vec_cpy[col]);
    }
    r_vec[z++] = float(dot);
  }

  return 0;
}

static PyObject *vector_mul_float(VectorObject *vec, const float scalar)
{
  float *tvec = static_cast<float *>(PyMem_Malloc(vec->vec_num * sizeof(float)));
  if (tvec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "vec * float: "
                    "problem allocating pointer space");
    return nullptr;
  }

  mul_vn_vn_fl(tvec, vec->vec, vec->vec_num, scalar);
  return Vector_CreatePyObject_alloc(tvec, vec->vec_num, Py_TYPE(vec));
}

static PyObject *vector_mul_vec(VectorObject *vec1, VectorObject *vec2)
{
  float *tvec = static_cast<float *>(PyMem_Malloc(vec1->vec_num * sizeof(float)));
  if (tvec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "vec * vec: "
                    "problem allocating pointer space");
    return nullptr;
  }

  mul_vn_vnvn(tvec, vec1->vec, vec2->vec, vec1->vec_num);
  return Vector_CreatePyObject_alloc(tvec, vec1->vec_num, Py_TYPE(vec1));
}

/** Multiplication (element-wise or scalar): `object * object`. */
static PyObject *Vector_mul(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;
  float scalar;

  if (VectorObject_Check(v1)) {
    vec1 = (VectorObject *)v1;
    if (BaseMath_ReadCallback(vec1) == -1) {
      return nullptr;
    }
  }
  if (VectorObject_Check(v2)) {
    vec2 = (VectorObject *)v2;
    if (BaseMath_ReadCallback(vec2) == -1) {
      return nullptr;
    }
  }

  /* Intentionally don't support (Quaternion) here, uses reverse order instead. */

  /* make sure v1 is always the vector */
  if (vec1 && vec2) {
    if (vec1->vec_num != vec2->vec_num) {
      PyErr_SetString(PyExc_ValueError,
                      "Vector multiplication: "
                      "vectors must have the same dimensions for this operation");
      return nullptr;
    }

    /* element-wise product */
    return vector_mul_vec(vec1, vec2);
  }
  if (vec1) {
    if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* VEC * FLOAT */
      return vector_mul_float(vec1, scalar);
    }
  }
  else if (vec2) {
    if (((scalar = PyFloat_AsDouble(v1)) == -1.0f && PyErr_Occurred()) == 0) { /* FLOAT * VEC */
      return vector_mul_float(vec2, scalar);
    }
  }

  PyErr_Format(PyExc_TypeError,
               "Element-wise multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(v1)->tp_name,
               Py_TYPE(v2)->tp_name);
  return nullptr;
}

/** Multiplication in-place (element-wise or scalar): `object *= object`. */
static PyObject *Vector_imul(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;
  float scalar;

  if (VectorObject_Check(v1)) {
    vec1 = (VectorObject *)v1;
    if (BaseMath_ReadCallback(vec1) == -1) {
      return nullptr;
    }
  }
  if (VectorObject_Check(v2)) {
    vec2 = (VectorObject *)v2;
    if (BaseMath_ReadCallback(vec2) == -1) {
      return nullptr;
    }
  }

  if (BaseMath_ReadCallback_ForWrite(vec1) == -1) {
    return nullptr;
  }

  /* Intentionally don't support (Quaternion, Matrix) here, uses reverse order instead. */

  if (vec1 && vec2) {
    if (vec1->vec_num != vec2->vec_num) {
      PyErr_SetString(PyExc_ValueError,
                      "Vector multiplication: "
                      "vectors must have the same dimensions for this operation");
      return nullptr;
    }

    /* Element-wise product in-place. */
    mul_vn_vn(vec1->vec, vec2->vec, vec1->vec_num);
  }
  else if (vec1 && (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0)) {
    /* VEC *= FLOAT */
    mul_vn_fl(vec1->vec, vec1->vec_num, scalar);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "In place element-wise multiplication: "
                 "not supported between '%.200s' and '%.200s' types",
                 Py_TYPE(v1)->tp_name,
                 Py_TYPE(v2)->tp_name);
    return nullptr;
  }

  (void)BaseMath_WriteCallback(vec1);
  Py_INCREF(v1);
  return v1;
}

/** Multiplication (matrix multiply): `object @ object`. */
static PyObject *Vector_matmul(PyObject *v1, PyObject *v2)
{
  VectorObject *vec1 = nullptr, *vec2 = nullptr;
  int vec_num;

  if (VectorObject_Check(v1)) {
    vec1 = (VectorObject *)v1;
    if (BaseMath_ReadCallback(vec1) == -1) {
      return nullptr;
    }
  }
  if (VectorObject_Check(v2)) {
    vec2 = (VectorObject *)v2;
    if (BaseMath_ReadCallback(vec2) == -1) {
      return nullptr;
    }
  }

  /* Intentionally don't support (Quaternion) here, uses reverse order instead. */

  /* make sure v1 is always the vector */
  if (vec1 && vec2) {
    if (vec1->vec_num != vec2->vec_num) {
      PyErr_SetString(PyExc_ValueError,
                      "Vector multiplication: "
                      "vectors must have the same dimensions for this operation");
      return nullptr;
    }

    /* Dot product. */
    return PyFloat_FromDouble(dot_vn_vn(vec1->vec, vec2->vec, vec1->vec_num));
  }
  if (vec1) {
    if (MatrixObject_Check(v2)) {
      /* VEC @ MATRIX */
      float tvec[MAX_DIMENSIONS];

      if (BaseMath_ReadCallback((MatrixObject *)v2) == -1) {
        return nullptr;
      }
      if (row_vector_multiplication(tvec, vec1, (MatrixObject *)v2) == -1) {
        return nullptr;
      }

      if (((MatrixObject *)v2)->row_num == 4 && vec1->vec_num == 3) {
        vec_num = 3;
      }
      else {
        vec_num = ((MatrixObject *)v2)->col_num;
      }

      return Vector_CreatePyObject(tvec, vec_num, Py_TYPE(vec1));
    }
  }

  PyErr_Format(PyExc_TypeError,
               "Vector multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(v1)->tp_name,
               Py_TYPE(v2)->tp_name);
  return nullptr;
}

/** Multiplication in-place (matrix multiply): `object @= object`. */
static PyObject *Vector_imatmul(PyObject *v1, PyObject *v2)
{
  PyErr_Format(PyExc_TypeError,
               "In place vector multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(v1)->tp_name,
               Py_TYPE(v2)->tp_name);
  return nullptr;
}

/** Division: `object / object`. */
static PyObject *Vector_div(PyObject *v1, PyObject *v2)
{
  float *vec = nullptr, scalar;
  VectorObject *vec1 = nullptr;

  if (!VectorObject_Check(v1)) { /* not a vector */
    PyErr_SetString(PyExc_TypeError,
                    "Vector division: "
                    "Vector must be divided by a float");
    return nullptr;
  }
  vec1 = (VectorObject *)v1; /* vector */

  if (BaseMath_ReadCallback(vec1) == -1) {
    return nullptr;
  }

  if ((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) {
    /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError,
                    "Vector division: "
                    "Vector must be divided by a float");
    return nullptr;
  }

  if (scalar == 0.0f) {
    PyErr_SetString(PyExc_ZeroDivisionError,
                    "Vector division: "
                    "divide by zero error");
    return nullptr;
  }

  vec = static_cast<float *>(PyMem_Malloc(vec1->vec_num * sizeof(float)));

  if (vec == nullptr) {
    PyErr_SetString(PyExc_MemoryError,
                    "vec / value: "
                    "problem allocating pointer space");
    return nullptr;
  }

  mul_vn_vn_fl(vec, vec1->vec, vec1->vec_num, 1.0f / scalar);

  return Vector_CreatePyObject_alloc(vec, vec1->vec_num, Py_TYPE(v1));
}

/** Division in-place: `object /= object`. */
static PyObject *Vector_idiv(PyObject *v1, PyObject *v2)
{
  float scalar;
  VectorObject *vec1 = (VectorObject *)v1;

  if (BaseMath_ReadCallback_ForWrite(vec1) == -1) {
    return nullptr;
  }

  if ((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) {
    /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError,
                    "Vector division: "
                    "Vector must be divided by a float");
    return nullptr;
  }

  if (scalar == 0.0f) {
    PyErr_SetString(PyExc_ZeroDivisionError,
                    "Vector division: "
                    "divide by zero error");
    return nullptr;
  }

  mul_vn_fl(vec1->vec, vec1->vec_num, 1.0f / scalar);

  (void)BaseMath_WriteCallback(vec1);

  Py_INCREF(v1);
  return v1;
}

/** Negative (returns the negative of this object): `-object`. */
static PyObject *Vector_neg(VectorObject *self)
{
  float *tvec;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  tvec = static_cast<float *>(PyMem_Malloc(self->vec_num * sizeof(float)));
  negate_vn_vn(tvec, self->vec, self->vec_num);
  return Vector_CreatePyObject_alloc(tvec, self->vec_num, Py_TYPE(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Protocol Declarations
 * \{ */

static PySequenceMethods Vector_SeqMethods = {
    /*sq_length*/ (lenfunc)Vector_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)Vector_item,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ (ssizeobjargproc)Vector_ass_item,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods Vector_AsMapping = {
    /*mp_length*/ (lenfunc)Vector_len,
    /*mp_subscript*/ (binaryfunc)Vector_subscript,
    /*mp_ass_subscript*/ (objobjargproc)Vector_ass_subscript,
};

static PyNumberMethods Vector_NumMethods = {
    /*nb_add*/ (binaryfunc)Vector_add,
    /*nb_subtract*/ (binaryfunc)Vector_sub,
    /*nb_multiply*/ (binaryfunc)Vector_mul,
    /*nb_remainder*/ nullptr,
    /*nb_divmod*/ nullptr,
    /*nb_power*/ nullptr,
    /*nb_negative*/ (unaryfunc)Vector_neg,
    /*nb_positive*/ (unaryfunc)Vector_copy,
    /*nb_absolute*/ nullptr,
    /*nb_bool*/ nullptr,
    /*nb_invert*/ nullptr,
    /*nb_lshift*/ nullptr,
    /*nb_rshift*/ nullptr,
    /*nb_and*/ nullptr,
    /*nb_xor*/ nullptr,
    /*nb_or*/ nullptr,
    /*nb_int*/ nullptr,
    /*nb_reserved*/ nullptr,
    /*nb_float*/ nullptr,
    /*nb_inplace_add*/ Vector_iadd,
    /*nb_inplace_subtract*/ Vector_isub,
    /*nb_inplace_multiply*/ Vector_imul,
    /*nb_inplace_remainder*/ nullptr,
    /*nb_inplace_power*/ nullptr,
    /*nb_inplace_lshift*/ nullptr,
    /*nb_inplace_rshift*/ nullptr,
    /*nb_inplace_and*/ nullptr,
    /*nb_inplace_xor*/ nullptr,
    /*nb_inplace_or*/ nullptr,
    /*nb_floor_divide*/ nullptr,
    /*nb_true_divide*/ Vector_div,
    /*nb_inplace_floor_divide*/ nullptr,
    /*nb_inplace_true_divide*/ Vector_idiv,
    /*nb_index*/ nullptr,
    /*nb_matrix_multiply*/ (binaryfunc)Vector_matmul,
    /*nb_inplace_matrix_multiply*/ (binaryfunc)Vector_imatmul,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Get/Set Item Implementation
 * \{ */

/* Vector axis: `vector.x/y/z/w`. */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_axis_x_doc,
    "Vector X axis.\n"
    "\n"
    ":type: float\n");
PyDoc_STRVAR(
    /* Wrap. */
    Vector_axis_y_doc,
    "Vector Y axis.\n"
    "\n"
    ":type: float\n");
PyDoc_STRVAR(
    /* Wrap. */
    Vector_axis_z_doc,
    "Vector Z axis (3D Vectors only).\n"
    "\n"
    ":type: float\n");
PyDoc_STRVAR(
    /* Wrap. */
    Vector_axis_w_doc,
    "Vector W axis (4D Vectors only).\n"
    "\n"
    ":type: float\n");

static PyObject *Vector_axis_get(VectorObject *self, void *type)
{
  return vector_item_internal(self, POINTER_AS_INT(type), true);
}

static int Vector_axis_set(VectorObject *self, PyObject *value, void *type)
{
  return vector_ass_item_internal(self, POINTER_AS_INT(type), value, true);
}

/* `Vector.length`. */

PyDoc_STRVAR(
    /* Wrap. */
    Vector_length_doc,
    "Vector Length.\n"
    "\n"
    ":type: float\n");
static PyObject *Vector_length_get(VectorObject *self, void * /*closure*/)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return PyFloat_FromDouble(sqrt(dot_vn_vn(self->vec, self->vec, self->vec_num)));
}

static int Vector_length_set(VectorObject *self, PyObject *value)
{
  double dot = 0.0f, param;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  if ((param = PyFloat_AsDouble(value)) == -1.0 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "length must be set to a number");
    return -1;
  }

  if (param < 0.0) {
    PyErr_SetString(PyExc_ValueError, "cannot set a vectors length to a negative value");
    return -1;
  }
  if (param == 0.0) {
    copy_vn_fl(self->vec, self->vec_num, 0.0f);
    return 0;
  }

  dot = dot_vn_vn(self->vec, self->vec, self->vec_num);

  if (!dot) {
    /* can't sqrt zero */
    return 0;
  }

  dot = sqrt(dot);

  if (dot == param) {
    return 0;
  }

  dot = dot / param;

  mul_vn_fl(self->vec, self->vec_num, 1.0 / dot);

  (void)BaseMath_WriteCallback(self); /* checked already */

  return 0;
}

/* `Vector.length_squared`. */
PyDoc_STRVAR(
    /* Wrap. */
    Vector_length_squared_doc,
    "Vector length squared (v.dot(v)).\n"
    "\n"
    ":type: float\n");
static PyObject *Vector_length_squared_get(VectorObject *self, void * /*closure*/)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  return PyFloat_FromDouble(dot_vn_vn(self->vec, self->vec, self->vec_num));
}

/* `Vector.xyzw`, etc.. */
PyDoc_STRVAR(
    /* Wrap. */
    Vector_swizzle_doc,
    ":type: :class:`Vector`");
/**
 * Python script used to make swizzle array:
 *
 * \code{.py}
 * SWIZZLE_BITS_PER_AXIS = 3
 * SWIZZLE_VALID_AXIS = 0x4
 *
 * axis_dict = {}
 * axis_pos = {"x": 0, "y": 1, "z": 2, "w": 3}
 * axis_chars = "xyzw"
 * while len(axis_chars) >= 2:
 *     for axis_0 in axis_chars:
 *         axis_0_pos = axis_pos[axis_0]
 *         for axis_1 in axis_chars:
 *             axis_1_pos = axis_pos[axis_1]
 *             axis_dict[axis_0 + axis_1] = (
 *                 "(({:d} | SWIZZLE_VALID_AXIS) | "
 *                 "(({:d} | SWIZZLE_VALID_AXIS) << SWIZZLE_BITS_PER_AXIS))"
 *             ).format(axis_0_pos, axis_1_pos)
 *             if len(axis_chars) <= 2:
 *                 continue
 *             for axis_2 in axis_chars:
 *                 axis_2_pos = axis_pos[axis_2]
 *                 axis_dict[axis_0 + axis_1 + axis_2] = (
 *                     "(({:d} | SWIZZLE_VALID_AXIS) | "
 *                     "(({:d} | SWIZZLE_VALID_AXIS) << SWIZZLE_BITS_PER_AXIS) | "
 *                     "(({:d} | SWIZZLE_VALID_AXIS) << (SWIZZLE_BITS_PER_AXIS * 2)))"
 *                 ).format(axis_0_pos, axis_1_pos, axis_2_pos)
 *                 if len(axis_chars) <= 3:
 *                     continue
 *                 for axis_3 in axis_chars:
 *                     axis_3_pos = axis_pos[axis_3]
 *                     axis_dict[axis_0 + axis_1 + axis_2 + axis_3] = (
 *                         "(({:d} | SWIZZLE_VALID_AXIS) | "
 *                         "(({:d} | SWIZZLE_VALID_AXIS) << SWIZZLE_BITS_PER_AXIS) | "
 *                         "(({:d} | SWIZZLE_VALID_AXIS) << (SWIZZLE_BITS_PER_AXIS * 2)) | "
 *                         "(({:d} | SWIZZLE_VALID_AXIS) << (SWIZZLE_BITS_PER_AXIS * 3)))"
 *                     ).format(axis_0_pos, axis_1_pos, axis_2_pos, axis_3_pos)
 *
 *     axis_chars = axis_chars[:-1]
 * items = list(axis_dict.items())
 * items.sort(
 *     key=lambda a: a[0].replace("x", "0").replace("y", "1").replace("z", "2").replace("w", "3")
 * )
 *
 * for size in range(2, 5):
 *     for rw_pass in (True, False):
 *         key_args = ", ".join(list("abcd"[:size]))
 *
 *         print("#define VECTOR_SWIZZLE{:d}_{:s}_DEF(attr, {:s}) \\".format(
 *             size,
 *             "RW" if rw_pass else "RO",
 *             key_args,
 *         ))
 *         print("    {{attr, (getter){:s}, (setter){:s}, {:s}, SWIZZLE{:d}({:s}), }}".format(
 *             "Vector_swizzle_get",
 *             "Vector_swizzle_set" if rw_pass else "nullptr",
 *             "Vector_swizzle_doc",
 *             size,
 *             key_args,
 *         ))
 * print()
 *
 * unique = set()
 * for key, val in items:
 *     num = eval(val)
 *     key_args = ", ".join(["{:d}".format(axis_pos[c]) for c in key.lower()])
 *     macro = "VECTOR_SWIZZLE{:d}_{:s}_DEF".format(
 *         len(key),
 *         "RW" if len(set(key)) == len(key) else "RO",
 *     )
 *     print("    {:s}(\"{:s}\", {:s}),".format(
 *         macro,
 *         key,
 *         key_args,
 *     ))
 *     unique.add(num)
 *
 * if len(unique) != len(items):
 *     print("ERROR, duplicate values found")
 * \endcode
 */

/**
 * Get a new Vector according to the provided swizzle bits.
 */
static PyObject *Vector_swizzle_get(VectorObject *self, void *closure)
{
  size_t axis_to;
  size_t axis_from;
  float vec[MAX_DIMENSIONS];
  uint swizzleClosure;

  if (BaseMath_ReadCallback(self) == -1) {
    return nullptr;
  }

  /* Unpack the axes from the closure into an array. */
  axis_to = 0;
  swizzleClosure = POINTER_AS_INT(closure);
  while (swizzleClosure & SWIZZLE_VALID_AXIS) {
    axis_from = swizzleClosure & SWIZZLE_AXIS;
    if (axis_from >= self->vec_num) {
      PyErr_SetString(PyExc_AttributeError,
                      "Vector swizzle: "
                      "specified axis not present");
      return nullptr;
    }

    vec[axis_to] = self->vec[axis_from];
    swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
    axis_to++;
  }

  return Vector_CreatePyObject(vec, axis_to, Py_TYPE(self));
}

/**
 * Set the items of this vector using a swizzle.
 * - If value is a vector or list this operates like an array copy, except that
 *   the destination is effectively re-ordered as defined by the swizzle. At
 *   most `min(len(source), len(destination))` values will be copied.
 * - If the value is scalar, it is copied to all axes listed in the swizzle.
 * - If an axis appears more than once in the swizzle, the final occurrence is
 *   the one that determines its value.
 *
 * \return 0 on success and -1 on failure. On failure, the vector will be unchanged.
 */
static int Vector_swizzle_set(VectorObject *self, PyObject *value, void *closure)
{
  size_t size_from;
  float scalarVal;

  size_t axis_from;
  size_t axis_to;

  uint swizzleClosure;

  float tvec[MAX_DIMENSIONS];
  float vec_assign[MAX_DIMENSIONS];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  /* Check that the closure can be used with this vector: even 2D vectors have
   * swizzles defined for axes z and w, but they would be invalid. */
  swizzleClosure = POINTER_AS_INT(closure);
  axis_from = 0;

  while (swizzleClosure & SWIZZLE_VALID_AXIS) {
    axis_to = swizzleClosure & SWIZZLE_AXIS;
    if (axis_to >= self->vec_num) {
      PyErr_SetString(PyExc_AttributeError,
                      "Vector swizzle: "
                      "specified axis not present");
      return -1;
    }
    swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
    axis_from++;
  }

  if (((scalarVal = PyFloat_AsDouble(value)) == -1 && PyErr_Occurred()) == 0) {
    int i;

    for (i = 0; i < MAX_DIMENSIONS; i++) {
      vec_assign[i] = scalarVal;
    }

    size_from = axis_from;
  }
  else if (PyErr_Clear(), /* run but ignore the result */
           (size_from = size_t(mathutils_array_parse(
                vec_assign, 2, 4, value, "Vector.**** = swizzle assignment"))) == size_t(-1))
  {
    return -1;
  }

  if (axis_from != size_from) {
    PyErr_SetString(PyExc_AttributeError, "Vector swizzle: size does not match swizzle");
    return -1;
  }

  /* Copy vector contents onto swizzled axes. */
  axis_from = 0;
  swizzleClosure = POINTER_AS_INT(closure);

  /* We must first copy current vec into tvec, else some org values may be lost.
   * See #31760.
   * Assuming self->vec_num can't be higher than MAX_DIMENSIONS! */
  memcpy(tvec, self->vec, self->vec_num * sizeof(float));

  while (swizzleClosure & SWIZZLE_VALID_AXIS) {
    axis_to = swizzleClosure & SWIZZLE_AXIS;
    tvec[axis_to] = vec_assign[axis_from];
    swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
    axis_from++;
  }

  /* We must copy back the whole tvec into vec, else some changes may be lost (e.g. xz...).
   * See #31760. */
  memcpy(self->vec, tvec, self->vec_num * sizeof(float));
  /* continue with BaseMathObject_WriteCallback at the end */

  if (BaseMath_WriteCallback(self) == -1) {
    return -1;
  }

  return 0;
}

#define _SWIZZLE1(a) ((a) | SWIZZLE_VALID_AXIS)
#define _SWIZZLE2(a, b) (_SWIZZLE1(a) | (((b) | SWIZZLE_VALID_AXIS) << (SWIZZLE_BITS_PER_AXIS)))
#define _SWIZZLE3(a, b, c) \
  (_SWIZZLE2(a, b) | (((c) | SWIZZLE_VALID_AXIS) << (SWIZZLE_BITS_PER_AXIS * 2)))
#define _SWIZZLE4(a, b, c, d) \
  (_SWIZZLE3(a, b, c) | (((d) | SWIZZLE_VALID_AXIS) << (SWIZZLE_BITS_PER_AXIS * 3)))

#define SWIZZLE2(a, b) POINTER_FROM_INT(_SWIZZLE2(a, b))
#define SWIZZLE3(a, b, c) POINTER_FROM_INT(_SWIZZLE3(a, b, c))
#define SWIZZLE4(a, b, c, d) POINTER_FROM_INT(_SWIZZLE4(a, b, c, d))

#define VECTOR_SWIZZLE2_RW_DEF(attr, a, b) \
  { \
      attr, \
      (getter)Vector_swizzle_get, \
      (setter)Vector_swizzle_set, \
      Vector_swizzle_doc, \
      SWIZZLE2(a, b), \
  }
#define VECTOR_SWIZZLE2_RO_DEF(attr, a, b) \
  { \
      attr, \
      (getter)Vector_swizzle_get, \
      (setter) nullptr, \
      Vector_swizzle_doc, \
      SWIZZLE2(a, b), \
  }
#define VECTOR_SWIZZLE3_RW_DEF(attr, a, b, c) \
  { \
      attr, \
      (getter)Vector_swizzle_get, \
      (setter)Vector_swizzle_set, \
      Vector_swizzle_doc, \
      SWIZZLE3(a, b, c), \
  }
#define VECTOR_SWIZZLE3_RO_DEF(attr, a, b, c) \
  { \
      attr, \
      (getter)Vector_swizzle_get, \
      (setter) nullptr, \
      Vector_swizzle_doc, \
      SWIZZLE3(a, b, c), \
  }
#define VECTOR_SWIZZLE4_RW_DEF(attr, a, b, c, d) \
  { \
      attr, \
      (getter)Vector_swizzle_get, \
      (setter)Vector_swizzle_set, \
      Vector_swizzle_doc, \
      SWIZZLE4(a, b, c, d), \
  }
#define VECTOR_SWIZZLE4_RO_DEF(attr, a, b, c, d) \
  {attr, (getter)Vector_swizzle_get, (setter) nullptr, Vector_swizzle_doc, SWIZZLE4(a, b, c, d)}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Get/Set Item Definitions
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

static PyGetSetDef Vector_getseters[] = {
    {"x",
     (getter)Vector_axis_get,
     (setter)Vector_axis_set,
     Vector_axis_x_doc,
     POINTER_FROM_INT(0)},
    {"y",
     (getter)Vector_axis_get,
     (setter)Vector_axis_set,
     Vector_axis_y_doc,
     POINTER_FROM_INT(1)},
    {"z",
     (getter)Vector_axis_get,
     (setter)Vector_axis_set,
     Vector_axis_z_doc,
     POINTER_FROM_INT(2)},
    {"w",
     (getter)Vector_axis_get,
     (setter)Vector_axis_set,
     Vector_axis_w_doc,
     POINTER_FROM_INT(3)},
    {"length", (getter)Vector_length_get, (setter)Vector_length_set, Vector_length_doc, nullptr},
    {"length_squared",
     (getter)Vector_length_squared_get,
     (setter) nullptr,
     Vector_length_squared_doc,
     nullptr},
    {"magnitude",
     (getter)Vector_length_get,
     (setter)Vector_length_set,
     Vector_length_doc,
     nullptr},
    {"is_wrapped",
     (getter)BaseMathObject_is_wrapped_get,
     (setter) nullptr,
     BaseMathObject_is_wrapped_doc,
     nullptr},
    {"is_frozen",
     (getter)BaseMathObject_is_frozen_get,
     (setter) nullptr,
     BaseMathObject_is_frozen_doc,
     nullptr},
    {"is_valid",
     (getter)BaseMathObject_is_valid_get,
     (setter) nullptr,
     BaseMathObject_is_valid_doc,
     nullptr},
    {"owner",
     (getter)BaseMathObject_owner_get,
     (setter) nullptr,
     BaseMathObject_owner_doc,
     nullptr},

    /* Auto-generated swizzle attributes, see Python script above. */
    VECTOR_SWIZZLE2_RO_DEF("xx", 0, 0),
    VECTOR_SWIZZLE3_RO_DEF("xxx", 0, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("xxxx", 0, 0, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("xxxy", 0, 0, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("xxxz", 0, 0, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("xxxw", 0, 0, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("xxy", 0, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("xxyx", 0, 0, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("xxyy", 0, 0, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("xxyz", 0, 0, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("xxyw", 0, 0, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("xxz", 0, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("xxzx", 0, 0, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("xxzy", 0, 0, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("xxzz", 0, 0, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("xxzw", 0, 0, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("xxw", 0, 0, 3),
    VECTOR_SWIZZLE4_RO_DEF("xxwx", 0, 0, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("xxwy", 0, 0, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("xxwz", 0, 0, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("xxww", 0, 0, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("xy", 0, 1),
    VECTOR_SWIZZLE3_RO_DEF("xyx", 0, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("xyxx", 0, 1, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("xyxy", 0, 1, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("xyxz", 0, 1, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("xyxw", 0, 1, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("xyy", 0, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("xyyx", 0, 1, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("xyyy", 0, 1, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("xyyz", 0, 1, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("xyyw", 0, 1, 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("xyz", 0, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("xyzx", 0, 1, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("xyzy", 0, 1, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("xyzz", 0, 1, 2, 2),
    VECTOR_SWIZZLE4_RW_DEF("xyzw", 0, 1, 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("xyw", 0, 1, 3),
    VECTOR_SWIZZLE4_RO_DEF("xywx", 0, 1, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("xywy", 0, 1, 3, 1),
    VECTOR_SWIZZLE4_RW_DEF("xywz", 0, 1, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("xyww", 0, 1, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("xz", 0, 2),
    VECTOR_SWIZZLE3_RO_DEF("xzx", 0, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("xzxx", 0, 2, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("xzxy", 0, 2, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("xzxz", 0, 2, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("xzxw", 0, 2, 0, 3),
    VECTOR_SWIZZLE3_RW_DEF("xzy", 0, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("xzyx", 0, 2, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("xzyy", 0, 2, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("xzyz", 0, 2, 1, 2),
    VECTOR_SWIZZLE4_RW_DEF("xzyw", 0, 2, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("xzz", 0, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("xzzx", 0, 2, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("xzzy", 0, 2, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("xzzz", 0, 2, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("xzzw", 0, 2, 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("xzw", 0, 2, 3),
    VECTOR_SWIZZLE4_RO_DEF("xzwx", 0, 2, 3, 0),
    VECTOR_SWIZZLE4_RW_DEF("xzwy", 0, 2, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("xzwz", 0, 2, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("xzww", 0, 2, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("xw", 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("xwx", 0, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("xwxx", 0, 3, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("xwxy", 0, 3, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("xwxz", 0, 3, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("xwxw", 0, 3, 0, 3),
    VECTOR_SWIZZLE3_RW_DEF("xwy", 0, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("xwyx", 0, 3, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("xwyy", 0, 3, 1, 1),
    VECTOR_SWIZZLE4_RW_DEF("xwyz", 0, 3, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("xwyw", 0, 3, 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("xwz", 0, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("xwzx", 0, 3, 2, 0),
    VECTOR_SWIZZLE4_RW_DEF("xwzy", 0, 3, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("xwzz", 0, 3, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("xwzw", 0, 3, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("xww", 0, 3, 3),
    VECTOR_SWIZZLE4_RO_DEF("xwwx", 0, 3, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("xwwy", 0, 3, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("xwwz", 0, 3, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("xwww", 0, 3, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("yx", 1, 0),
    VECTOR_SWIZZLE3_RO_DEF("yxx", 1, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("yxxx", 1, 0, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("yxxy", 1, 0, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("yxxz", 1, 0, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("yxxw", 1, 0, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("yxy", 1, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("yxyx", 1, 0, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("yxyy", 1, 0, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("yxyz", 1, 0, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("yxyw", 1, 0, 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("yxz", 1, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("yxzx", 1, 0, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("yxzy", 1, 0, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("yxzz", 1, 0, 2, 2),
    VECTOR_SWIZZLE4_RW_DEF("yxzw", 1, 0, 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("yxw", 1, 0, 3),
    VECTOR_SWIZZLE4_RO_DEF("yxwx", 1, 0, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("yxwy", 1, 0, 3, 1),
    VECTOR_SWIZZLE4_RW_DEF("yxwz", 1, 0, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("yxww", 1, 0, 3, 3),
    VECTOR_SWIZZLE2_RO_DEF("yy", 1, 1),
    VECTOR_SWIZZLE3_RO_DEF("yyx", 1, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("yyxx", 1, 1, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("yyxy", 1, 1, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("yyxz", 1, 1, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("yyxw", 1, 1, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("yyy", 1, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("yyyx", 1, 1, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("yyyy", 1, 1, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("yyyz", 1, 1, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("yyyw", 1, 1, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("yyz", 1, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("yyzx", 1, 1, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("yyzy", 1, 1, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("yyzz", 1, 1, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("yyzw", 1, 1, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("yyw", 1, 1, 3),
    VECTOR_SWIZZLE4_RO_DEF("yywx", 1, 1, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("yywy", 1, 1, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("yywz", 1, 1, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("yyww", 1, 1, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("yz", 1, 2),
    VECTOR_SWIZZLE3_RW_DEF("yzx", 1, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("yzxx", 1, 2, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("yzxy", 1, 2, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("yzxz", 1, 2, 0, 2),
    VECTOR_SWIZZLE4_RW_DEF("yzxw", 1, 2, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("yzy", 1, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("yzyx", 1, 2, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("yzyy", 1, 2, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("yzyz", 1, 2, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("yzyw", 1, 2, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("yzz", 1, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("yzzx", 1, 2, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("yzzy", 1, 2, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("yzzz", 1, 2, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("yzzw", 1, 2, 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("yzw", 1, 2, 3),
    VECTOR_SWIZZLE4_RW_DEF("yzwx", 1, 2, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("yzwy", 1, 2, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("yzwz", 1, 2, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("yzww", 1, 2, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("yw", 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("ywx", 1, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("ywxx", 1, 3, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("ywxy", 1, 3, 0, 1),
    VECTOR_SWIZZLE4_RW_DEF("ywxz", 1, 3, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("ywxw", 1, 3, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("ywy", 1, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("ywyx", 1, 3, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("ywyy", 1, 3, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("ywyz", 1, 3, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("ywyw", 1, 3, 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("ywz", 1, 3, 2),
    VECTOR_SWIZZLE4_RW_DEF("ywzx", 1, 3, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("ywzy", 1, 3, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("ywzz", 1, 3, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("ywzw", 1, 3, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("yww", 1, 3, 3),
    VECTOR_SWIZZLE4_RO_DEF("ywwx", 1, 3, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("ywwy", 1, 3, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("ywwz", 1, 3, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("ywww", 1, 3, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("zx", 2, 0),
    VECTOR_SWIZZLE3_RO_DEF("zxx", 2, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("zxxx", 2, 0, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("zxxy", 2, 0, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("zxxz", 2, 0, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("zxxw", 2, 0, 0, 3),
    VECTOR_SWIZZLE3_RW_DEF("zxy", 2, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("zxyx", 2, 0, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("zxyy", 2, 0, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("zxyz", 2, 0, 1, 2),
    VECTOR_SWIZZLE4_RW_DEF("zxyw", 2, 0, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("zxz", 2, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("zxzx", 2, 0, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("zxzy", 2, 0, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("zxzz", 2, 0, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("zxzw", 2, 0, 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("zxw", 2, 0, 3),
    VECTOR_SWIZZLE4_RO_DEF("zxwx", 2, 0, 3, 0),
    VECTOR_SWIZZLE4_RW_DEF("zxwy", 2, 0, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("zxwz", 2, 0, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("zxww", 2, 0, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("zy", 2, 1),
    VECTOR_SWIZZLE3_RW_DEF("zyx", 2, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("zyxx", 2, 1, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("zyxy", 2, 1, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("zyxz", 2, 1, 0, 2),
    VECTOR_SWIZZLE4_RW_DEF("zyxw", 2, 1, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("zyy", 2, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("zyyx", 2, 1, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("zyyy", 2, 1, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("zyyz", 2, 1, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("zyyw", 2, 1, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("zyz", 2, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("zyzx", 2, 1, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("zyzy", 2, 1, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("zyzz", 2, 1, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("zyzw", 2, 1, 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("zyw", 2, 1, 3),
    VECTOR_SWIZZLE4_RW_DEF("zywx", 2, 1, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("zywy", 2, 1, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("zywz", 2, 1, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("zyww", 2, 1, 3, 3),
    VECTOR_SWIZZLE2_RO_DEF("zz", 2, 2),
    VECTOR_SWIZZLE3_RO_DEF("zzx", 2, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("zzxx", 2, 2, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("zzxy", 2, 2, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("zzxz", 2, 2, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("zzxw", 2, 2, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("zzy", 2, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("zzyx", 2, 2, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("zzyy", 2, 2, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("zzyz", 2, 2, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("zzyw", 2, 2, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("zzz", 2, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("zzzx", 2, 2, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("zzzy", 2, 2, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("zzzz", 2, 2, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("zzzw", 2, 2, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("zzw", 2, 2, 3),
    VECTOR_SWIZZLE4_RO_DEF("zzwx", 2, 2, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("zzwy", 2, 2, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("zzwz", 2, 2, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("zzww", 2, 2, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("zw", 2, 3),
    VECTOR_SWIZZLE3_RW_DEF("zwx", 2, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("zwxx", 2, 3, 0, 0),
    VECTOR_SWIZZLE4_RW_DEF("zwxy", 2, 3, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("zwxz", 2, 3, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("zwxw", 2, 3, 0, 3),
    VECTOR_SWIZZLE3_RW_DEF("zwy", 2, 3, 1),
    VECTOR_SWIZZLE4_RW_DEF("zwyx", 2, 3, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("zwyy", 2, 3, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("zwyz", 2, 3, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("zwyw", 2, 3, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("zwz", 2, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("zwzx", 2, 3, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("zwzy", 2, 3, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("zwzz", 2, 3, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("zwzw", 2, 3, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("zww", 2, 3, 3),
    VECTOR_SWIZZLE4_RO_DEF("zwwx", 2, 3, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("zwwy", 2, 3, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("zwwz", 2, 3, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("zwww", 2, 3, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("wx", 3, 0),
    VECTOR_SWIZZLE3_RO_DEF("wxx", 3, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("wxxx", 3, 0, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("wxxy", 3, 0, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("wxxz", 3, 0, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("wxxw", 3, 0, 0, 3),
    VECTOR_SWIZZLE3_RW_DEF("wxy", 3, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("wxyx", 3, 0, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("wxyy", 3, 0, 1, 1),
    VECTOR_SWIZZLE4_RW_DEF("wxyz", 3, 0, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("wxyw", 3, 0, 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("wxz", 3, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("wxzx", 3, 0, 2, 0),
    VECTOR_SWIZZLE4_RW_DEF("wxzy", 3, 0, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("wxzz", 3, 0, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("wxzw", 3, 0, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("wxw", 3, 0, 3),
    VECTOR_SWIZZLE4_RO_DEF("wxwx", 3, 0, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("wxwy", 3, 0, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("wxwz", 3, 0, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("wxww", 3, 0, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("wy", 3, 1),
    VECTOR_SWIZZLE3_RW_DEF("wyx", 3, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("wyxx", 3, 1, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("wyxy", 3, 1, 0, 1),
    VECTOR_SWIZZLE4_RW_DEF("wyxz", 3, 1, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("wyxw", 3, 1, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("wyy", 3, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("wyyx", 3, 1, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("wyyy", 3, 1, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("wyyz", 3, 1, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("wyyw", 3, 1, 1, 3),
    VECTOR_SWIZZLE3_RW_DEF("wyz", 3, 1, 2),
    VECTOR_SWIZZLE4_RW_DEF("wyzx", 3, 1, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("wyzy", 3, 1, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("wyzz", 3, 1, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("wyzw", 3, 1, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("wyw", 3, 1, 3),
    VECTOR_SWIZZLE4_RO_DEF("wywx", 3, 1, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("wywy", 3, 1, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("wywz", 3, 1, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("wyww", 3, 1, 3, 3),
    VECTOR_SWIZZLE2_RW_DEF("wz", 3, 2),
    VECTOR_SWIZZLE3_RW_DEF("wzx", 3, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("wzxx", 3, 2, 0, 0),
    VECTOR_SWIZZLE4_RW_DEF("wzxy", 3, 2, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("wzxz", 3, 2, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("wzxw", 3, 2, 0, 3),
    VECTOR_SWIZZLE3_RW_DEF("wzy", 3, 2, 1),
    VECTOR_SWIZZLE4_RW_DEF("wzyx", 3, 2, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("wzyy", 3, 2, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("wzyz", 3, 2, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("wzyw", 3, 2, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("wzz", 3, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("wzzx", 3, 2, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("wzzy", 3, 2, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("wzzz", 3, 2, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("wzzw", 3, 2, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("wzw", 3, 2, 3),
    VECTOR_SWIZZLE4_RO_DEF("wzwx", 3, 2, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("wzwy", 3, 2, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("wzwz", 3, 2, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("wzww", 3, 2, 3, 3),
    VECTOR_SWIZZLE2_RO_DEF("ww", 3, 3),
    VECTOR_SWIZZLE3_RO_DEF("wwx", 3, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("wwxx", 3, 3, 0, 0),
    VECTOR_SWIZZLE4_RO_DEF("wwxy", 3, 3, 0, 1),
    VECTOR_SWIZZLE4_RO_DEF("wwxz", 3, 3, 0, 2),
    VECTOR_SWIZZLE4_RO_DEF("wwxw", 3, 3, 0, 3),
    VECTOR_SWIZZLE3_RO_DEF("wwy", 3, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("wwyx", 3, 3, 1, 0),
    VECTOR_SWIZZLE4_RO_DEF("wwyy", 3, 3, 1, 1),
    VECTOR_SWIZZLE4_RO_DEF("wwyz", 3, 3, 1, 2),
    VECTOR_SWIZZLE4_RO_DEF("wwyw", 3, 3, 1, 3),
    VECTOR_SWIZZLE3_RO_DEF("wwz", 3, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("wwzx", 3, 3, 2, 0),
    VECTOR_SWIZZLE4_RO_DEF("wwzy", 3, 3, 2, 1),
    VECTOR_SWIZZLE4_RO_DEF("wwzz", 3, 3, 2, 2),
    VECTOR_SWIZZLE4_RO_DEF("wwzw", 3, 3, 2, 3),
    VECTOR_SWIZZLE3_RO_DEF("www", 3, 3, 3),
    VECTOR_SWIZZLE4_RO_DEF("wwwx", 3, 3, 3, 0),
    VECTOR_SWIZZLE4_RO_DEF("wwwy", 3, 3, 3, 1),
    VECTOR_SWIZZLE4_RO_DEF("wwwz", 3, 3, 3, 2),
    VECTOR_SWIZZLE4_RO_DEF("wwww", 3, 3, 3, 3),

#undef AXIS_FROM_CHAR
#undef SWIZZLE1
#undef SWIZZLE2
#undef SWIZZLE3
#undef SWIZZLE4
#undef _SWIZZLE1
#undef _SWIZZLE2
#undef _SWIZZLE3
#undef _SWIZZLE4

#undef VECTOR_SWIZZLE2_RW_DEF
#undef VECTOR_SWIZZLE2_RO_DEF
#undef VECTOR_SWIZZLE3_RW_DEF
#undef VECTOR_SWIZZLE3_RO_DEF
#undef VECTOR_SWIZZLE4_RW_DEF
#undef VECTOR_SWIZZLE4_RO_DEF

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

/* -------------------------------------------------------------------- */
/** \name Vector Type: Method Definitions
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

static PyMethodDef Vector_methods[] = {
    /* Class Methods */
    {"Fill", (PyCFunction)C_Vector_Fill, METH_VARARGS | METH_CLASS, C_Vector_Fill_doc},
    {"Range", (PyCFunction)C_Vector_Range, METH_VARARGS | METH_CLASS, C_Vector_Range_doc},
    {"Linspace", (PyCFunction)C_Vector_Linspace, METH_VARARGS | METH_CLASS, C_Vector_Linspace_doc},
    {"Repeat", (PyCFunction)C_Vector_Repeat, METH_VARARGS | METH_CLASS, C_Vector_Repeat_doc},

    /* In place only. */
    {"zero", (PyCFunction)Vector_zero, METH_NOARGS, Vector_zero_doc},
    {"negate", (PyCFunction)Vector_negate, METH_NOARGS, Vector_negate_doc},

    /* Operate on original or copy. */
    {"normalize", (PyCFunction)Vector_normalize, METH_NOARGS, Vector_normalize_doc},
    {"normalized", (PyCFunction)Vector_normalized, METH_NOARGS, Vector_normalized_doc},

    {"resize", (PyCFunction)Vector_resize, METH_O, Vector_resize_doc},
    {"resized", (PyCFunction)Vector_resized, METH_O, Vector_resized_doc},
    {"to_2d", (PyCFunction)Vector_to_2d, METH_NOARGS, Vector_to_2d_doc},
    {"resize_2d", (PyCFunction)Vector_resize_2d, METH_NOARGS, Vector_resize_2d_doc},
    {"to_3d", (PyCFunction)Vector_to_3d, METH_NOARGS, Vector_to_3d_doc},
    {"resize_3d", (PyCFunction)Vector_resize_3d, METH_NOARGS, Vector_resize_3d_doc},
    {"to_4d", (PyCFunction)Vector_to_4d, METH_NOARGS, Vector_to_4d_doc},
    {"resize_4d", (PyCFunction)Vector_resize_4d, METH_NOARGS, Vector_resize_4d_doc},
    {"to_tuple", (PyCFunction)Vector_to_tuple, METH_VARARGS, Vector_to_tuple_doc},
    {"to_track_quat", (PyCFunction)Vector_to_track_quat, METH_VARARGS, Vector_to_track_quat_doc},
    {"orthogonal", (PyCFunction)Vector_orthogonal, METH_NOARGS, Vector_orthogonal_doc},

    /* Operation between 2 or more types. */
    {"reflect", (PyCFunction)Vector_reflect, METH_O, Vector_reflect_doc},
    {"cross", (PyCFunction)Vector_cross, METH_O, Vector_cross_doc},
    {"dot", (PyCFunction)Vector_dot, METH_O, Vector_dot_doc},
    {"angle", (PyCFunction)Vector_angle, METH_VARARGS, Vector_angle_doc},
    {"angle_signed", (PyCFunction)Vector_angle_signed, METH_VARARGS, Vector_angle_signed_doc},
    {"rotation_difference",
     (PyCFunction)Vector_rotation_difference,
     METH_O,
     Vector_rotation_difference_doc},
    {"project", (PyCFunction)Vector_project, METH_O, Vector_project_doc},
    {"lerp", (PyCFunction)Vector_lerp, METH_VARARGS, Vector_lerp_doc},
    {"slerp", (PyCFunction)Vector_slerp, METH_VARARGS, Vector_slerp_doc},
    {"rotate", (PyCFunction)Vector_rotate, METH_O, Vector_rotate_doc},

    /* Base-math methods. */
    {"freeze", (PyCFunction)BaseMathObject_freeze, METH_NOARGS, BaseMathObject_freeze_doc},

    {"copy", (PyCFunction)Vector_copy, METH_NOARGS, Vector_copy_doc},
    {"__copy__", (PyCFunction)Vector_copy, METH_NOARGS, nullptr},
    {"__deepcopy__", (PyCFunction)Vector_deepcopy, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: Python Object Definition
 *
 * \note #Py_TPFLAGS_CHECKTYPES allows us to avoid casting all types to Vector when coercing
 * but this means for eg that (vec * mat) and (mat * vec)
 * both get sent to Vector_mul and it needs to sort out the order
 * \{ */

#ifdef MATH_STANDALONE
#  define Vector_str nullptr
#endif

PyDoc_STRVAR(
    /* Wrap. */
    vector_doc,
    ".. class:: Vector(seq=(0.0, 0.0, 0.0), /)\n"
    "\n"
    "   This object gives access to Vectors in Blender.\n"
    "\n"
    "   :arg seq: Components of the vector, must be a sequence of at least two.\n"
    "   :type seq: Sequence[float]\n");
PyTypeObject vector_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Vector",
    /*tp_basicsize*/ sizeof(VectorObject),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BaseMathObject_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Vector_repr,
    /*tp_as_number*/ &Vector_NumMethods,
    /*tp_as_sequence*/ &Vector_SeqMethods,
    /*tp_as_mapping*/ &Vector_AsMapping,
    /*tp_hash*/ (hashfunc)Vector_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ (reprfunc)Vector_str,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ &Vector_as_buffer,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ vector_doc,
    /*tp_traverse*/ (traverseproc)BaseMathObject_traverse,
    /*tp_clear*/ (inquiry)BaseMathObject_clear,
    /*tp_richcompare*/ (richcmpfunc)Vector_richcmpr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ Vector_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ Vector_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ (inquiry)BaseMathObject_is_gc,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ Vector_vectorcall,
};

#ifdef MATH_STANDALONE
#  undef Vector_str nullptr
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Type: C/API Constructors
 * \{ */

PyObject *Vector_CreatePyObject(const float *vec, const int vec_num, PyTypeObject *base_type)
{
  VectorObject *self;
  float *vec_alloc;

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector(): invalid size");
    return nullptr;
  }

  vec_alloc = static_cast<float *>(PyMem_Malloc(vec_num * sizeof(float)));
  if (UNLIKELY(vec_alloc == nullptr)) {
    PyErr_SetString(PyExc_MemoryError,
                    "Vector(): "
                    "problem allocating data");
    return nullptr;
  }

  self = BASE_MATH_NEW(VectorObject, vector_Type, base_type);
  if (self) {
    self->vec = vec_alloc;
    self->vec_num = vec_num;

    /* Initialize callbacks as nullptr. */
    self->cb_user = nullptr;
    self->cb_type = self->cb_subtype = 0;

    if (vec) {
      memcpy(self->vec, vec, vec_num * sizeof(float));
    }
    else { /* new empty */
      copy_vn_fl(self->vec, vec_num, 0.0f);
      if (vec_num == 4) { /* do the homogeneous thing */
        self->vec[3] = 1.0f;
      }
    }
    self->flag = BASE_MATH_FLAG_DEFAULT;
  }
  else {
    PyMem_Free(vec_alloc);
  }

  return (PyObject *)self;
}

PyObject *Vector_CreatePyObject_wrap(float *vec, const int vec_num, PyTypeObject *base_type)
{
  VectorObject *self;

  if (vec_num < 2) {
    PyErr_SetString(PyExc_RuntimeError, "Vector(): invalid size");
    return nullptr;
  }

  self = BASE_MATH_NEW(VectorObject, vector_Type, base_type);
  if (self) {
    self->vec_num = vec_num;

    /* Initialize callbacks as nullptr. */
    self->cb_user = nullptr;
    self->cb_type = self->cb_subtype = 0;

    self->vec = vec;
    self->flag = BASE_MATH_FLAG_DEFAULT | BASE_MATH_FLAG_IS_WRAP;
  }
  return (PyObject *)self;
}

PyObject *Vector_CreatePyObject_cb(PyObject *cb_user, int vec_num, uchar cb_type, uchar cb_subtype)
{
  VectorObject *self = (VectorObject *)Vector_CreatePyObject(nullptr, vec_num, nullptr);
  if (self) {
    Py_INCREF(cb_user);
    self->cb_user = cb_user;
    self->cb_type = cb_type;
    self->cb_subtype = cb_subtype;
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)self));
    PyObject_GC_Track(self);
  }

  return (PyObject *)self;
}

PyObject *Vector_CreatePyObject_alloc(float *vec, const int vec_num, PyTypeObject *base_type)
{
  VectorObject *self;
  self = (VectorObject *)Vector_CreatePyObject_wrap(vec, vec_num, base_type);
  if (self) {
    self->flag &= ~BASE_MATH_FLAG_IS_WRAP;
  }

  return (PyObject *)self;
}

/** \} */
