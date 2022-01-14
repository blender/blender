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

#pragma once

extern PyTypeObject matrix_Type;
extern PyTypeObject matrix_access_Type;

typedef unsigned short ushort;

#define MatrixObject_Check(v) PyObject_TypeCheck((v), &matrix_Type)
#define MatrixObject_CheckExact(v) (Py_TYPE(v) == &matrix_Type)

#define MATRIX_MAX_DIM 4

/* matrix[row][col] == MATRIX_ITEM_INDEX(matrix, row, col) */

#ifdef DEBUG
#  define MATRIX_ITEM_ASSERT(_mat, _row, _col) \
    (BLI_assert(_row < (_mat)->num_row && _col < (_mat)->num_col))
#else
#  define MATRIX_ITEM_ASSERT(_mat, _row, _col) (void)0
#endif

#define MATRIX_ITEM_INDEX_NUMROW(_totrow, _row, _col) (((_totrow) * (_col)) + (_row))
#define MATRIX_ITEM_INDEX(_mat, _row, _col) \
  (MATRIX_ITEM_ASSERT(_mat, _row, _col), (((_mat)->num_row * (_col)) + (_row)))
#define MATRIX_ITEM_PTR(_mat, _row, _col) ((_mat)->matrix + MATRIX_ITEM_INDEX(_mat, _row, _col))
#define MATRIX_ITEM(_mat, _row, _col) ((_mat)->matrix[MATRIX_ITEM_INDEX(_mat, _row, _col)])

#define MATRIX_COL_INDEX(_mat, _col) (MATRIX_ITEM_INDEX(_mat, 0, _col))
#define MATRIX_COL_PTR(_mat, _col) ((_mat)->matrix + MATRIX_COL_INDEX(_mat, _col))

typedef struct {
  BASE_MATH_MEMBERS(matrix);
  ushort num_col;
  ushort num_row;
} MatrixObject;

/* struct data contains a pointer to the actual data that the
 * object uses. It can use either PyMem allocated data (which will
 * be stored in py_data) or be a wrapper for data allocated through
 * blender (stored in blend_data). This is an either/or struct not both */

/* prototypes */
PyObject *Matrix_CreatePyObject(const float *mat,
                                ushort num_col,
                                ushort num_row,
                                PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;
PyObject *Matrix_CreatePyObject_wrap(float *mat,
                                     ushort num_col,
                                     ushort num_row,
                                     PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
PyObject *Matrix_CreatePyObject_cb(PyObject *user,
                                   unsigned short num_col,
                                   unsigned short num_row,
                                   unsigned char cb_type,
                                   unsigned char cb_subtype) ATTR_WARN_UNUSED_RESULT;

/**
 * \param mat: Initialized matrix value to use in-place, allocated with #PyMem_Malloc
 */
PyObject *Matrix_CreatePyObject_alloc(float *mat,
                                      ushort num_col,
                                      ushort num_row,
                                      PyTypeObject *base_type) ATTR_WARN_UNUSED_RESULT;

/* PyArg_ParseTuple's "O&" formatting helpers. */
int Matrix_ParseAny(PyObject *o, void *p);
int Matrix_Parse2x2(PyObject *o, void *p);
int Matrix_Parse3x3(PyObject *o, void *p);
int Matrix_Parse4x4(PyObject *o, void *p);

extern unsigned char mathutils_matrix_row_cb_index; /* default */
extern unsigned char mathutils_matrix_col_cb_index;
extern unsigned char mathutils_matrix_translation_cb_index;

extern struct Mathutils_Callback mathutils_matrix_row_cb; /* default */
extern struct Mathutils_Callback mathutils_matrix_col_cb;
extern struct Mathutils_Callback mathutils_matrix_translation_cb;

void matrix_as_3x3(float mat[3][3], MatrixObject *self);
