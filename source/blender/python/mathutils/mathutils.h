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

#ifndef __MATHUTILS_H__
#define __MATHUTILS_H__

/** \file
 * \ingroup pymathutils
 */

/* Can cast different mathutils types to this, use for generic funcs */

#include "BLI_compiler_attrs.h"

struct DynStr;

extern char BaseMathObject_is_wrapped_doc[];
extern char BaseMathObject_is_frozen_doc[];
extern char BaseMathObject_owner_doc[];

#define BASE_MATH_NEW(struct_name, root_type, base_type) \
  ((struct_name *)((base_type ? (base_type)->tp_alloc(base_type, 0) : \
                                _PyObject_GC_New(&(root_type)))))

/** BaseMathObject.flag */
enum {
  /**
   * Do not own the memory used in this vector,
   * \note This is error prone if the memory may be freed while this vector is in use.
   * Prefer using callbacks where possible, see: #Mathutils_RegisterCallback
   */
  BASE_MATH_FLAG_IS_WRAP = (1 << 0),
  /**
   * Prevent changes to the vector so it can be used as a set or dictionary key for example.
   * (typical use cases for tuple).
   */
  BASE_MATH_FLAG_IS_FROZEN = (1 << 1),
};
#define BASE_MATH_FLAG_DEFAULT 0

#define BASE_MATH_MEMBERS(_data) \
  /** Array of data (alias), wrapped status depends on wrapped status. */ \
  PyObject_VAR_HEAD float *_data; \
  /** If this vector references another object, otherwise NULL, *Note* this owns its reference */ \
  PyObject *cb_user; \
  /** Which user funcs do we adhere to, RNA, etc */ \
  unsigned char cb_type; \
  /** Subtype: location, rotation... \
   * to avoid defining many new functions for every attribute of the same type */ \
  unsigned char cb_subtype; \
  /** Wrapped data type. */ \
  unsigned char flag

typedef struct {
  BASE_MATH_MEMBERS(data);
} BaseMathObject;

/* types */
#include "mathutils_Vector.h"
#include "mathutils_Matrix.h"
#include "mathutils_Quaternion.h"
#include "mathutils_Euler.h"
#include "mathutils_Color.h"

/* avoid checking all types */
#define BaseMathObject_CheckExact(v) (Py_TYPE(v)->tp_dealloc == (destructor)BaseMathObject_dealloc)

PyObject *BaseMathObject_owner_get(BaseMathObject *self, void *);
PyObject *BaseMathObject_is_wrapped_get(BaseMathObject *self, void *);
PyObject *BaseMathObject_is_frozen_get(BaseMathObject *self, void *);

extern char BaseMathObject_freeze_doc[];
PyObject *BaseMathObject_freeze(BaseMathObject *self);

int BaseMathObject_traverse(BaseMathObject *self, visitproc visit, void *arg);
int BaseMathObject_clear(BaseMathObject *self);
void BaseMathObject_dealloc(BaseMathObject *self);

PyMODINIT_FUNC PyInit_mathutils(void);

int EXPP_FloatsAreEqual(float A, float B, int floatSteps);
int EXPP_VectorsAreEqual(const float *vecA, const float *vecB, int size, int floatSteps);

typedef struct Mathutils_Callback Mathutils_Callback;

typedef int (*BaseMathCheckFunc)(BaseMathObject *);    /* checks the user is still valid */
typedef int (*BaseMathGetFunc)(BaseMathObject *, int); /* gets the vector from the user */
typedef int (*BaseMathSetFunc)(BaseMathObject *,
                               int); /* sets the users vector values once its modified */
typedef int (*BaseMathGetIndexFunc)(BaseMathObject *,
                                    int,
                                    int); /* same as above but only for an index */
typedef int (*BaseMathSetIndexFunc)(BaseMathObject *,
                                    int,
                                    int); /* same as above but only for an index */

struct Mathutils_Callback {
  BaseMathCheckFunc check;
  BaseMathGetFunc get;
  BaseMathSetFunc set;
  BaseMathGetIndexFunc get_index;
  BaseMathSetIndexFunc set_index;
};

