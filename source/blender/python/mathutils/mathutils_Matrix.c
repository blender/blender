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
#  include "BLI_string.h"
#endif

typedef enum eMatrixAccess_t {
  MAT_ACCESS_ROW,
  MAT_ACCESS_COL,
} eMatrixAccess_t;

static PyObject *Matrix_copy_notest(MatrixObject *self, const float *matrix);
static PyObject *Matrix_copy(MatrixObject *self);
static PyObject *Matrix_deepcopy(MatrixObject *self, PyObject *args);
static int Matrix_ass_slice(MatrixObject *self, int begin, int end, PyObject *value);
static PyObject *matrix__apply_to_copy(PyObject *(*matrix_func)(MatrixObject *),
                                       MatrixObject *self);
static PyObject *MatrixAccess_CreatePyObject(MatrixObject *matrix, const eMatrixAccess_t type);

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static int matrix_row_vector_check(MatrixObject *mat, VectorObject *vec, int row)
{
  if ((vec->vec_num != mat->col_num) || (row >= mat->row_num)) {
    PyErr_SetString(PyExc_AttributeError,
                    "Matrix(): "
                    "owner matrix has been resized since this row vector was created");
    return 0;
  }

  return 1;
}

static int matrix_col_vector_check(MatrixObject *mat, VectorObject *vec, int col)
{
  if ((vec->vec_num != mat->row_num) || (col >= mat->col_num)) {
    PyErr_SetString(PyExc_AttributeError,
                    "Matrix(): "
                    "owner matrix has been resized since this column vector was created");
    return 0;
  }

  return 1;
}

/** When a matrix is 4x4 size but initialized as a 3x3, re-assign values for 4x4. */
static void matrix_3x3_as_4x4(float mat[16])
{
  mat[10] = mat[8];
  mat[9] = mat[7];
  mat[8] = mat[6];
  mat[7] = 0.0f;
  mat[6] = mat[5];
  mat[5] = mat[4];
  mat[4] = mat[3];
  mat[3] = 0.0f;
}

void matrix_as_3x3(float mat[3][3], MatrixObject *self)
{
  copy_v3_v3(mat[0], MATRIX_COL_PTR(self, 0));
  copy_v3_v3(mat[1], MATRIX_COL_PTR(self, 1));
  copy_v3_v3(mat[2], MATRIX_COL_PTR(self, 2));
}

static void matrix_copy(MatrixObject *mat_dst, const MatrixObject *mat_src)
{
  BLI_assert((mat_dst->col_num == mat_src->col_num) && (mat_dst->row_num == mat_src->row_num));
  BLI_assert(mat_dst != mat_src);

  memcpy(mat_dst->matrix, mat_src->matrix, sizeof(float) * (mat_dst->col_num * mat_dst->row_num));
}

static void matrix_unit_internal(MatrixObject *self)
{
  const int mat_size = sizeof(float) * (self->col_num * self->row_num);
  memset(self->matrix, 0x0, mat_size);
  const int col_row_max = min_ii(self->col_num, self->row_num);
  const int row_num = self->row_num;
  for (int col = 0; col < col_row_max; col++) {
    self->matrix[(col * row_num) + col] = 1.0f;
  }
}

/** Transposes memory layout, row/columns don't have to match. */
static void matrix_transpose_internal(float mat_dst_fl[], const MatrixObject *mat_src)
{
  ushort col, row;
  uint i = 0;

  for (row = 0; row < mat_src->row_num; row++) {
    for (col = 0; col < mat_src->col_num; col++) {
      mat_dst_fl[i++] = MATRIX_ITEM(mat_src, row, col);
    }
  }
}

/** Assumes `rowsize == colsize` is checked and the read callback has run. */
static float matrix_determinant_internal(const MatrixObject *self)
{
  if (self->col_num == 2) {
    return determinant_m2(MATRIX_ITEM(self, 0, 0),
                          MATRIX_ITEM(self, 0, 1),
                          MATRIX_ITEM(self, 1, 0),
                          MATRIX_ITEM(self, 1, 1));
  }
  if (self->col_num == 3) {
    return determinant_m3(MATRIX_ITEM(self, 0, 0),
                          MATRIX_ITEM(self, 0, 1),
                          MATRIX_ITEM(self, 0, 2),
                          MATRIX_ITEM(self, 1, 0),
                          MATRIX_ITEM(self, 1, 1),
                          MATRIX_ITEM(self, 1, 2),
                          MATRIX_ITEM(self, 2, 0),
                          MATRIX_ITEM(self, 2, 1),
                          MATRIX_ITEM(self, 2, 2));
  }

  return determinant_m4((const float(*)[4])self->matrix);
}

static void adjoint_matrix_n(float *mat_dst, const float *mat_src, const ushort dim)
{
  /* calculate the classical adjoint */
  switch (dim) {
    case 2: {
      adjoint_m2_m2((float(*)[2])mat_dst, (const float(*)[2])mat_src);
      break;
    }
    case 3: {
      adjoint_m3_m3((float(*)[3])mat_dst, (const float(*)[3])mat_src);
      break;
    }
    case 4: {
      adjoint_m4_m4((float(*)[4])mat_dst, (const float(*)[4])mat_src);
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }
}

static void matrix_invert_with_det_n_internal(float *mat_dst,
                                              const float *mat_src,
                                              const float det,
                                              const ushort dim)
{
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];
  ushort i, j, k;

  BLI_assert(det != 0.0f);

  adjoint_matrix_n(mat, mat_src, dim);

  /* divide by determinant & set values */
  k = 0;
  for (i = 0; i < dim; i++) {   /* col_num */
    for (j = 0; j < dim; j++) { /* row_num */
      mat_dst[MATRIX_ITEM_INDEX_NUMROW(dim, j, i)] = mat[k++] / det;
    }
  }
}

/**
 * \param r_mat: can be from `self->matrix` or not.
 */
static bool matrix_invert_internal(const MatrixObject *self, float *r_mat)
{
  float det;
  BLI_assert(self->col_num == self->row_num);
  det = matrix_determinant_internal(self);

  if (det != 0.0f) {
    matrix_invert_with_det_n_internal(r_mat, self->matrix, det, self->col_num);
    return true;
  }

  return false;
}

/**
 * Similar to `matrix_invert_internal` but should never error.
 * \param r_mat: can be from `self->matrix` or not.
 */
static void matrix_invert_safe_internal(const MatrixObject *self, float *r_mat)
{
  float det;
  float *in_mat = self->matrix;
  BLI_assert(self->col_num == self->row_num);
  det = matrix_determinant_internal(self);

  if (det == 0.0f) {
    const float eps = PSEUDOINVERSE_EPSILON;

    /* We will copy self->matrix into r_mat (if needed),
     * and modify it in place to add diagonal epsilon. */
    in_mat = r_mat;

    switch (self->col_num) {
      case 2: {
        float(*mat)[2] = (float(*)[2])in_mat;

        if (in_mat != self->matrix) {
          copy_m2_m2(mat, (const float(*)[2])self->matrix);
        }
        mat[0][0] += eps;
        mat[1][1] += eps;

        if (UNLIKELY((det = determinant_m2(mat[0][0], mat[0][1], mat[1][0], mat[1][1])) == 0.0f)) {
          unit_m2(mat);
          det = 1.0f;
        }
        break;
      }
      case 3: {
        float(*mat)[3] = (float(*)[3])in_mat;

        if (in_mat != self->matrix) {
          copy_m3_m3(mat, (const float(*)[3])self->matrix);
        }
        mat[0][0] += eps;
        mat[1][1] += eps;
        mat[2][2] += eps;

        if (UNLIKELY((det = determinant_m3_array(mat)) == 0.0f)) {
          unit_m3(mat);
          det = 1.0f;
        }
        break;
      }
      case 4: {
        float(*mat)[4] = (float(*)[4])in_mat;

        if (in_mat != self->matrix) {
          copy_m4_m4(mat, (const float(*)[4])self->matrix);
        }
        mat[0][0] += eps;
        mat[1][1] += eps;
        mat[2][2] += eps;
        mat[3][3] += eps;

        if (UNLIKELY(det = determinant_m4(mat)) == 0.0f) {
          unit_m4(mat);
          det = 1.0f;
        }
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }

  matrix_invert_with_det_n_internal(r_mat, in_mat, det, self->col_num);
}

static PyObject *matrix__apply_to_copy(PyObject *(*matrix_func)(MatrixObject *),
                                       MatrixObject *self)
{
  PyObject *ret = Matrix_copy(self);
  if (ret) {
    PyObject *ret_dummy = matrix_func((MatrixObject *)ret);
    if (ret_dummy) {
      Py_DECREF(ret_dummy);
      return ret;
    }
    /* error */
    Py_DECREF(ret);
    return NULL;
  }

  /* copy may fail if the read callback errors out */
  return NULL;
}

static bool matrix_is_identity(MatrixObject *self)
{
  for (int row = 0; row < self->row_num; row++) {
    for (int col = 0; col < self->col_num; col++) {
      if (MATRIX_ITEM(self, row, col) != ((row != col) ? 0.0f : 1.0f)) {
        return false;
      }
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Row Callbacks
 * This is so you can do `matrix[i][j] = val` or `matrix.row[i][j] = val`.
 * \{ */

uchar mathutils_matrix_row_cb_index = -1;

static int mathutils_matrix_row_check(BaseMathObject *bmo)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  return BaseMath_ReadCallback(self);
}

static int mathutils_matrix_row_get(BaseMathObject *bmo, int row)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  int col;

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }
  if (!matrix_row_vector_check(self, (VectorObject *)bmo, row)) {
    return -1;
  }

  for (col = 0; col < self->col_num; col++) {
    bmo->data[col] = MATRIX_ITEM(self, row, col);
  }

  return 0;
}

static int mathutils_matrix_row_set(BaseMathObject *bmo, int row)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  int col;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }
  if (!matrix_row_vector_check(self, (VectorObject *)bmo, row)) {
    return -1;
  }

  for (col = 0; col < self->col_num; col++) {
    MATRIX_ITEM(self, row, col) = bmo->data[col];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

static int mathutils_matrix_row_get_index(BaseMathObject *bmo, int row, int col)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }
  if (!matrix_row_vector_check(self, (VectorObject *)bmo, row)) {
    return -1;
  }

  bmo->data[col] = MATRIX_ITEM(self, row, col);
  return 0;
}

static int mathutils_matrix_row_set_index(BaseMathObject *bmo, int row, int col)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }
  if (!matrix_row_vector_check(self, (VectorObject *)bmo, row)) {
    return -1;
  }

  MATRIX_ITEM(self, row, col) = bmo->data[col];

  (void)BaseMath_WriteCallback(self);
  return 0;
}

Mathutils_Callback mathutils_matrix_row_cb = {
    mathutils_matrix_row_check,
    mathutils_matrix_row_get,
    mathutils_matrix_row_set,
    mathutils_matrix_row_get_index,
    mathutils_matrix_row_set_index,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Column Callbacks
 * This is so you can do `matrix.col[i][j] = val`.
 * \{ */

uchar mathutils_matrix_col_cb_index = -1;

static int mathutils_matrix_col_check(BaseMathObject *bmo)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  return BaseMath_ReadCallback(self);
}

static int mathutils_matrix_col_get(BaseMathObject *bmo, int col)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  int row_num;
  int row;

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }
  if (!matrix_col_vector_check(self, (VectorObject *)bmo, col)) {
    return -1;
  }

  /* for 'translation' `vec_num` will always be '3' even on 4x4 vec */
  row_num = min_ii(self->row_num, ((const VectorObject *)bmo)->vec_num);

  for (row = 0; row < row_num; row++) {
    bmo->data[row] = MATRIX_ITEM(self, row, col);
  }

  return 0;
}

static int mathutils_matrix_col_set(BaseMathObject *bmo, int col)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  int row_num;
  int row;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }
  if (!matrix_col_vector_check(self, (VectorObject *)bmo, col)) {
    return -1;
  }

  /* for 'translation' `vec_num` will always be '3' even on 4x4 vec */
  row_num = min_ii(self->row_num, ((const VectorObject *)bmo)->vec_num);

  for (row = 0; row < row_num; row++) {
    MATRIX_ITEM(self, row, col) = bmo->data[row];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

static int mathutils_matrix_col_get_index(BaseMathObject *bmo, int col, int row)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }
  if (!matrix_col_vector_check(self, (VectorObject *)bmo, col)) {
    return -1;
  }

  bmo->data[row] = MATRIX_ITEM(self, row, col);
  return 0;
}

static int mathutils_matrix_col_set_index(BaseMathObject *bmo, int col, int row)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }
  if (!matrix_col_vector_check(self, (VectorObject *)bmo, col)) {
    return -1;
  }

  MATRIX_ITEM(self, row, col) = bmo->data[row];

  (void)BaseMath_WriteCallback(self);
  return 0;
}

Mathutils_Callback mathutils_matrix_col_cb = {
    mathutils_matrix_col_check,
    mathutils_matrix_col_get,
    mathutils_matrix_col_set,
    mathutils_matrix_col_get_index,
    mathutils_matrix_col_set_index,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Translation Callbacks
 * This is so you can do `matrix.translation = val`.
 *
 * \note this is _exactly like matrix.col except the 4th component is always omitted.
 * \{ */

uchar mathutils_matrix_translation_cb_index = -1;

static int mathutils_matrix_translation_check(BaseMathObject *bmo)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  return BaseMath_ReadCallback(self);
}

static int mathutils_matrix_translation_get(BaseMathObject *bmo, int col)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  int row;

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  for (row = 0; row < 3; row++) {
    bmo->data[row] = MATRIX_ITEM(self, row, col);
  }

  return 0;
}

static int mathutils_matrix_translation_set(BaseMathObject *bmo, int col)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;
  int row;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  for (row = 0; row < 3; row++) {
    MATRIX_ITEM(self, row, col) = bmo->data[row];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

static int mathutils_matrix_translation_get_index(BaseMathObject *bmo, int col, int row)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  bmo->data[row] = MATRIX_ITEM(self, row, col);
  return 0;
}