unsigned char Mathutils_RegisterCallback(Mathutils_Callback *cb);

int _BaseMathObject_ReadCallback(BaseMathObject *self);
int _BaseMathObject_WriteCallback(BaseMathObject *self);
int _BaseMathObject_ReadIndexCallback(BaseMathObject *self, int index);
int _BaseMathObject_WriteIndexCallback(BaseMathObject *self, int index);

void _BaseMathObject_RaiseFrozenExc(const BaseMathObject *self);
void _BaseMathObject_RaiseNotFrozenExc(const BaseMathObject *self);

/* since this is called so often avoid where possible */
#define BaseMath_ReadCallback(_self) \
  (((_self)->cb_user ? _BaseMathObject_ReadCallback((BaseMathObject *)_self) : 0))
#define BaseMath_WriteCallback(_self) \
  (((_self)->cb_user ? _BaseMathObject_WriteCallback((BaseMathObject *)_self) : 0))
#define BaseMath_ReadIndexCallback(_self, _index) \
  (((_self)->cb_user ? _BaseMathObject_ReadIndexCallback((BaseMathObject *)_self, _index) : 0))
#define BaseMath_WriteIndexCallback(_self, _index) \
  (((_self)->cb_user ? _BaseMathObject_WriteIndexCallback((BaseMathObject *)_self, _index) : 0))

/* support BASE_MATH_FLAG_IS_FROZEN */
#define BaseMath_ReadCallback_ForWrite(_self) \
  (UNLIKELY((_self)->flag & BASE_MATH_FLAG_IS_FROZEN) ? \
       (_BaseMathObject_RaiseFrozenExc((BaseMathObject *)_self), -1) : \
       (BaseMath_ReadCallback(_self)))

#define BaseMath_ReadIndexCallback_ForWrite(_self, _index) \
  (UNLIKELY((_self)->flag & BASE_MATH_FLAG_IS_FROZEN) ? \
       (_BaseMathObject_RaiseFrozenExc((BaseMathObject *)_self), -1) : \
       (BaseMath_ReadIndexCallback(_self, _index)))

#define BaseMath_Prepare_ForWrite(_self) \
  (UNLIKELY((_self)->flag & BASE_MATH_FLAG_IS_FROZEN) ? \
       (_BaseMathObject_RaiseFrozenExc((BaseMathObject *)_self), -1) : \
       0)

#define BaseMathObject_Prepare_ForHash(_self) \
  (UNLIKELY(((_self)->flag & BASE_MATH_FLAG_IS_FROZEN) == 0) ? \
       (_BaseMathObject_RaiseNotFrozenExc((BaseMathObject *)_self), -1) : \
       0)

/* utility func */
int mathutils_array_parse(
    float *array, int array_min, int array_max, PyObject *value, const char *error_prefix);
int mathutils_array_parse_alloc(float **array,
                                int array_min,
                                PyObject *value,
                                const char *error_prefix);
int mathutils_array_parse_alloc_v(float **array,
                                  int array_dim,
                                  PyObject *value,
                                  const char *error_prefix);
int mathutils_any_to_rotmat(float rmat[3][3], PyObject *value, const char *error_prefix);

Py_hash_t mathutils_array_hash(const float *float_array, size_t array_len);

/* zero remaining unused elements of the array */
#define MU_ARRAY_ZERO (1u << 30)
/* ignore larger py sequences than requested (just use first elements),
 * handy when using 3d vectors as 2d */
#define MU_ARRAY_SPILL (1u << 31)

#define MU_ARRAY_FLAGS (MU_ARRAY_ZERO | MU_ARRAY_SPILL)

int column_vector_multiplication(float rvec[4], VectorObject *vec, MatrixObject *mat);

#ifndef MATH_STANDALONE
/* dynstr as python string utility functions */
PyObject *mathutils_dynstr_to_py(struct DynStr *ds);
#endif

#endif /* __MATHUTILS_H__ */