static int mathutils_matrix_translation_set_index(BaseMathObject *bmo, int col, int row)
{
  MatrixObject *self = (MatrixObject *)bmo->cb_user;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  MATRIX_ITEM(self, row, col) = bmo->data[row];

  (void)BaseMath_WriteCallback(self);
  return 0;
}

Mathutils_Callback mathutils_matrix_translation_cb = {
    mathutils_matrix_translation_check,
    mathutils_matrix_translation_get,
    mathutils_matrix_translation_set,
    mathutils_matrix_translation_get_index,
    mathutils_matrix_translation_set_index,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: `__new__` / `mathutils.Matrix()`
 * \{ */

static PyObject *Matrix_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  if (kwds && PyDict_Size(kwds)) {
    PyErr_SetString(PyExc_TypeError,
                    "Matrix(): "
                    "takes no keyword args");
    return NULL;
  }

  switch (PyTuple_GET_SIZE(args)) {
    case 0:
      return Matrix_CreatePyObject(NULL, 4, 4, type);
    case 1: {
      PyObject *arg = PyTuple_GET_ITEM(args, 0);

      /* Input is now as a sequence of rows so length of sequence
       * is the number of rows */
      /* -1 is an error, size checks will account for this */
      const ushort row_num = PySequence_Size(arg);

      if (row_num >= 2 && row_num <= 4) {
        PyObject *item = PySequence_GetItem(arg, 0);
        /* Since each item is a row, number of items is the
         * same as the number of columns */
        const ushort col_num = PySequence_Size(item);
        Py_XDECREF(item);

        if (col_num >= 2 && col_num <= 4) {
          /* Sane row & col size, new matrix and assign as slice. */
          PyObject *matrix = Matrix_CreatePyObject(NULL, col_num, row_num, type);
          if (Matrix_ass_slice((MatrixObject *)matrix, 0, INT_MAX, arg) == 0) {
            return matrix;
          }
          /* matrix ok, slice assignment not */
          Py_DECREF(matrix);
        }
      }
      break;
    }
  }

  /* will overwrite error */
  PyErr_SetString(PyExc_TypeError,
                  "Matrix(): "
                  "expects no args or a single arg containing 2-4 numeric sequences");
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Class Methods
 * \{ */

/** Identity constructor: `mathutils.Matrix.Identity()`. */
PyDoc_STRVAR(C_Matrix_Identity_doc,
             ".. classmethod:: Identity(size)\n"
             "\n"
             "   Create an identity matrix.\n"
             "\n"
             "   :arg size: The size of the identity matrix to construct [2, 4].\n"
             "   :type size: int\n"
             "   :return: A new identity matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *C_Matrix_Identity(PyObject *cls, PyObject *args)
{
  int matSize;

  if (!PyArg_ParseTuple(args, "i:Matrix.Identity", &matSize)) {
    return NULL;
  }

  if (matSize < 2 || matSize > 4) {
    PyErr_SetString(PyExc_RuntimeError,
                    "Matrix.Identity(): "
                    "size must be between 2 and 4");
    return NULL;
  }

  return Matrix_CreatePyObject(NULL, matSize, matSize, (PyTypeObject *)cls);
}

/** Rotation constructor: `mathutils.Matrix.Rotation()`. */
PyDoc_STRVAR(C_Matrix_Rotation_doc,
             ".. classmethod:: Rotation(angle, size, axis)\n"
             "\n"
             "   Create a matrix representing a rotation.\n"
             "\n"
             "   :arg angle: The angle of rotation desired, in radians.\n"
             "   :type angle: float\n"
             "   :arg size: The size of the rotation matrix to construct [2, 4].\n"
             "   :type size: int\n"
             "   :arg axis: a string in ['X', 'Y', 'Z'] or a 3D Vector Object\n"
             "      (optional when size is 2).\n"
             "   :type axis: string or :class:`Vector`\n"
             "   :return: A new rotation matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *C_Matrix_Rotation(PyObject *cls, PyObject *args)
{
  PyObject *vec = NULL;
  const char *axis = NULL;
  int matSize;
  double angle; /* Use double because of precision problems at high values. */
  float mat[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  if (!PyArg_ParseTuple(args, "di|O:Matrix.Rotation", &angle, &matSize, &vec)) {
    return NULL;
  }

  if (vec && PyUnicode_Check(vec)) {
    axis = PyUnicode_AsUTF8((PyObject *)vec);
    if (axis == NULL || axis[0] == '\0' || axis[1] != '\0' || axis[0] < 'X' || axis[0] > 'Z') {
      PyErr_SetString(PyExc_ValueError,
                      "Matrix.Rotation(): "
                      "3rd argument axis value must be a 3D vector "
                      "or a string in 'X', 'Y', 'Z'");
      return NULL;
    }

    /* use the string */
    vec = NULL;
  }

  angle = angle_wrap_rad(angle);

  if (!ELEM(matSize, 2, 3, 4)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.Rotation(): "
                    "can only return a 2x2 3x3 or 4x4 matrix");
    return NULL;
  }
  if (matSize == 2 && (vec != NULL)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.Rotation(): "
                    "cannot create a 2x2 rotation matrix around arbitrary axis");
    return NULL;
  }
  if ((ELEM(matSize, 3, 4)) && (axis == NULL) && (vec == NULL)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.Rotation(): "
                    "axis of rotation for 3d and 4d matrices is required");
    return NULL;
  }

  /* check for valid vector/axis above */
  if (vec) {
    float tvec[3];

    if (mathutils_array_parse(
            tvec, 3, 3, vec, "Matrix.Rotation(angle, size, axis), invalid 'axis' arg") == -1) {
      return NULL;
    }

    axis_angle_to_mat3((float(*)[3])mat, tvec, angle);
  }
  else if (matSize == 2) {
    angle_to_mat2((float(*)[2])mat, angle);
  }
  else {
    /* valid axis checked above */
    axis_angle_to_mat3_single((float(*)[3])mat, axis[0], angle);
  }

  if (matSize == 4) {
    matrix_3x3_as_4x4(mat);
  }
  /* pass to matrix creation */
  return Matrix_CreatePyObject(mat, matSize, matSize, (PyTypeObject *)cls);
}

/** Translation constructor: `mathutils.Matrix.Translation()`. */
PyDoc_STRVAR(C_Matrix_Translation_doc,
             ".. classmethod:: Translation(vector)\n"
             "\n"
             "   Create a matrix representing a translation.\n"
             "\n"
             "   :arg vector: The translation vector.\n"
             "   :type vector: :class:`Vector`\n"
             "   :return: An identity matrix with a translation.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *C_Matrix_Translation(PyObject *cls, PyObject *value)
{
  float mat[4][4];

  unit_m4(mat);

  if (mathutils_array_parse(
          mat[3], 3, 4, value, "mathutils.Matrix.Translation(vector), invalid vector arg") == -1) {
    return NULL;
  }

  return Matrix_CreatePyObject(&mat[0][0], 4, 4, (PyTypeObject *)cls);
}

PyDoc_STRVAR(C_Matrix_Diagonal_doc,
             ".. classmethod:: Diagonal(vector)\n"
             "\n"
             "   Create a diagonal (scaling) matrix using the values from the vector.\n"
             "\n"
             "   :arg vector: The vector of values for the diagonal.\n"
             "   :type vector: :class:`Vector`\n"
             "   :return: A diagonal matrix.\n"
             "   :rtype: :class:`Matrix`\n");
/** Diagonal constructor: `mathutils.Matrix.Diagonal()`. */
static PyObject *C_Matrix_Diagonal(PyObject *cls, PyObject *value)
{
  float mat[16] = {0.0f};
  float vec[4];

  int size = mathutils_array_parse(
      vec, 2, 4, value, "mathutils.Matrix.Diagonal(vector), invalid vector arg");

  if (size == -1) {
    return NULL;
  }

  for (int i = 0; i < size; i++) {
    mat[size * i + i] = vec[i];
  }

  return Matrix_CreatePyObject(mat, size, size, (PyTypeObject *)cls);
}

/** Scale constructor: `mathutils.Matrix.Scale()`. */
PyDoc_STRVAR(C_Matrix_Scale_doc,
             ".. classmethod:: Scale(factor, size, axis)\n"
             "\n"
             "   Create a matrix representing a scaling.\n"
             "\n"
             "   :arg factor: The factor of scaling to apply.\n"
             "   :type factor: float\n"
             "   :arg size: The size of the scale matrix to construct [2, 4].\n"
             "   :type size: int\n"
             "   :arg axis: Direction to influence scale. (optional).\n"
             "   :type axis: :class:`Vector`\n"
             "   :return: A new scale matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *C_Matrix_Scale(PyObject *cls, PyObject *args)
{
  PyObject *vec = NULL;
  int vec_num;
  float tvec[3];
  float factor;
  int matSize;
  float mat[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  if (!PyArg_ParseTuple(args, "fi|O:Matrix.Scale", &factor, &matSize, &vec)) {
    return NULL;
  }
  if (!ELEM(matSize, 2, 3, 4)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.Scale(): "
                    "can only return a 2x2 3x3 or 4x4 matrix");
    return NULL;
  }
  if (vec) {
    vec_num = (matSize == 2 ? 2 : 3);
    if (mathutils_array_parse(
            tvec, vec_num, vec_num, vec, "Matrix.Scale(factor, size, axis), invalid 'axis' arg") ==
        -1) {
      return NULL;
    }
  }
  if (vec == NULL) { /* scaling along axis */
    if (matSize == 2) {
      mat[0] = factor;
      mat[3] = factor;
    }
    else {
      mat[0] = factor;
      mat[4] = factor;
      mat[8] = factor;
    }
  }
  else {
    /* scaling in arbitrary direction
     * normalize arbitrary axis */
    float norm = 0.0f;
    int x;
    for (x = 0; x < vec_num; x++) {
      norm += tvec[x] * tvec[x];
    }
    norm = sqrtf(norm);
    for (x = 0; x < vec_num; x++) {
      tvec[x] /= norm;
    }
    if (matSize == 2) {
      mat[0] = 1 + ((factor - 1) * (tvec[0] * tvec[0]));
      mat[1] = ((factor - 1) * (tvec[0] * tvec[1]));
      mat[2] = ((factor - 1) * (tvec[0] * tvec[1]));
      mat[3] = 1 + ((factor - 1) * (tvec[1] * tvec[1]));
    }
    else {
      mat[0] = 1 + ((factor - 1) * (tvec[0] * tvec[0]));
      mat[1] = ((factor - 1) * (tvec[0] * tvec[1]));
      mat[2] = ((factor - 1) * (tvec[0] * tvec[2]));
      mat[3] = ((factor - 1) * (tvec[0] * tvec[1]));
      mat[4] = 1 + ((factor - 1) * (tvec[1] * tvec[1]));
      mat[5] = ((factor - 1) * (tvec[1] * tvec[2]));
      mat[6] = ((factor - 1) * (tvec[0] * tvec[2]));
      mat[7] = ((factor - 1) * (tvec[1] * tvec[2]));
      mat[8] = 1 + ((factor - 1) * (tvec[2] * tvec[2]));
    }
  }
  if (matSize == 4) {
    matrix_3x3_as_4x4(mat);
  }
  /* pass to matrix creation */
  return Matrix_CreatePyObject(mat, matSize, matSize, (PyTypeObject *)cls);
}
/** Orthographic projection constructor: `mathutils.Matrix.OrthoProjection()`. */
PyDoc_STRVAR(C_Matrix_OrthoProjection_doc,
             ".. classmethod:: OrthoProjection(axis, size)\n"
             "\n"
             "   Create a matrix to represent an orthographic projection.\n"
             "\n"
             "   :arg axis: Can be any of the following: ['X', 'Y', 'XY', 'XZ', 'YZ'],\n"
             "      where a single axis is for a 2D matrix.\n"
             "      Or a vector for an arbitrary axis\n"
             "   :type axis: string or :class:`Vector`\n"
             "   :arg size: The size of the projection matrix to construct [2, 4].\n"
             "   :type size: int\n"
             "   :return: A new projection matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *C_Matrix_OrthoProjection(PyObject *cls, PyObject *args)
{
  PyObject *axis;

  int matSize, x;
  float norm = 0.0f;
  float mat[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  if (!PyArg_ParseTuple(args, "Oi:Matrix.OrthoProjection", &axis, &matSize)) {
    return NULL;
  }
  if (!ELEM(matSize, 2, 3, 4)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.OrthoProjection(): "
                    "can only return a 2x2 3x3 or 4x4 matrix");
    return NULL;
  }

  if (PyUnicode_Check(axis)) { /* ortho projection onto cardinal plane */
    Py_ssize_t plane_len;
    const char *plane = PyUnicode_AsUTF8AndSize(axis, &plane_len);
    if (matSize == 2) {
      if (plane_len == 1 && plane[0] == 'X') {
        mat[0] = 1.0f;
      }
      else if (plane_len == 1 && plane[0] == 'Y') {
        mat[3] = 1.0f;
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "Matrix.OrthoProjection(): "
                     "unknown plane, expected: X, Y, not '%.200s'",
                     plane);
        return NULL;
      }
    }
    else {
      if (plane_len == 2 && plane[0] == 'X' && plane[1] == 'Y') {
        mat[0] = 1.0f;
        mat[4] = 1.0f;
      }
      else if (plane_len == 2 && plane[0] == 'X' && plane[1] == 'Z') {
        mat[0] = 1.0f;
        mat[8] = 1.0f;
      }
      else if (plane_len == 2 && plane[0] == 'Y' && plane[1] == 'Z') {
        mat[4] = 1.0f;
        mat[8] = 1.0f;
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "Matrix.OrthoProjection(): "
                     "unknown plane, expected: XY, XZ, YZ, not '%.200s'",
                     plane);
        return NULL;
      }
    }
  }
  else {
    /* arbitrary plane */

    const int vec_num = (matSize == 2 ? 2 : 3);
    float tvec[4];

    if (mathutils_array_parse(tvec,
                              vec_num,
                              vec_num,
                              axis,
                              "Matrix.OrthoProjection(axis, size), invalid 'axis' arg") == -1) {
      return NULL;
    }

    /* normalize arbitrary axis */
    for (x = 0; x < vec_num; x++) {
      norm += tvec[x] * tvec[x];
    }
    norm = sqrtf(norm);
    for (x = 0; x < vec_num; x++) {
      tvec[x] /= norm;
    }
    if (matSize == 2) {
      mat[0] = 1 - (tvec[0] * tvec[0]);
      mat[1] = -(tvec[0] * tvec[1]);
      mat[2] = -(tvec[0] * tvec[1]);
      mat[3] = 1 - (tvec[1] * tvec[1]);
    }
    else if (matSize > 2) {
      mat[0] = 1 - (tvec[0] * tvec[0]);
      mat[1] = -(tvec[0] * tvec[1]);
      mat[2] = -(tvec[0] * tvec[2]);
      mat[3] = -(tvec[0] * tvec[1]);
      mat[4] = 1 - (tvec[1] * tvec[1]);
      mat[5] = -(tvec[1] * tvec[2]);
      mat[6] = -(tvec[0] * tvec[2]);
      mat[7] = -(tvec[1] * tvec[2]);
      mat[8] = 1 - (tvec[2] * tvec[2]);
    }
  }
  if (matSize == 4) {
    matrix_3x3_as_4x4(mat);
  }
  /* pass to matrix creation */
  return Matrix_CreatePyObject(mat, matSize, matSize, (PyTypeObject *)cls);
}

/** Shear constructor: `mathutils.Matrix.Shear()`. */
PyDoc_STRVAR(C_Matrix_Shear_doc,
             ".. classmethod:: Shear(plane, size, factor)\n"
             "\n"
             "   Create a matrix to represent an shear transformation.\n"
             "\n"
             "   :arg plane: Can be any of the following: ['X', 'Y', 'XY', 'XZ', 'YZ'],\n"
             "      where a single axis is for a 2D matrix only.\n"
             "   :type plane: string\n"
             "   :arg size: The size of the shear matrix to construct [2, 4].\n"
             "   :type size: int\n"
             "   :arg factor: The factor of shear to apply. For a 3 or 4 *size* matrix\n"
             "      pass a pair of floats corresponding with the *plane* axis.\n"
             "   :type factor: float or float pair\n"
             "   :return: A new shear matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *C_Matrix_Shear(PyObject *cls, PyObject *args)
{
  int matSize;
  const char *plane;
  PyObject *fac;
  float mat[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

  if (!PyArg_ParseTuple(args, "siO:Matrix.Shear", &plane, &matSize, &fac)) {
    return NULL;
  }
  if (!ELEM(matSize, 2, 3, 4)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.Shear(): "
                    "can only return a 2x2 3x3 or 4x4 matrix");
    return NULL;
  }

  if (matSize == 2) {
    float const factor = PyFloat_AsDouble(fac);

    if (factor == -1.0f && PyErr_Occurred()) {
      PyErr_SetString(PyExc_TypeError,
                      "Matrix.Shear(): "
                      "the factor to be a float");
      return NULL;
    }

    /* unit */
    mat[0] = 1.0f;
    mat[3] = 1.0f;

    if (STREQ(plane, "X")) {
      mat[2] = factor;
    }
    else if (STREQ(plane, "Y")) {
      mat[1] = factor;
    }
    else {
      PyErr_SetString(PyExc_ValueError,
                      "Matrix.Shear(): "
                      "expected: X, Y or wrong matrix size for shearing plane");
      return NULL;
    }
  }
  else {
    /* 3 or 4, apply as 3x3, resize later if needed */
    float factor[2];

    if (mathutils_array_parse(factor, 2, 2, fac, "Matrix.Shear()") == -1) {
      return NULL;
    }

    /* unit */
    mat[0] = 1.0f;
    mat[4] = 1.0f;
    mat[8] = 1.0f;

    if (STREQ(plane, "XY")) {
      mat[6] = factor[0];
      mat[7] = factor[1];
    }
    else if (STREQ(plane, "XZ")) {
      mat[3] = factor[0];
      mat[5] = factor[1];
    }
    else if (STREQ(plane, "YZ")) {
      mat[1] = factor[0];
      mat[2] = factor[1];
    }
    else {
      PyErr_SetString(PyExc_ValueError,
                      "Matrix.Shear(): "
                      "expected: X, Y, XY, XZ, YZ");
      return NULL;
    }
  }

  if (matSize == 4) {
    matrix_3x3_as_4x4(mat);
  }
  /* pass to matrix creation */
  return Matrix_CreatePyObject(mat, matSize, matSize, (PyTypeObject *)cls);
}

PyDoc_STRVAR(
    C_Matrix_LocRotScale_doc,
    ".. classmethod:: LocRotScale(location, rotation, scale)\n"
    "\n"
    "   Create a matrix combining translation, rotation and scale,\n"
    "   acting as the inverse of the decompose() method.\n"
    "\n"
    "   Any of the inputs may be replaced with None if not needed.\n"
    "\n"
    "   :arg location: The translation component.\n"
    "   :type location: :class:`Vector` or None\n"
    "   :arg rotation: The rotation component.\n"
    "   :type rotation: 3x3 :class:`Matrix`, :class:`Quaternion`, :class:`Euler` or None\n"
    "   :arg scale: The scale component.\n"
    "   :type scale: :class:`Vector` or None\n"
    "   :return: Combined transformation matrix. \n"
    "   :rtype: 4x4 :class:`Matrix`\n");
static PyObject *C_Matrix_LocRotScale(PyObject *cls, PyObject *args)
{
  PyObject *loc_obj, *rot_obj, *scale_obj;
  float mat[4][4], loc[3];

  if (!PyArg_ParseTuple(args, "OOO:Matrix.LocRotScale", &loc_obj, &rot_obj, &scale_obj)) {
    return NULL;
  }

  /* Decode location. */
  if (loc_obj == Py_None) {
    zero_v3(loc);
  }
  else if (mathutils_array_parse(
               loc, 3, 3, loc_obj, "Matrix.LocRotScale(), invalid location argument") == -1) {
    return NULL;
  }

  /* Decode rotation. */
  if (rot_obj == Py_None) {
    unit_m4(mat);
  }
  else if (QuaternionObject_Check(rot_obj)) {
    QuaternionObject *quat_obj = (QuaternionObject *)rot_obj;

    if (BaseMath_ReadCallback(quat_obj) == -1) {
      return NULL;
    }

    quat_to_mat4(mat, quat_obj->quat);
  }
  else if (EulerObject_Check(rot_obj)) {
    EulerObject *eul_obj = (EulerObject *)rot_obj;

    if (BaseMath_ReadCallback(eul_obj) == -1) {
      return NULL;
    }

    eulO_to_mat4(mat, eul_obj->eul, eul_obj->order);
  }
  else if (MatrixObject_Check(rot_obj)) {
    MatrixObject *mat_obj = (MatrixObject *)rot_obj;

    if (BaseMath_ReadCallback(mat_obj) == -1) {
      return NULL;
    }

    if (mat_obj->col_num == 3 && mat_obj->row_num == 3) {
      copy_m4_m3(mat, (const float(*)[3])mat_obj->matrix);
    }
    else {
      PyErr_SetString(PyExc_ValueError,
                      "Matrix.LocRotScale(): "
                      "inappropriate rotation matrix size - expects 3x3 matrix");
      return NULL;
    }
  }
  else {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.LocRotScale(): "
                    "rotation argument must be Matrix, Quaternion, Euler or None");
    return NULL;
  }

  /* Decode scale. */
  if (scale_obj != Py_None) {
    float scale[3];

    if (mathutils_array_parse(
            scale, 3, 3, scale_obj, "Matrix.LocRotScale(), invalid scale argument") == -1) {
      return NULL;
    }

    rescale_m4(mat, scale);
  }

  copy_v3_v3(mat[3], loc);

  return Matrix_CreatePyObject(&mat[0][0], 4, 4, (PyTypeObject *)cls);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: To Quaternion
 * \{ */

PyDoc_STRVAR(Matrix_to_quaternion_doc,
             ".. method:: to_quaternion()\n"
             "\n"
             "   Return a quaternion representation of the rotation matrix.\n"
             "\n"
             "   :return: Quaternion representation of the rotation matrix.\n"
             "   :rtype: :class:`Quaternion`\n");
static PyObject *Matrix_to_quaternion(MatrixObject *self)
{
  float quat[4];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* must be 3-4 cols, 3-4 rows, square matrix */
  if ((self->row_num < 3) || (self->col_num < 3) || (self->row_num != self->col_num)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.to_quat(): "
                    "inappropriate matrix size - expects 3x3 or 4x4 matrix");
    return NULL;
  }
  if (self->row_num == 3) {
    mat3_to_quat(quat, (const float(*)[3])self->matrix);
  }
  else {
    mat4_to_quat(quat, (const float(*)[4])self->matrix);
  }
  return Quaternion_CreatePyObject(quat, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: To Euler
 * \{ */

PyDoc_STRVAR(Matrix_to_euler_doc,
             ".. method:: to_euler(order, euler_compat)\n"
             "\n"
             "   Return an Euler representation of the rotation matrix\n"
             "   (3x3 or 4x4 matrix only).\n"
             "\n"
             "   :arg order: Optional rotation order argument in\n"
             "      ['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX'].\n"
             "   :type order: string\n"
             "   :arg euler_compat: Optional euler argument the new euler will be made\n"
             "      compatible with (no axis flipping between them).\n"
             "      Useful for converting a series of matrices to animation curves.\n"
             "   :type euler_compat: :class:`Euler`\n"
             "   :return: Euler representation of the matrix.\n"
             "   :rtype: :class:`Euler`\n");
static PyObject *Matrix_to_euler(MatrixObject *self, PyObject *args)
{
  const char *order_str = NULL;
  short order = EULER_ORDER_XYZ;
  float eul[3], eul_compatf[3];
  EulerObject *eul_compat = NULL;

  float mat[3][3];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  if (!PyArg_ParseTuple(args, "|sO!:to_euler", &order_str, &euler_Type, &eul_compat)) {
    return NULL;
  }

  if (eul_compat) {
    if (BaseMath_ReadCallback(eul_compat) == -1) {
      return NULL;
    }

    copy_v3_v3(eul_compatf, eul_compat->eul);
  }

  /* Must be 3-4 cols, 3-4 rows, square matrix. */
  if (self->row_num == 3 && self->col_num == 3) {
    copy_m3_m3(mat, (const float(*)[3])self->matrix);
  }
  else if (self->row_num == 4 && self->col_num == 4) {
    copy_m3_m4(mat, (const float(*)[4])self->matrix);
  }
  else {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.to_euler(): "
                    "inappropriate matrix size - expects 3x3 or 4x4 matrix");
    return NULL;
  }

  if (order_str) {
    order = euler_order_from_string(order_str, "Matrix.to_euler()");

    if (order == -1) {
      return NULL;
    }
  }

  normalize_m3(mat);

  if (eul_compat) {
    if (order == 1) {
      mat3_normalized_to_compatible_eul(eul, eul_compatf, mat);
    }
    else {
      mat3_normalized_to_compatible_eulO(eul, eul_compatf, order, mat);
    }
  }
  else {
    if (order == 1) {
      mat3_normalized_to_eul(eul, mat);
    }
    else {
      mat3_normalized_to_eulO(eul, order, mat);
    }
  }

  return Euler_CreatePyObject(eul, order, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Resize
 * \{ */

PyDoc_STRVAR(Matrix_resize_4x4_doc,
             ".. method:: resize_4x4()\n"
             "\n"
             "   Resize the matrix to 4x4.\n");
static PyObject *Matrix_resize_4x4(MatrixObject *self)
{
  float mat[4][4];
  int col;

  if (self->flag & BASE_MATH_FLAG_IS_WRAP) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.resize_4x4(): "
                    "cannot resize wrapped data - make a copy and resize that");
    return NULL;
  }
  if (self->cb_user) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.resize_4x4(): "
                    "cannot resize owned data - make a copy and resize that");
    return NULL;
  }

  self->matrix = PyMem_Realloc(self->matrix, (sizeof(float) * (MATRIX_MAX_DIM * MATRIX_MAX_DIM)));
  if (self->matrix == NULL) {
    PyErr_SetString(PyExc_MemoryError,
                    "Matrix.resize_4x4(): "
                    "problem allocating pointer space");
    return NULL;
  }

  unit_m4(mat);

  for (col = 0; col < self->col_num; col++) {
    memcpy(mat[col], MATRIX_COL_PTR(self, col), self->row_num * sizeof(float));
  }

  copy_m4_m4((float(*)[4])self->matrix, (const float(*)[4])mat);

  self->col_num = 4;
  self->row_num = 4;

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: To NxN
 * \{ */

static PyObject *Matrix_to_NxN(MatrixObject *self, const int col_num, const int row_num)
{
  const int mat_size = sizeof(float) * (col_num * row_num);
  MatrixObject *pymat = (MatrixObject *)Matrix_CreatePyObject_alloc(
      PyMem_Malloc(mat_size), col_num, row_num, Py_TYPE(self));

  if ((self->row_num == row_num) && (self->col_num == col_num)) {
    memcpy(pymat->matrix, self->matrix, mat_size);
  }
  else {
    if ((self->col_num < col_num) || (self->row_num < row_num)) {
      matrix_unit_internal(pymat);
    }
    const int col_len_src = min_ii(col_num, self->col_num);
    const int row_len_src = min_ii(row_num, self->row_num);
    for (int col = 0; col < col_len_src; col++) {
      memcpy(
          &pymat->matrix[col * row_num], MATRIX_COL_PTR(self, col), sizeof(float) * row_len_src);
    }
  }
  return (PyObject *)pymat;
}

PyDoc_STRVAR(Matrix_to_2x2_doc,
             ".. method:: to_2x2()\n"
             "\n"
             "   Return a 2x2 copy of this matrix.\n"
             "\n"
             "   :return: a new matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_to_2x2(MatrixObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }
  return Matrix_to_NxN(self, 2, 2);
}

PyDoc_STRVAR(Matrix_to_3x3_doc,
             ".. method:: to_3x3()\n"
             "\n"
             "   Return a 3x3 copy of this matrix.\n"
             "\n"
             "   :return: a new matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_to_3x3(MatrixObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }
  return Matrix_to_NxN(self, 3, 3);
}

PyDoc_STRVAR(Matrix_to_4x4_doc,
             ".. method:: to_4x4()\n"
             "\n"
             "   Return a 4x4 copy of this matrix.\n"
             "\n"
             "   :return: a new matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_to_4x4(MatrixObject *self)
{

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }
  return Matrix_to_NxN(self, 4, 4);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: To Translation/Scale
 * \{ */

PyDoc_STRVAR(Matrix_to_translation_doc,
             ".. method:: to_translation()\n"
             "\n"
             "   Return the translation part of a 4 row matrix.\n"
             "\n"
             "   :return: Return the translation of a matrix.\n"
             "   :rtype: :class:`Vector`\n");
static PyObject *Matrix_to_translation(MatrixObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  if ((self->row_num < 3) || self->col_num < 4) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.to_translation(): "
                    "inappropriate matrix size");
    return NULL;
  }

  return Vector_CreatePyObject(MATRIX_COL_PTR(self, 3), 3, NULL);
}

PyDoc_STRVAR(Matrix_to_scale_doc,
             ".. method:: to_scale()\n"
             "\n"
             "   Return the scale part of a 3x3 or 4x4 matrix.\n"
             "\n"
             "   :return: Return the scale of a matrix.\n"
             "   :rtype: :class:`Vector`\n"
             "\n"
             "   .. note:: This method does not return a negative scale on any axis because it is "
             "not possible to obtain this data from the matrix alone.\n");
static PyObject *Matrix_to_scale(MatrixObject *self)
{
  float rot[3][3];
  float mat[3][3];
  float size[3];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* Must be 3-4 cols, 3-4 rows, square matrix. */
  if ((self->row_num < 3) || (self->col_num < 3)) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.to_scale(): "
                    "inappropriate matrix size, 3x3 minimum size");
    return NULL;
  }

  matrix_as_3x3(mat, self);

  /* compatible mat4_to_loc_rot_size */
  mat3_to_rot_size(rot, size, mat);

  return Vector_CreatePyObject(size, 3, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Invert
 * \{ */

/** Re-usable checks for invert. */
static bool matrix_invert_is_compat(const MatrixObject *self)
{
  if (self->col_num != self->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.invert(ed): "
                    "only square matrices are supported");
    return false;
  }

  return true;
}

static bool matrix_invert_args_check(const MatrixObject *self, PyObject *args, bool check_type)
{
  switch (PyTuple_GET_SIZE(args)) {
    case 0:
      return true;
    case 1:
      if (check_type) {
        const MatrixObject *fallback = (const MatrixObject *)PyTuple_GET_ITEM(args, 0);
        if (!MatrixObject_Check(fallback)) {
          PyErr_SetString(PyExc_TypeError,
                          "Matrix.invert: "
                          "expects a matrix argument or nothing");
          return false;
        }

        if ((self->col_num != fallback->col_num) || (self->row_num != fallback->row_num)) {
          PyErr_SetString(PyExc_TypeError,
                          "Matrix.invert: "
                          "matrix argument has different dimensions");
          return false;
        }
      }

      return true;
    default:
      PyErr_SetString(PyExc_ValueError,
                      "Matrix.invert(ed): "
                      "takes at most one argument");
      return false;
  }
}

static void matrix_invert_raise_degenerate(void)
{
  PyErr_SetString(PyExc_ValueError,
                  "Matrix.invert(ed): "
                  "matrix does not have an inverse");
}

PyDoc_STRVAR(
    Matrix_invert_doc,
    ".. method:: invert(fallback=None)\n"
    "\n"
    "   Set the matrix to its inverse.\n"
    "\n"
    "   :arg fallback: Set the matrix to this value when the inverse cannot be calculated\n"
    "      (instead of raising a :exc:`ValueError` exception).\n"
    "   :type fallback: :class:`Matrix`\n"
    "\n"
    "   .. seealso:: `Inverse matrix <https://en.wikipedia.org/wiki/Inverse_matrix>`__ on "
    "Wikipedia.\n");
static PyObject *Matrix_invert(MatrixObject *self, PyObject *args)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (matrix_invert_is_compat(self) == false) {
    return NULL;
  }

  if (matrix_invert_args_check(self, args, true) == false) {
    return NULL;
  }

  if (matrix_invert_internal(self, self->matrix)) {
    /* pass */
  }
  else {
    if (PyTuple_GET_SIZE(args) == 1) {
      MatrixObject *fallback = (MatrixObject *)PyTuple_GET_ITEM(args, 0);

      if (BaseMath_ReadCallback(fallback) == -1) {
        return NULL;
      }

      if (self != fallback) {
        matrix_copy(self, fallback);
      }
    }
    else {
      matrix_invert_raise_degenerate();
      return NULL;
    }
  }

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Matrix_inverted_doc,
             ".. method:: inverted(fallback=None)\n"
             "\n"
             "   Return an inverted copy of the matrix.\n"
             "\n"
             "   :arg fallback: return this when the inverse can't be calculated\n"
             "      (instead of raising a :exc:`ValueError`).\n"
             "   :type fallback: any\n"
             "   :return: the inverted matrix or fallback when given.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_inverted(MatrixObject *self, PyObject *args)
{
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  if (matrix_invert_args_check(self, args, false) == false) {
    return NULL;
  }

  if (matrix_invert_is_compat(self) == false) {
    return NULL;
  }

  if (matrix_invert_internal(self, mat)) {
    /* pass */
  }
  else {
    if (PyTuple_GET_SIZE(args) == 1) {
      PyObject *fallback = PyTuple_GET_ITEM(args, 0);
      Py_INCREF(fallback);
      return fallback;
    }

    matrix_invert_raise_degenerate();
    return NULL;
  }

  return Matrix_copy_notest(self, mat);
}

static PyObject *Matrix_inverted_noargs(MatrixObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (matrix_invert_is_compat(self) == false) {
    return NULL;
  }

  if (matrix_invert_internal(self, self->matrix)) {
    /* pass */
  }
  else {
    matrix_invert_raise_degenerate();
    return NULL;
  }

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    Matrix_invert_safe_doc,
    ".. method:: invert_safe()\n"
    "\n"
    "   Set the matrix to its inverse, will never error.\n"
    "   If degenerated (e.g. zero scale on an axis), add some epsilon to its diagonal, "
    "to get an invertible one.\n"
    "   If tweaked matrix is still degenerated, set to the identity matrix instead.\n"
    "\n"
    "   .. seealso:: `Inverse Matrix <https://en.wikipedia.org/wiki/Inverse_matrix>`__ on "
    "Wikipedia.\n");
static PyObject *Matrix_invert_safe(MatrixObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (matrix_invert_is_compat(self) == false) {
    return NULL;
  }

  matrix_invert_safe_internal(self, self->matrix);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Matrix_inverted_safe_doc,
             ".. method:: inverted_safe()\n"
             "\n"
             "   Return an inverted copy of the matrix, will never error.\n"
             "   If degenerated (e.g. zero scale on an axis), add some epsilon to its diagonal, "
             "to get an invertible one.\n"
             "   If tweaked matrix is still degenerated, return the identity matrix instead.\n"
             "\n"
             "   :return: the inverted matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_inverted_safe(MatrixObject *self)
{
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  if (matrix_invert_is_compat(self) == false) {
    return NULL;
  }

  matrix_invert_safe_internal(self, mat);

  return Matrix_copy_notest(self, mat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Adjugate
 * \{ */

PyDoc_STRVAR(
    Matrix_adjugate_doc,
    ".. method:: adjugate()\n"
    "\n"
    "   Set the matrix to its adjugate.\n"
    "\n"
    "   :raises ValueError: if the matrix cannot be adjugate.\n"
    "\n"
    "   .. seealso:: `Adjugate matrix <https://en.wikipedia.org/wiki/Adjugate_matrix>`__ on "
    "Wikipedia.\n");
static PyObject *Matrix_adjugate(MatrixObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (self->col_num != self->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.adjugate(d): "
                    "only square matrices are supported");
    return NULL;
  }

  /* calculate the classical adjoint */
  if (self->col_num <= 4) {
    adjoint_matrix_n(self->matrix, self->matrix, self->col_num);
  }
  else {
    PyErr_Format(
        PyExc_ValueError, "Matrix adjugate(d): size (%d) unsupported", (int)self->col_num);
    return NULL;
  }

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Matrix_adjugated_doc,
             ".. method:: adjugated()\n"
             "\n"
             "   Return an adjugated copy of the matrix.\n"
             "\n"
             "   :return: the adjugated matrix.\n"
             "   :rtype: :class:`Matrix`\n"
             "   :raises ValueError: if the matrix cannot be adjugated\n");
static PyObject *Matrix_adjugated(MatrixObject *self)
{
  return matrix__apply_to_copy(Matrix_adjugate, self);
}

PyDoc_STRVAR(
    Matrix_rotate_doc,
    ".. method:: rotate(other)\n"
    "\n"
    "   Rotates the matrix by another mathutils value.\n"
    "\n"
    "   :arg other: rotation component of mathutils value\n"
    "   :type other: :class:`Euler`, :class:`Quaternion` or :class:`Matrix`\n"
    "\n"
    "   .. note:: If any of the columns are not unit length this may not have desired results.\n");
static PyObject *Matrix_rotate(MatrixObject *self, PyObject *value)
{
  float self_rmat[3][3], other_rmat[3][3], rmat[3][3];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (mathutils_any_to_rotmat(other_rmat, value, "matrix.rotate(value)") == -1) {
    return NULL;
  }

  if (self->row_num != 3 || self->col_num != 3) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.rotate(): "
                    "must have 3x3 dimensions");
    return NULL;
  }

  matrix_as_3x3(self_rmat, self);
  mul_m3_m3m3(rmat, other_rmat, self_rmat);

  copy_m3_m3((float(*)[3])(self->matrix), rmat);

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Decompose
 * \{ */

PyDoc_STRVAR(Matrix_decompose_doc,
             ".. method:: decompose()\n"
             "\n"
             "   Return the translation, rotation, and scale components of this matrix.\n"
             "\n"
             "   :return: tuple of translation, rotation, and scale\n"
             "   :rtype: (:class:`Vector`, :class:`Quaternion`, :class:`Vector`)");
static PyObject *Matrix_decompose(MatrixObject *self)
{
  PyObject *ret;
  float loc[3];
  float rot[3][3];
  float quat[4];
  float size[3];

  if (self->row_num != 4 || self->col_num != 4) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.decompose(): "
                    "inappropriate matrix size - expects 4x4 matrix");
    return NULL;
  }

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  mat4_to_loc_rot_size(loc, rot, size, (const float(*)[4])self->matrix);
  mat3_normalized_to_quat_fast(quat, rot);

  ret = PyTuple_New(3);
  PyTuple_SET_ITEMS(ret,
                    Vector_CreatePyObject(loc, 3, NULL),
                    Quaternion_CreatePyObject(quat, NULL),
                    Vector_CreatePyObject(size, 3, NULL));
  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Linear Interpolate (lerp)
 * \{ */

PyDoc_STRVAR(Matrix_lerp_doc,
             ".. function:: lerp(other, factor)\n"
             "\n"
             "   Returns the interpolation of two matrices. Uses polar decomposition, see"
             "   \"Matrix Animation and Polar Decomposition\", Shoemake and Duff, 1992.\n"
             "\n"
             "   :arg other: value to interpolate with.\n"
             "   :type other: :class:`Matrix`\n"
             "   :arg factor: The interpolation value in [0.0, 1.0].\n"
             "   :type factor: float\n"
             "   :return: The interpolated matrix.\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_lerp(MatrixObject *self, PyObject *args)
{
  MatrixObject *mat2 = NULL;
  float fac, mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];

  if (!PyArg_ParseTuple(args, "O!f:lerp", &matrix_Type, &mat2, &fac)) {
    return NULL;
  }

  if (self->col_num != mat2->col_num || self->row_num != mat2->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.lerp(): "
                    "expects both matrix objects of the same dimensions");
    return NULL;
  }

  if (BaseMath_ReadCallback(self) == -1 || BaseMath_ReadCallback(mat2) == -1) {
    return NULL;
  }

  /* TODO: different sized matrix. */
  if (self->col_num == 4 && self->row_num == 4) {
#ifdef MATH_STANDALONE
    blend_m4_m4m4((float(*)[4])mat, (float(*)[4])self->matrix, (float(*)[4])mat2->matrix, fac);
#else
    interp_m4_m4m4((float(*)[4])mat, (float(*)[4])self->matrix, (float(*)[4])mat2->matrix, fac);
#endif
  }
  else if (self->col_num == 3 && self->row_num == 3) {
#ifdef MATH_STANDALONE
    blend_m3_m3m3((float(*)[3])mat, (float(*)[3])self->matrix, (float(*)[3])mat2->matrix, fac);
#else
    interp_m3_m3m3((float(*)[3])mat, (float(*)[3])self->matrix, (float(*)[3])mat2->matrix, fac);
#endif
  }
  else {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.lerp(): "
                    "only 3x3 and 4x4 matrices supported");
    return NULL;
  }

  return Matrix_CreatePyObject(mat, self->col_num, self->row_num, Py_TYPE(self));
}

PyDoc_STRVAR(
    Matrix_determinant_doc,
    ".. method:: determinant()\n"
    "\n"
    "   Return the determinant of a matrix.\n"
    "\n"
    "   :return: Return the determinant of a matrix.\n"
    "   :rtype: float\n"
    "\n"
    "   .. seealso:: `Determinant <https://en.wikipedia.org/wiki/Determinant>`__ on Wikipedia.\n");
static PyObject *Matrix_determinant(MatrixObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  if (self->col_num != self->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.determinant(): "
                    "only square matrices are supported");
    return NULL;
  }

  return PyFloat_FromDouble((double)matrix_determinant_internal(self));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Transpose
 * \{ */

PyDoc_STRVAR(
    Matrix_transpose_doc,
    ".. method:: transpose()\n"
    "\n"
    "   Set the matrix to its transpose.\n"
    "\n"
    "   .. seealso:: `Transpose <https://en.wikipedia.org/wiki/Transpose>`__ on Wikipedia.\n");
static PyObject *Matrix_transpose(MatrixObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (self->col_num != self->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.transpose(d): "
                    "only square matrices are supported");
    return NULL;
  }

  if (self->col_num == 2) {
    const float t = MATRIX_ITEM(self, 1, 0);
    MATRIX_ITEM(self, 1, 0) = MATRIX_ITEM(self, 0, 1);
    MATRIX_ITEM(self, 0, 1) = t;
  }
  else if (self->col_num == 3) {
    transpose_m3((float(*)[3])self->matrix);
  }
  else {
    transpose_m4((float(*)[4])self->matrix);
  }

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Matrix_transposed_doc,
             ".. method:: transposed()\n"
             "\n"
             "   Return a new, transposed matrix.\n"
             "\n"
             "   :return: a transposed matrix\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_transposed(MatrixObject *self)
{
  return matrix__apply_to_copy(Matrix_transpose, self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Normalize
 * \{ */

PyDoc_STRVAR(Matrix_normalize_doc,
             ".. method:: normalize()\n"
             "\n"
             "   Normalize each of the matrix columns.\n");
static PyObject *Matrix_normalize(MatrixObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (self->col_num != self->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.normalize(): "
                    "only square matrices are supported");
    return NULL;
  }

  if (self->col_num == 3) {
    normalize_m3((float(*)[3])self->matrix);
  }
  else if (self->col_num == 4) {
    normalize_m4((float(*)[4])self->matrix);
  }
  else {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.normalize(): "
                    "can only use a 3x3 or 4x4 matrix");
  }

  (void)BaseMath_WriteCallback(self);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Matrix_normalized_doc,
             ".. method:: normalized()\n"
             "\n"
             "   Return a column normalized matrix\n"
             "\n"
             "   :return: a column normalized matrix\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_normalized(MatrixObject *self)
{
  return matrix__apply_to_copy(Matrix_normalize, self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Zero
 * \{ */

PyDoc_STRVAR(Matrix_zero_doc,
             ".. method:: zero()\n"
             "\n"
             "   Set all the matrix values to zero.\n"
             "\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_zero(MatrixObject *self)
{
  if (BaseMath_Prepare_ForWrite(self) == -1) {
    return NULL;
  }

  copy_vn_fl(self->matrix, self->col_num * self->row_num, 0.0f);

  if (BaseMath_WriteCallback(self) == -1) {
    return NULL;
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Set Identity
 * \{ */

static void matrix_identity_internal(MatrixObject *self)
{
  BLI_assert((self->col_num == self->row_num) && (self->row_num <= 4));

  if (self->col_num == 2) {
    unit_m2((float(*)[2])self->matrix);
  }
  else if (self->col_num == 3) {
    unit_m3((float(*)[3])self->matrix);
  }
  else {
    unit_m4((float(*)[4])self->matrix);
  }
}

PyDoc_STRVAR(Matrix_identity_doc,
             ".. method:: identity()\n"
             "\n"
             "   Set the matrix to the identity matrix.\n"
             "\n"
             "   .. note:: An object with a location and rotation of zero, and a scale of one\n"
             "      will have an identity matrix.\n"
             "\n"
             "   .. seealso:: `Identity matrix <https://en.wikipedia.org/wiki/Identity_matrix>`__ "
             "on Wikipedia.\n");
static PyObject *Matrix_identity(MatrixObject *self)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (self->col_num != self->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix.identity(): "
                    "only square matrices are supported");
    return NULL;
  }

  matrix_identity_internal(self);

  if (BaseMath_WriteCallback(self) == -1) {
    return NULL;
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Methods: Copy
 * \{ */

/** Copy `Matrix.copy()` */
static PyObject *Matrix_copy_notest(MatrixObject *self, const float *matrix)
{
  return Matrix_CreatePyObject((const float *)matrix, self->col_num, self->row_num, Py_TYPE(self));
}

PyDoc_STRVAR(Matrix_copy_doc,
             ".. method:: copy()\n"
             "\n"
             "   Returns a copy of this matrix.\n"
             "\n"
             "   :return: an instance of itself\n"
             "   :rtype: :class:`Matrix`\n");
static PyObject *Matrix_copy(MatrixObject *self)
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  return Matrix_copy_notest(self, self->matrix);
}

/** Deep-copy `Matrix.deepcopy()` */
static PyObject *Matrix_deepcopy(MatrixObject *self, PyObject *args)
{
  if (!PyC_CheckArgs_DeepCopy(args)) {
    return NULL;
  }
  return Matrix_copy(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: `__repr__` & `__str__`
 * \{ */

static PyObject *Matrix_repr(MatrixObject *self)
{
  int col, row;
  PyObject *rows[MATRIX_MAX_DIM] = {NULL};

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  for (row = 0; row < self->row_num; row++) {
    rows[row] = PyTuple_New(self->col_num);
    for (col = 0; col < self->col_num; col++) {
      PyTuple_SET_ITEM(rows[row], col, PyFloat_FromDouble(MATRIX_ITEM(self, row, col)));
    }
  }
  switch (self->row_num) {
    case 2:
      return PyUnicode_FromFormat(
          "Matrix((%R,\n"
          "        %R))",
          rows[0],
          rows[1]);

    case 3:
      return PyUnicode_FromFormat(
          "Matrix((%R,\n"
          "        %R,\n"
          "        %R))",
          rows[0],
          rows[1],
          rows[2]);

    case 4:
      return PyUnicode_FromFormat(
          "Matrix((%R,\n"
          "        %R,\n"
          "        %R,\n"
          "        %R))",
          rows[0],
          rows[1],
          rows[2],
          rows[3]);
  }

  Py_FatalError("Matrix(): invalid row size!");
  return NULL;
}

#ifndef MATH_STANDALONE
static PyObject *Matrix_str(MatrixObject *self)
{
  DynStr *ds;

  int maxsize[MATRIX_MAX_DIM];
  int row, col;

  char dummy_buf[64];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  ds = BLI_dynstr_new();

  /* First determine the maximum width for each column */
  for (col = 0; col < self->col_num; col++) {
    maxsize[col] = 0;
    for (row = 0; row < self->row_num; row++) {
      const int size = BLI_snprintf_rlen(
          dummy_buf, sizeof(dummy_buf), "%.4f", MATRIX_ITEM(self, row, col));
      maxsize[col] = max_ii(maxsize[col], size);
    }
  }

  /* Now write the unicode string to be printed */
  BLI_dynstr_appendf(ds, "<Matrix %dx%d (", self->row_num, self->col_num);
  for (row = 0; row < self->row_num; row++) {
    for (col = 0; col < self->col_num; col++) {
      BLI_dynstr_appendf(ds, col ? ", %*.4f" : "%*.4f", maxsize[col], MATRIX_ITEM(self, row, col));
    }
    BLI_dynstr_append(ds, row + 1 != self->row_num ? ")\n            (" : ")");
  }
  BLI_dynstr_append(ds, ">");

  return mathutils_dynstr_to_py(ds); /* frees ds */
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Rich Compare
 * \{ */

static PyObject *Matrix_richcmpr(PyObject *a, PyObject *b, int op)
{
  PyObject *res;
  int ok = -1; /* zero is true */

  if (MatrixObject_Check(a) && MatrixObject_Check(b)) {
    MatrixObject *matA = (MatrixObject *)a;
    MatrixObject *matB = (MatrixObject *)b;

    if (BaseMath_ReadCallback(matA) == -1 || BaseMath_ReadCallback(matB) == -1) {
      return NULL;
    }

    ok = ((matA->row_num == matB->row_num) && (matA->col_num == matB->col_num) &&
          EXPP_VectorsAreEqual(matA->matrix, matB->matrix, (matA->col_num * matA->row_num), 1)) ?
             0 :
             -1;
  }

  switch (op) {
    case Py_NE:
      ok = !ok;
      ATTR_FALLTHROUGH;
    case Py_EQ:
      res = ok ? Py_False : Py_True;
      break;

    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
      res = Py_NotImplemented;
      break;
    default:
      PyErr_BadArgument();
      return NULL;
  }

  return Py_INCREF_RET(res);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Hash (`__hash__`)
 * \{ */

static Py_hash_t Matrix_hash(MatrixObject *self)
{
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];

  if (BaseMath_ReadCallback(self) == -1) {
    return -1;
  }

  if (BaseMathObject_Prepare_ForHash(self) == -1) {
    return -1;
  }

  matrix_transpose_internal(mat, self);

  return mathutils_array_hash(mat, self->row_num * self->col_num);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Sequence & Mapping Protocol Implementation
 * \{ */

/** Sequence length: `len(object)`. */
static int Matrix_len(MatrixObject *self)
{
  return self->row_num;
}

/**
 * Sequence accessor (get): `x = object[i]`.
 * \note the wrapped vector gives direct access to the matrix data.
 */
static PyObject *Matrix_item_row(MatrixObject *self, int row)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (row < 0 || row >= self->row_num) {
    PyErr_SetString(PyExc_IndexError,
                    "matrix[attribute]: "
                    "array index out of range");
    return NULL;
  }
  return Vector_CreatePyObject_cb(
      (PyObject *)self, self->col_num, mathutils_matrix_row_cb_index, row);
}
/**
 * Sequence accessor (get): `x = object.col[i]`.
 * \note the wrapped vector gives direct access to the matrix data.
 */
static PyObject *Matrix_item_col(MatrixObject *self, int col)
{
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return NULL;
  }

  if (col < 0 || col >= self->col_num) {
    PyErr_SetString(PyExc_IndexError,
                    "matrix[attribute]: "
                    "array index out of range");
    return NULL;
  }
  return Vector_CreatePyObject_cb(
      (PyObject *)self, self->row_num, mathutils_matrix_col_cb_index, col);
}

/** Sequence accessor (set): `object[i] = x`. */
static int Matrix_ass_item_row(MatrixObject *self, int row, PyObject *value)
{
  int col;
  float vec[MATRIX_MAX_DIM];
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  if (row >= self->row_num || row < 0) {
    PyErr_SetString(PyExc_IndexError, "matrix[attribute] = x: bad row");
    return -1;
  }

  if (mathutils_array_parse(
          vec, self->col_num, self->col_num, value, "matrix[i] = value assignment") == -1) {
    return -1;
  }

  /* Since we are assigning a row we cannot memcpy */
  for (col = 0; col < self->col_num; col++) {
    MATRIX_ITEM(self, row, col) = vec[col];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

/** Sequence accessor (set): `object.col[i] = x`. */
static int Matrix_ass_item_col(MatrixObject *self, int col, PyObject *value)
{
  int row;
  float vec[MATRIX_MAX_DIM];
  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  if (col >= self->col_num || col < 0) {
    PyErr_SetString(PyExc_IndexError, "matrix[attribute] = x: bad col");
    return -1;
  }

  if (mathutils_array_parse(
          vec, self->row_num, self->row_num, value, "matrix[i] = value assignment") == -1) {
    return -1;
  }

  /* Since we are assigning a row we cannot memcpy */
  for (row = 0; row < self->row_num; row++) {
    MATRIX_ITEM(self, row, col) = vec[row];
  }

  (void)BaseMath_WriteCallback(self);
  return 0;
}

/** Sequence slice accessor (get): `x = object[i:j]`. */
static PyObject *Matrix_slice(MatrixObject *self, int begin, int end)
{

  PyObject *tuple;
  int count;

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  CLAMP(begin, 0, self->row_num);
  CLAMP(end, 0, self->row_num);
  begin = MIN2(begin, end);

  tuple = PyTuple_New(end - begin);
  for (count = begin; count < end; count++) {
    PyTuple_SET_ITEM(tuple,
                     count - begin,
                     Vector_CreatePyObject_cb(
                         (PyObject *)self, self->col_num, mathutils_matrix_row_cb_index, count));
  }

  return tuple;
}

/** Sequence slice accessor (set): `object[i:j] = x`. */
static int Matrix_ass_slice(MatrixObject *self, int begin, int end, PyObject *value)
{
  PyObject *value_fast;

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  CLAMP(begin, 0, self->row_num);
  CLAMP(end, 0, self->row_num);
  begin = MIN2(begin, end);

  /* non list/tuple cases */
  if (!(value_fast = PySequence_Fast(value, "matrix[begin:end] = value"))) {
    /* PySequence_Fast sets the error */
    return -1;
  }

  PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
  const int size = end - begin;
  int row, col;
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];
  float vec[4];

  if (PySequence_Fast_GET_SIZE(value_fast) != size) {
    Py_DECREF(value_fast);
    PyErr_SetString(PyExc_ValueError,
                    "matrix[begin:end] = []: "
                    "size mismatch in slice assignment");
    return -1;
  }

  memcpy(mat, self->matrix, self->col_num * self->row_num * sizeof(float));

  /* parse sub items */
  for (row = begin; row < end; row++) {
    /* parse each sub sequence */
    PyObject *item = value_fast_items[row - begin];

    if (mathutils_array_parse(
            vec, self->col_num, self->col_num, item, "matrix[begin:end] = value assignment") ==
        -1) {
      Py_DECREF(value_fast);
      return -1;
    }

    for (col = 0; col < self->col_num; col++) {
      mat[col * self->row_num + row] = vec[col];
    }
  }

  Py_DECREF(value_fast);

  /* Parsed well - now set in matrix. */
  memcpy(self->matrix, mat, self->col_num * self->row_num * sizeof(float));

  (void)BaseMath_WriteCallback(self);
  return 0;
}

/** Sequence generic subscript (get): `x = object[...]`. */
static PyObject *Matrix_subscript(MatrixObject *self, PyObject *item)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return NULL;
    }
    if (i < 0) {
      i += self->row_num;
    }
    return Matrix_item_row(self, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->row_num, &start, &stop, &step, &slicelength) < 0) {
      return NULL;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return Matrix_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with matrices");
    return NULL;
  }

  PyErr_Format(
      PyExc_TypeError, "matrix indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return NULL;
}

/** Sequence generic subscript (set): `object[...] = x`. */
static int Matrix_ass_subscript(MatrixObject *self, PyObject *item, PyObject *value)
{
  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }
    if (i < 0) {
      i += self->row_num;
    }
    return Matrix_ass_item_row(self, i, value);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, self->row_num, &start, &stop, &step, &slicelength) < 0) {
      return -1;
    }

    if (step == 1) {
      return Matrix_ass_slice(self, start, stop, value);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with matrices");
    return -1;
  }

  PyErr_Format(
      PyExc_TypeError, "matrix indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Numeric Protocol Implementation
 * \{ */

/** Addition: `object + object`. */
static PyObject *Matrix_add(PyObject *m1, PyObject *m2)
{
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];
  MatrixObject *mat1 = NULL, *mat2 = NULL;

  mat1 = (MatrixObject *)m1;
  mat2 = (MatrixObject *)m2;

  if (!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
    PyErr_Format(PyExc_TypeError,
                 "Matrix addition: (%s + %s) "
                 "invalid type for this operation",
                 Py_TYPE(m1)->tp_name,
                 Py_TYPE(m2)->tp_name);
    return NULL;
  }

  if (BaseMath_ReadCallback(mat1) == -1 || BaseMath_ReadCallback(mat2) == -1) {
    return NULL;
  }

  if (mat1->col_num != mat2->col_num || mat1->row_num != mat2->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix addition: "
                    "matrices must have the same dimensions for this operation");
    return NULL;
  }

  add_vn_vnvn(mat, mat1->matrix, mat2->matrix, mat1->col_num * mat1->row_num);

  return Matrix_CreatePyObject(mat, mat1->col_num, mat1->row_num, Py_TYPE(mat1));
}

/** Subtraction: `object - object`. */
static PyObject *Matrix_sub(PyObject *m1, PyObject *m2)
{
  float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];
  MatrixObject *mat1 = NULL, *mat2 = NULL;

  mat1 = (MatrixObject *)m1;
  mat2 = (MatrixObject *)m2;

  if (!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
    PyErr_Format(PyExc_TypeError,
                 "Matrix subtraction: (%s - %s) "
                 "invalid type for this operation",
                 Py_TYPE(m1)->tp_name,
                 Py_TYPE(m2)->tp_name);
    return NULL;
  }

  if (BaseMath_ReadCallback(mat1) == -1 || BaseMath_ReadCallback(mat2) == -1) {
    return NULL;
  }

  if (mat1->col_num != mat2->col_num || mat1->row_num != mat2->row_num) {
    PyErr_SetString(PyExc_ValueError,
                    "Matrix addition: "
                    "matrices must have the same dimensions for this operation");
    return NULL;
  }

  sub_vn_vnvn(mat, mat1->matrix, mat2->matrix, mat1->col_num * mat1->row_num);

  return Matrix_CreatePyObject(mat, mat1->col_num, mat1->row_num, Py_TYPE(mat1));
}

/** Multiplication (element-wise): `object * object`. */
static PyObject *matrix_mul_float(MatrixObject *mat, const float scalar)
{
  float tmat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];
  mul_vn_vn_fl(tmat, mat->matrix, mat->col_num * mat->row_num, scalar);
  return Matrix_CreatePyObject(tmat, mat->col_num, mat->row_num, Py_TYPE(mat));
}

static PyObject *Matrix_mul(PyObject *m1, PyObject *m2)
{
  float scalar;

  MatrixObject *mat1 = NULL, *mat2 = NULL;

  if (MatrixObject_Check(m1)) {
    mat1 = (MatrixObject *)m1;
    if (BaseMath_ReadCallback(mat1) == -1) {
      return NULL;
    }
  }
  if (MatrixObject_Check(m2)) {
    mat2 = (MatrixObject *)m2;
    if (BaseMath_ReadCallback(mat2) == -1) {
      return NULL;
    }
  }

  if (mat1 && mat2) {
    /* MATRIX * MATRIX */
    float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];

    if ((mat1->row_num != mat2->row_num) || (mat1->col_num != mat2->col_num)) {
      PyErr_SetString(PyExc_ValueError,
                      "matrix1 * matrix2: matrix1 number of rows/columns "
                      "and the matrix2 number of rows/columns must be the same");
      return NULL;
    }

    mul_vn_vnvn(mat, mat1->matrix, mat2->matrix, mat1->col_num * mat1->row_num);

    return Matrix_CreatePyObject(mat, mat2->col_num, mat1->row_num, Py_TYPE(mat1));
  }
  if (mat2) {
    /* FLOAT/INT * MATRIX */
    if (((scalar = PyFloat_AsDouble(m1)) == -1.0f && PyErr_Occurred()) == 0) {
      return matrix_mul_float(mat2, scalar);
    }
  }
  else if (mat1) {
    /* MATRIX * FLOAT/INT */
    if (((scalar = PyFloat_AsDouble(m2)) == -1.0f && PyErr_Occurred()) == 0) {
      return matrix_mul_float(mat1, scalar);
    }
  }

  PyErr_Format(PyExc_TypeError,
               "Element-wise multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(m1)->tp_name,
               Py_TYPE(m2)->tp_name);
  return NULL;
}

/** Multiplication in-place (element-wise): `object *= object`. */
static PyObject *Matrix_imul(PyObject *m1, PyObject *m2)
{
  float scalar;

  MatrixObject *mat1 = NULL, *mat2 = NULL;

  if (MatrixObject_Check(m1)) {
    mat1 = (MatrixObject *)m1;
    if (BaseMath_ReadCallback(mat1) == -1) {
      return NULL;
    }
  }
  if (MatrixObject_Check(m2)) {
    mat2 = (MatrixObject *)m2;
    if (BaseMath_ReadCallback(mat2) == -1) {
      return NULL;
    }
  }

  if (mat1 && mat2) {
    /* MATRIX *= MATRIX */
    if ((mat1->row_num != mat2->row_num) || (mat1->col_num != mat2->col_num)) {
      PyErr_SetString(PyExc_ValueError,
                      "matrix1 *= matrix2: matrix1 number of rows/columns "
                      "and the matrix2 number of rows/columns must be the same");
      return NULL;
    }

    mul_vn_vn(mat1->matrix, mat2->matrix, mat1->col_num * mat1->row_num);
  }
  else if (mat1 && (((scalar = PyFloat_AsDouble(m2)) == -1.0f && PyErr_Occurred()) == 0)) {
    /* MATRIX *= FLOAT/INT */
    mul_vn_fl(mat1->matrix, mat1->row_num * mat1->col_num, scalar);
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "In place element-wise multiplication: "
                 "not supported between '%.200s' and '%.200s' types",
                 Py_TYPE(m1)->tp_name,
                 Py_TYPE(m2)->tp_name);
    return NULL;
  }

  (void)BaseMath_WriteCallback(mat1);
  Py_INCREF(m1);
  return m1;
}

/** Multiplication (matrix multiply): `object @ object`. */
static PyObject *Matrix_matmul(PyObject *m1, PyObject *m2)
{
  int vec_num;

  MatrixObject *mat1 = NULL, *mat2 = NULL;

  if (MatrixObject_Check(m1)) {
    mat1 = (MatrixObject *)m1;
    if (BaseMath_ReadCallback(mat1) == -1) {
      return NULL;
    }
  }
  if (MatrixObject_Check(m2)) {
    mat2 = (MatrixObject *)m2;
    if (BaseMath_ReadCallback(mat2) == -1) {
      return NULL;
    }
  }

  if (mat1 && mat2) {
    /* MATRIX @ MATRIX */
    float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];

    int col, row, item;

    if (mat1->col_num != mat2->row_num) {
      PyErr_SetString(PyExc_ValueError,
                      "matrix1 * matrix2: matrix1 number of columns "
                      "and the matrix2 number of rows must be the same");
      return NULL;
    }

    for (col = 0; col < mat2->col_num; col++) {
      for (row = 0; row < mat1->row_num; row++) {
        double dot = 0.0f;
        for (item = 0; item < mat1->col_num; item++) {
          dot += (double)(MATRIX_ITEM(mat1, row, item) * MATRIX_ITEM(mat2, item, col));
        }
        mat[(col * mat1->row_num) + row] = (float)dot;
      }
    }

    return Matrix_CreatePyObject(mat, mat2->col_num, mat1->row_num, Py_TYPE(mat1));
  }
  if (mat1) {
    /* MATRIX @ VECTOR */
    if (VectorObject_Check(m2)) {
      VectorObject *vec2 = (VectorObject *)m2;
      float tvec[MATRIX_MAX_DIM];
      if (BaseMath_ReadCallback(vec2) == -1) {
        return NULL;
      }
      if (column_vector_multiplication(tvec, vec2, mat1) == -1) {
        return NULL;
      }

      if (mat1->col_num == 4 && vec2->vec_num == 3) {
        vec_num = 3;
      }
      else {
        vec_num = mat1->row_num;
      }

      return Vector_CreatePyObject(tvec, vec_num, Py_TYPE(m2));
    }
  }

  PyErr_Format(PyExc_TypeError,
               "Matrix multiplication: "
               "not supported between '%.200s' and '%.200s' types",
               Py_TYPE(m1)->tp_name,
               Py_TYPE(m2)->tp_name);
  return NULL;
}

/** Multiplication in-place (matrix multiply): `object @= object`. */
static PyObject *Matrix_imatmul(PyObject *m1, PyObject *m2)
{
  MatrixObject *mat1 = NULL, *mat2 = NULL;

  if (MatrixObject_Check(m1)) {
    mat1 = (MatrixObject *)m1;
    if (BaseMath_ReadCallback(mat1) == -1) {
      return NULL;
    }
  }
  if (MatrixObject_Check(m2)) {
    mat2 = (MatrixObject *)m2;
    if (BaseMath_ReadCallback(mat2) == -1) {
      return NULL;
    }
  }

  if (mat1 && mat2) {
    /* MATRIX @= MATRIX */
    float mat[MATRIX_MAX_DIM * MATRIX_MAX_DIM];
    int col, row, item;

    if (mat1->col_num != mat2->row_num) {
      PyErr_SetString(PyExc_ValueError,
                      "matrix1 * matrix2: matrix1 number of columns "
                      "and the matrix2 number of rows must be the same");
      return NULL;
    }

    for (col = 0; col < mat2->col_num; col++) {
      for (row = 0; row < mat1->row_num; row++) {
        double dot = 0.0f;
        for (item = 0; item < mat1->col_num; item++) {
          dot += (double)(MATRIX_ITEM(mat1, row, item) * MATRIX_ITEM(mat2, item, col));
        }
        /* store in new matrix as overwriting original at this point will cause
         * subsequent iterations to use incorrect values */
        mat[(col * mat1->row_num) + row] = (float)dot;
      }
    }

    /* copy matrix back */
    memcpy(mat1->matrix, mat, (mat1->row_num * mat1->col_num) * sizeof(float));
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "In place matrix multiplication: "
                 "not supported between '%.200s' and '%.200s' types",
                 Py_TYPE(m1)->tp_name,
                 Py_TYPE(m2)->tp_name);
    return NULL;
  }

  (void)BaseMath_WriteCallback(mat1);
  Py_INCREF(m1);
  return m1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Protocol Declarations
 * \{ */

static PySequenceMethods Matrix_SeqMethods = {
    (lenfunc)Matrix_len,                  /*sq_length*/
    (binaryfunc)NULL,                     /*sq_concat*/
    (ssizeargfunc)NULL,                   /*sq_repeat*/
    (ssizeargfunc)Matrix_item_row,        /*sq_item*/
    (ssizessizeargfunc)NULL,              /*sq_slice(DEPRECATED)*/
    (ssizeobjargproc)Matrix_ass_item_row, /*sq_ass_item*/
    (ssizessizeobjargproc)NULL,           /*sq_ass_slice(DEPRECATED)*/
    (objobjproc)NULL,                     /*sq_contains*/
    (binaryfunc)NULL,                     /*sq_inplace_concat*/
    (ssizeargfunc)NULL,                   /*sq_inplace_repeat*/
};

static PyMappingMethods Matrix_AsMapping = {
    (lenfunc)Matrix_len,
    (binaryfunc)Matrix_subscript,
    (objobjargproc)Matrix_ass_subscript,
};

static PyNumberMethods Matrix_NumMethods = {
    (binaryfunc)Matrix_add,            /*nb_add*/
    (binaryfunc)Matrix_sub,            /*nb_subtract*/
    (binaryfunc)Matrix_mul,            /*nb_multiply*/
    NULL,                              /*nb_remainder*/
    NULL,                              /*nb_divmod*/
    NULL,                              /*nb_power*/
    (unaryfunc)0,                      /*nb_negative*/
    (unaryfunc)0,                      /*tp_positive*/
    (unaryfunc)0,                      /*tp_absolute*/
    (inquiry)0,                        /*tp_bool*/
    (unaryfunc)Matrix_inverted_noargs, /*nb_invert*/
    NULL,                              /*nb_lshift*/
    (binaryfunc)0,                     /*nb_rshift*/
    NULL,                              /*nb_and*/
    NULL,                              /*nb_xor*/
    NULL,                              /*nb_or*/
    NULL,                              /*nb_int*/
    NULL,                              /*nb_reserved*/
    NULL,                              /*nb_float*/
    NULL,                              /*nb_inplace_add*/
    NULL,                              /*nb_inplace_subtract*/
    (binaryfunc)Matrix_imul,           /*nb_inplace_multiply*/
    NULL,                              /*nb_inplace_remainder*/
    NULL,                              /*nb_inplace_power*/
    NULL,                              /*nb_inplace_lshift*/
    NULL,                              /*nb_inplace_rshift*/
    NULL,                              /*nb_inplace_and*/
    NULL,                              /*nb_inplace_xor*/
    NULL,                              /*nb_inplace_or*/
    NULL,                              /*nb_floor_divide*/
    NULL,                              /*nb_true_divide*/
    NULL,                              /*nb_inplace_floor_divide*/
    NULL,                              /*nb_inplace_true_divide*/
    NULL,                              /*nb_index*/
    (binaryfunc)Matrix_matmul,         /*nb_matrix_multiply*/
    (binaryfunc)Matrix_imatmul,        /*nb_inplace_matrix_multiply*/
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Get/Set Item Implementation
 * \{ */

PyDoc_STRVAR(Matrix_translation_doc, "The translation component of the matrix.\n\n:type: Vector");
static PyObject *Matrix_translation_get(MatrixObject *self, void *UNUSED(closure))
{
  PyObject *ret;

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* Must be 4x4 square matrix. */
  if (self->row_num != 4 || self->col_num != 4) {
    PyErr_SetString(PyExc_AttributeError,
                    "Matrix.translation: "
                    "inappropriate matrix size, must be 4x4");
    return NULL;
  }

  ret = (PyObject *)Vector_CreatePyObject_cb(
      (PyObject *)self, 3, mathutils_matrix_translation_cb_index, 3);

  return ret;
}

static int Matrix_translation_set(MatrixObject *self, PyObject *value, void *UNUSED(closure))
{
  float tvec[3];

  if (BaseMath_ReadCallback_ForWrite(self) == -1) {
    return -1;
  }

  /* Must be 4x4 square matrix. */
  if (self->row_num != 4 || self->col_num != 4) {
    PyErr_SetString(PyExc_AttributeError,
                    "Matrix.translation: "
                    "inappropriate matrix size, must be 4x4");
    return -1;
  }

  if (mathutils_array_parse(tvec, 3, 3, value, "Matrix.translation") == -1) {
    return -1;
  }

  copy_v3_v3(((float(*)[4])self->matrix)[3], tvec);

  (void)BaseMath_WriteCallback(self);

  return 0;
}

PyDoc_STRVAR(Matrix_row_doc,
             "Access the matrix by rows (default), (read-only).\n\n:type: Matrix Access");
static PyObject *Matrix_row_get(MatrixObject *self, void *UNUSED(closure))
{
  return MatrixAccess_CreatePyObject(self, MAT_ACCESS_ROW);
}

PyDoc_STRVAR(
    Matrix_col_doc,
    "Access the matrix by columns, 3x3 and 4x4 only, (read-only).\n\n:type: Matrix Access");
static PyObject *Matrix_col_get(MatrixObject *self, void *UNUSED(closure))
{
  return MatrixAccess_CreatePyObject(self, MAT_ACCESS_COL);
}

PyDoc_STRVAR(Matrix_median_scale_doc,
             "The average scale applied to each axis (read-only).\n\n:type: float");
static PyObject *Matrix_median_scale_get(MatrixObject *self, void *UNUSED(closure))
{
  float mat[3][3];

  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* Must be 3-4 cols, 3-4 rows, square matrix. */
  if ((self->row_num < 3) || (self->col_num < 3)) {
    PyErr_SetString(PyExc_AttributeError,
                    "Matrix.median_scale: "
                    "inappropriate matrix size, 3x3 minimum");
    return NULL;
  }

  matrix_as_3x3(mat, self);

  return PyFloat_FromDouble(mat3_to_scale(mat));
}

PyDoc_STRVAR(Matrix_is_identity_doc,
             "True if this is an identity matrix (read-only).\n\n:type: bool");
static PyObject *Matrix_is_identity_get(MatrixObject *self, void *UNUSED(closure))
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }
  return PyBool_FromLong(matrix_is_identity(self));
}

PyDoc_STRVAR(Matrix_is_negative_doc,
             "True if this matrix results in a negative scale, 3x3 and 4x4 only, "
             "(read-only).\n\n:type: bool");
static PyObject *Matrix_is_negative_get(MatrixObject *self, void *UNUSED(closure))
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* Must be 3-4 cols, 3-4 rows, square matrix. */
  if (self->row_num == 4 && self->col_num == 4) {
    return PyBool_FromLong(is_negative_m4((const float(*)[4])self->matrix));
  }
  if (self->row_num == 3 && self->col_num == 3) {
    return PyBool_FromLong(is_negative_m3((const float(*)[3])self->matrix));
  }

  PyErr_SetString(PyExc_AttributeError,
                  "Matrix.is_negative: "
                  "inappropriate matrix size - expects 3x3 or 4x4 matrix");
  return NULL;
}

PyDoc_STRVAR(Matrix_is_orthogonal_doc,
             "True if this matrix is orthogonal, 3x3 and 4x4 only, (read-only).\n\n:type: bool");
static PyObject *Matrix_is_orthogonal_get(MatrixObject *self, void *UNUSED(closure))
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* Must be 3-4 cols, 3-4 rows, square matrix. */
  if (self->row_num == 4 && self->col_num == 4) {
    return PyBool_FromLong(is_orthonormal_m4((const float(*)[4])self->matrix));
  }
  if (self->row_num == 3 && self->col_num == 3) {
    return PyBool_FromLong(is_orthonormal_m3((const float(*)[3])self->matrix));
  }

  PyErr_SetString(PyExc_AttributeError,
                  "Matrix.is_orthogonal: "
                  "inappropriate matrix size - expects 3x3 or 4x4 matrix");
  return NULL;
}

PyDoc_STRVAR(Matrix_is_orthogonal_axis_vectors_doc,
             "True if this matrix has got orthogonal axis vectors, 3x3 and 4x4 only, "
             "(read-only).\n\n:type: bool");
static PyObject *Matrix_is_orthogonal_axis_vectors_get(MatrixObject *self, void *UNUSED(closure))
{
  if (BaseMath_ReadCallback(self) == -1) {
    return NULL;
  }

  /* Must be 3-4 cols, 3-4 rows, square matrix. */
  if (self->row_num == 4 && self->col_num == 4) {
    return PyBool_FromLong(is_orthogonal_m4((const float(*)[4])self->matrix));
  }
  if (self->row_num == 3 && self->col_num == 3) {
    return PyBool_FromLong(is_orthogonal_m3((const float(*)[3])self->matrix));
  }

  PyErr_SetString(PyExc_AttributeError,
                  "Matrix.is_orthogonal_axis_vectors: "
                  "inappropriate matrix size - expects 3x3 or 4x4 matrix");
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Get/Set Item Definitions
 * \{ */

static PyGetSetDef Matrix_getseters[] = {
    {"median_scale", (getter)Matrix_median_scale_get, (setter)NULL, Matrix_median_scale_doc, NULL},
    {"translation",
     (getter)Matrix_translation_get,
     (setter)Matrix_translation_set,
     Matrix_translation_doc,
     NULL},
    {"row", (getter)Matrix_row_get, (setter)NULL, Matrix_row_doc, NULL},
    {"col", (getter)Matrix_col_get, (setter)NULL, Matrix_col_doc, NULL},
    {"is_identity", (getter)Matrix_is_identity_get, (setter)NULL, Matrix_is_identity_doc, NULL},
    {"is_negative", (getter)Matrix_is_negative_get, (setter)NULL, Matrix_is_negative_doc, NULL},
    {"is_orthogonal",
     (getter)Matrix_is_orthogonal_get,
     (setter)NULL,
     Matrix_is_orthogonal_doc,
     NULL},
    {"is_orthogonal_axis_vectors",
     (getter)Matrix_is_orthogonal_axis_vectors_get,
     (setter)NULL,
     Matrix_is_orthogonal_axis_vectors_doc,
     NULL},
    {"is_wrapped",
     (getter)BaseMathObject_is_wrapped_get,
     (setter)NULL,
     BaseMathObject_is_wrapped_doc,
     NULL},
    {"is_frozen",
     (getter)BaseMathObject_is_frozen_get,
     (setter)NULL,
     BaseMathObject_is_frozen_doc,
     NULL},
    {"is_valid",
     (getter)BaseMathObject_is_valid_get,
     (setter)NULL,
     BaseMathObject_is_valid_doc,
     NULL},
    {"owner", (getter)BaseMathObject_owner_get, (setter)NULL, BaseMathObject_owner_doc, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Method Definitions
 * \{ */

static struct PyMethodDef Matrix_methods[] = {
    /* Derived values. */
    {"determinant", (PyCFunction)Matrix_determinant, METH_NOARGS, Matrix_determinant_doc},
    {"decompose", (PyCFunction)Matrix_decompose, METH_NOARGS, Matrix_decompose_doc},

    /* In place only. */
    {"zero", (PyCFunction)Matrix_zero, METH_NOARGS, Matrix_zero_doc},
    {"identity", (PyCFunction)Matrix_identity, METH_NOARGS, Matrix_identity_doc},

    /* Operate on original or copy. */
    {"transpose", (PyCFunction)Matrix_transpose, METH_NOARGS, Matrix_transpose_doc},
    {"transposed", (PyCFunction)Matrix_transposed, METH_NOARGS, Matrix_transposed_doc},
    {"normalize", (PyCFunction)Matrix_normalize, METH_NOARGS, Matrix_normalize_doc},
    {"normalized", (PyCFunction)Matrix_normalized, METH_NOARGS, Matrix_normalized_doc},
    {"invert", (PyCFunction)Matrix_invert, METH_VARARGS, Matrix_invert_doc},
    {"inverted", (PyCFunction)Matrix_inverted, METH_VARARGS, Matrix_inverted_doc},
    {"invert_safe", (PyCFunction)Matrix_invert_safe, METH_NOARGS, Matrix_invert_safe_doc},
    {"inverted_safe", (PyCFunction)Matrix_inverted_safe, METH_NOARGS, Matrix_inverted_safe_doc},
    {"adjugate", (PyCFunction)Matrix_adjugate, METH_NOARGS, Matrix_adjugate_doc},
    {"adjugated", (PyCFunction)Matrix_adjugated, METH_NOARGS, Matrix_adjugated_doc},
    {"to_2x2", (PyCFunction)Matrix_to_2x2, METH_NOARGS, Matrix_to_2x2_doc},
    {"to_3x3", (PyCFunction)Matrix_to_3x3, METH_NOARGS, Matrix_to_3x3_doc},
    {"to_4x4", (PyCFunction)Matrix_to_4x4, METH_NOARGS, Matrix_to_4x4_doc},
    /* TODO: {"resize_3x3", (PyCFunction) Matrix_resize3x3, METH_NOARGS, Matrix_resize3x3_doc}, */
    {"resize_4x4", (PyCFunction)Matrix_resize_4x4, METH_NOARGS, Matrix_resize_4x4_doc},
    {"rotate", (PyCFunction)Matrix_rotate, METH_O, Matrix_rotate_doc},

    /* Return converted representation. */
    {"to_euler", (PyCFunction)Matrix_to_euler, METH_VARARGS, Matrix_to_euler_doc},
    {"to_quaternion", (PyCFunction)Matrix_to_quaternion, METH_NOARGS, Matrix_to_quaternion_doc},
    {"to_scale", (PyCFunction)Matrix_to_scale, METH_NOARGS, Matrix_to_scale_doc},
    {"to_translation", (PyCFunction)Matrix_to_translation, METH_NOARGS, Matrix_to_translation_doc},

    /* Operation between 2 or more types. */
    {"lerp", (PyCFunction)Matrix_lerp, METH_VARARGS, Matrix_lerp_doc},
    {"copy", (PyCFunction)Matrix_copy, METH_NOARGS, Matrix_copy_doc},
    {"__copy__", (PyCFunction)Matrix_copy, METH_NOARGS, Matrix_copy_doc},
    {"__deepcopy__", (PyCFunction)Matrix_deepcopy, METH_VARARGS, Matrix_copy_doc},

    /* Base-math methods. */
    {"freeze", (PyCFunction)BaseMathObject_freeze, METH_NOARGS, BaseMathObject_freeze_doc},

    /* Class methods. */
    {"Identity", (PyCFunction)C_Matrix_Identity, METH_VARARGS | METH_CLASS, C_Matrix_Identity_doc},
    {"Rotation", (PyCFunction)C_Matrix_Rotation, METH_VARARGS | METH_CLASS, C_Matrix_Rotation_doc},
    {"Scale", (PyCFunction)C_Matrix_Scale, METH_VARARGS | METH_CLASS, C_Matrix_Scale_doc},
    {"Shear", (PyCFunction)C_Matrix_Shear, METH_VARARGS | METH_CLASS, C_Matrix_Shear_doc},
    {"Diagonal", (PyCFunction)C_Matrix_Diagonal, METH_O | METH_CLASS, C_Matrix_Diagonal_doc},
    {"Translation",
     (PyCFunction)C_Matrix_Translation,
     METH_O | METH_CLASS,
     C_Matrix_Translation_doc},
    {"OrthoProjection",
     (PyCFunction)C_Matrix_OrthoProjection,
     METH_VARARGS | METH_CLASS,
     C_Matrix_OrthoProjection_doc},
    {"LocRotScale",
     (PyCFunction)C_Matrix_LocRotScale,
     METH_VARARGS | METH_CLASS,
     C_Matrix_LocRotScale_doc},
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: Python Object Definition
 * \{ */

PyDoc_STRVAR(
    matrix_doc,
    ".. class:: Matrix([rows])\n"
    "\n"
    "   This object gives access to Matrices in Blender, supporting square and rectangular\n"
    "   matrices from 2x2 up to 4x4.\n"
    "\n"
    "   :arg rows: Sequence of rows. When omitted, a 4x4 identity matrix is constructed.\n"
    "   :type rows: 2d number sequence\n");
PyTypeObject matrix_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "Matrix", /*tp_name*/
    sizeof(MatrixObject),                    /*tp_basicsize*/
    0,                                       /*tp_itemsize*/
    (destructor)BaseMathObject_dealloc,      /*tp_dealloc*/
    (printfunc)NULL,                         /*tp_print*/
    NULL,                                    /*tp_getattr*/
    NULL,                                    /*tp_setattr*/
    NULL,                                    /*tp_compare*/
    (reprfunc)Matrix_repr,                   /*tp_repr*/
    &Matrix_NumMethods,                      /*tp_as_number*/
    &Matrix_SeqMethods,                      /*tp_as_sequence*/
    &Matrix_AsMapping,                       /*tp_as_mapping*/
    (hashfunc)Matrix_hash,                   /*tp_hash*/
    NULL,                                    /*tp_call*/
#ifndef MATH_STANDALONE
    (reprfunc)Matrix_str, /*tp_str*/
#else
    NULL, /*tp_str*/
#endif
    NULL,                                                          /*tp_getattro*/
    NULL,                                                          /*tp_setattro*/
    NULL,                                                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    matrix_doc,                                                    /*tp_doc*/
    (traverseproc)BaseMathObject_traverse,                         /* tp_traverse */
    (inquiry)BaseMathObject_clear,                                 /*tp_clear*/
    (richcmpfunc)Matrix_richcmpr,                                  /*tp_richcompare*/
    0,                                                             /*tp_weaklistoffset*/
    NULL,                                                          /*tp_iter*/
    NULL,                                                          /*tp_iternext*/
    Matrix_methods,                                                /*tp_methods*/
    NULL,                                                          /*tp_members*/
    Matrix_getseters,                                              /*tp_getset*/
    NULL,                                                          /*tp_base*/
    NULL,                                                          /*tp_dict*/
    NULL,                                                          /*tp_descr_get*/
    NULL,                                                          /*tp_descr_set*/
    0,                                                             /*tp_dictoffset*/
    NULL,                                                          /*tp_init*/
    NULL,                                                          /*tp_alloc*/
    Matrix_new,                                                    /*tp_new*/
    NULL,                                                          /*tp_free*/
    NULL,                                                          /*tp_is_gc*/
    NULL,                                                          /*tp_bases*/
    NULL,                                                          /*tp_mro*/
    NULL,                                                          /*tp_cache*/
    NULL,                                                          /*tp_subclasses*/
    NULL,                                                          /*tp_weaklist*/
    NULL,                                                          /*tp_del*/
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: C/API Constructors
 * \{ */

PyObject *Matrix_CreatePyObject(const float *mat,
                                const ushort col_num,
                                const ushort row_num,
                                PyTypeObject *base_type)
{
  MatrixObject *self;
  float *mat_alloc;

  /* matrix objects can be any 2-4row x 2-4col matrix */
  if (col_num < 2 || col_num > 4 || row_num < 2 || row_num > 4) {
    PyErr_SetString(PyExc_RuntimeError,
                    "Matrix(): "
                    "row and column sizes must be between 2 and 4");
    return NULL;
  }

  mat_alloc = PyMem_Malloc(col_num * row_num * sizeof(float));
  if (UNLIKELY(mat_alloc == NULL)) {
    PyErr_SetString(PyExc_MemoryError,
                    "Matrix(): "
                    "problem allocating data");
    return NULL;
  }

  self = BASE_MATH_NEW(MatrixObject, matrix_Type, base_type);
  if (self) {
    self->matrix = mat_alloc;
    self->col_num = col_num;
    self->row_num = row_num;

    /* init callbacks as NULL */
    self->cb_user = NULL;
    self->cb_type = self->cb_subtype = 0;

    if (mat) { /* If a float array passed. */
      memcpy(self->matrix, mat, col_num * row_num * sizeof(float));
    }
    else if (col_num == row_num) {
      /* or if no arguments are passed return identity matrix for square matrices */
      matrix_identity_internal(self);
    }
    else {
      /* otherwise zero everything */
      memset(self->matrix, 0, col_num * row_num * sizeof(float));
    }
    self->flag = BASE_MATH_FLAG_DEFAULT;
  }
  else {
    PyMem_Free(mat_alloc);
  }

  return (PyObject *)self;
}

PyObject *Matrix_CreatePyObject_wrap(float *mat,
                                     const ushort col_num,
                                     const ushort row_num,
                                     PyTypeObject *base_type)
{
  MatrixObject *self;

  /* matrix objects can be any 2-4row x 2-4col matrix */
  if (col_num < 2 || col_num > 4 || row_num < 2 || row_num > 4) {
    PyErr_SetString(PyExc_RuntimeError,
                    "Matrix(): "
                    "row and column sizes must be between 2 and 4");
    return NULL;
  }

  self = BASE_MATH_NEW(MatrixObject, matrix_Type, base_type);
  if (self) {
    self->col_num = col_num;
    self->row_num = row_num;

    /* init callbacks as NULL */
    self->cb_user = NULL;
    self->cb_type = self->cb_subtype = 0;

    self->matrix = mat;
    self->flag = BASE_MATH_FLAG_DEFAULT | BASE_MATH_FLAG_IS_WRAP;
  }
  return (PyObject *)self;
}

PyObject *Matrix_CreatePyObject_cb(
    PyObject *cb_user, const ushort col_num, const ushort row_num, uchar cb_type, uchar cb_subtype)
{
  MatrixObject *self = (MatrixObject *)Matrix_CreatePyObject(NULL, col_num, row_num, NULL);
  if (self) {
    Py_INCREF(cb_user);
    self->cb_user = cb_user;
    self->cb_type = cb_type;
    self->cb_subtype = cb_subtype;
    PyObject_GC_Track(self);
  }
  return (PyObject *)self;
}

PyObject *Matrix_CreatePyObject_alloc(float *mat,
                                      const ushort col_num,
                                      const ushort row_num,
                                      PyTypeObject *base_type)
{
  MatrixObject *self;
  self = (MatrixObject *)Matrix_CreatePyObject_wrap(mat, col_num, row_num, base_type);
  if (self) {
    self->flag &= ~BASE_MATH_FLAG_IS_WRAP;
  }

  return (PyObject *)self;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix Type: C/API Parse Utilities
 * \{ */

/**
 * Use with PyArg_ParseTuple's "O&" formatting.
 */
static bool Matrix_ParseCheck(MatrixObject *pymat)
{
  if (!MatrixObject_Check(pymat)) {
    PyErr_Format(
        PyExc_TypeError, "expected a mathutils.Matrix, not a %.200s", Py_TYPE(pymat)->tp_name);
    return false;
  }
  /* sets error */
  if (BaseMath_ReadCallback(pymat) == -1) {
    return false;
  }
  return true;
}

int Matrix_ParseAny(PyObject *o, void *p)
{
  MatrixObject **pymat_p = p;
  MatrixObject *pymat = (MatrixObject *)o;

  if (!Matrix_ParseCheck(pymat)) {
    return 0;
  }
  *pymat_p = pymat;
  return 1;
}

int Matrix_Parse2x2(PyObject *o, void *p)
{
  MatrixObject **pymat_p = p;
  MatrixObject *pymat = (MatrixObject *)o;

  if (!Matrix_ParseCheck(pymat)) {
    return 0;
  }
  if ((pymat->col_num != 2) || (pymat->row_num != 2)) {
    PyErr_SetString(PyExc_ValueError, "matrix must be 2x2");
    return 0;
  }

  *pymat_p = pymat;
  return 1;
}

int Matrix_Parse3x3(PyObject *o, void *p)
{
  MatrixObject **pymat_p = p;
  MatrixObject *pymat = (MatrixObject *)o;

  if (!Matrix_ParseCheck(pymat)) {
    return 0;
  }
  if ((pymat->col_num != 3) || (pymat->row_num != 3)) {
    PyErr_SetString(PyExc_ValueError, "matrix must be 3x3");
    return 0;
  }

  *pymat_p = pymat;
  return 1;
}

int Matrix_Parse4x4(PyObject *o, void *p)
{
  MatrixObject **pymat_p = p;
  MatrixObject *pymat = (MatrixObject *)o;

  if (!Matrix_ParseCheck(pymat)) {
    return 0;
  }
  if ((pymat->col_num != 4) || (pymat->row_num != 4)) {
    PyErr_SetString(PyExc_ValueError, "matrix must be 4x4");
    return 0;
  }

  *pymat_p = pymat;
  return 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix-Access Type: Struct & Internal Functions
 * \{ */

typedef struct {
  PyObject_HEAD /* Required Python macro. */
  MatrixObject *matrix_user;
  eMatrixAccess_t type;
} MatrixAccessObject;

static int MatrixAccess_traverse(MatrixAccessObject *self, visitproc visit, void *arg)
{
  Py_VISIT(self->matrix_user);
  return 0;
}

static int MatrixAccess_clear(MatrixAccessObject *self)
{
  Py_CLEAR(self->matrix_user);
  return 0;
}

static void MatrixAccess_dealloc(MatrixAccessObject *self)
{
  if (self->matrix_user) {
    PyObject_GC_UnTrack(self);
    MatrixAccess_clear(self);
  }

  Py_TYPE(self)->tp_free(self);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix-Access Type: Sequence Protocol
 * \{ */

static int MatrixAccess_len(MatrixAccessObject *self)
{
  return (self->type == MAT_ACCESS_ROW) ? self->matrix_user->row_num : self->matrix_user->col_num;
}

static PyObject *MatrixAccess_slice(MatrixAccessObject *self, int begin, int end)
{
  PyObject *tuple;
  int count;

  /* row/col access */
  MatrixObject *matrix_user = self->matrix_user;
  int matrix_access_len;
  PyObject *(*Matrix_item_new)(MatrixObject *, int);

  if (self->type == MAT_ACCESS_ROW) {
    matrix_access_len = matrix_user->row_num;
    Matrix_item_new = Matrix_item_row;
  }
  else { /* MAT_ACCESS_ROW */
    matrix_access_len = matrix_user->col_num;
    Matrix_item_new = Matrix_item_col;
  }

  CLAMP(begin, 0, matrix_access_len);
  if (end < 0) {
    end = (matrix_access_len + 1) + end;
  }
  CLAMP(end, 0, matrix_access_len);
  begin = MIN2(begin, end);

  tuple = PyTuple_New(end - begin);
  for (count = begin; count < end; count++) {
    PyTuple_SET_ITEM(tuple, count - begin, Matrix_item_new(matrix_user, count));
  }

  return tuple;
}

static PyObject *MatrixAccess_subscript(MatrixAccessObject *self, PyObject *item)
{
  MatrixObject *matrix_user = self->matrix_user;

  if (PyIndex_Check(item)) {
    Py_ssize_t i;
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return NULL;
    }
    if (self->type == MAT_ACCESS_ROW) {
      if (i < 0) {
        i += matrix_user->row_num;
      }
      return Matrix_item_row(matrix_user, i);
    }
    /* MAT_ACCESS_ROW */
    if (i < 0) {
      i += matrix_user->col_num;
    }
    return Matrix_item_col(matrix_user, i);
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(item, MatrixAccess_len(self), &start, &stop, &step, &slicelength) <
        0) {
      return NULL;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }
    if (step == 1) {
      return MatrixAccess_slice(self, start, stop);
    }

    PyErr_SetString(PyExc_IndexError, "slice steps not supported with matrix accessors");
    return NULL;
  }

  PyErr_Format(
      PyExc_TypeError, "matrix indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return NULL;
}

static int MatrixAccess_ass_subscript(MatrixAccessObject *self, PyObject *item, PyObject *value)
{
  MatrixObject *matrix_user = self->matrix_user;

  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }

    if (self->type == MAT_ACCESS_ROW) {
      if (i < 0) {
        i += matrix_user->row_num;
      }
      return Matrix_ass_item_row(matrix_user, i, value);
    }
    /* MAT_ACCESS_ROW */
    if (i < 0) {
      i += matrix_user->col_num;
    }
    return Matrix_ass_item_col(matrix_user, i, value);
  }
  /* TODO: slice. */

  PyErr_Format(
      PyExc_TypeError, "matrix indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
  return -1;
}

static PyObject *MatrixAccess_iter(MatrixAccessObject *self)
{
  /* Try get values from a collection. */
  PyObject *ret;
  PyObject *iter = NULL;
  ret = MatrixAccess_slice(self, 0, MATRIX_MAX_DIM);

  /* We know this is a tuple so no need to #PyIter_Check
   * otherwise it could be NULL (unlikely) if conversion failed. */
  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}

static PyMappingMethods MatrixAccess_AsMapping = {
    (lenfunc)MatrixAccess_len,
    (binaryfunc)MatrixAccess_subscript,
    (objobjargproc)MatrixAccess_ass_subscript,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix-Access Type: Python Object Definition
 * \{ */

PyTypeObject matrix_access_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "MatrixAccess",           /*tp_name*/
    sizeof(MatrixAccessObject),                              /*tp_basicsize*/
    0,                                                       /*tp_itemsize*/
    (destructor)MatrixAccess_dealloc,                        /*tp_dealloc*/
    (printfunc)NULL,                                         /*tp_print*/
    NULL,                                                    /*tp_getattr*/
    NULL,                                                    /*tp_setattr*/
    NULL,                                                    /*tp_compare*/
    NULL,                                                    /*tp_repr*/
    NULL,                                                    /*tp_as_number*/
    NULL /* &MatrixAccess_SeqMethods */ /* TODO */,          /*tp_as_sequence*/
    &MatrixAccess_AsMapping,                                 /*tp_as_mapping*/
    NULL,                                                    /*tp_hash*/
    NULL,                                                    /*tp_call*/
    NULL,                                                    /*tp_str*/
    NULL,                                                    /*tp_getattro*/
    NULL,                                                    /*tp_setattro*/
    NULL,                                                    /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,                 /*tp_flags*/
    NULL,                                                    /*tp_doc*/
    (traverseproc)MatrixAccess_traverse,                     /*tp_traverse*/
    (inquiry)MatrixAccess_clear,                             /*tp_clear*/
    NULL /* (richcmpfunc)MatrixAccess_richcmpr */ /* TODO*/, /*tp_richcompare*/
    0,                                                       /*tp_weaklistoffset*/
    (getiterfunc)MatrixAccess_iter,                          /* getiterfunc tp_iter; */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Matrix-Access Type: C/API Constructor
 * \{ */

static PyObject *MatrixAccess_CreatePyObject(MatrixObject *matrix, const eMatrixAccess_t type)
{
  MatrixAccessObject *matrix_access = (MatrixAccessObject *)PyObject_GC_New(MatrixObject,
                                                                            &matrix_access_Type);

  matrix_access->matrix_user = matrix;
  Py_INCREF(matrix);

  matrix_access->type = type;

  return (PyObject *)matrix_access;
}

/** \} */
