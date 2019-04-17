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
 * \ingroup pygen
 */

#ifndef __IDPROP_PY_API_H__
#define __IDPROP_PY_API_H__

struct BPy_IDGroup_Iter;
struct ID;
struct IDProperty;

extern PyTypeObject BPy_IDArray_Type;
extern PyTypeObject BPy_IDGroup_Iter_Type;
extern PyTypeObject BPy_IDGroup_Type;

#define BPy_IDArray_Check(v) (PyObject_TypeCheck(v, &BPy_IDArray_Type))
#define BPy_IDArray_CheckExact(v) (Py_TYPE(v) == &BPy_IDArray_Type)
#define BPy_IDGroup_Iter_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_Iter_Type))
#define BPy_IDGroup_Iter_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_Iter_Type)
#define BPy_IDGroup_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_Type))
#define BPy_IDGroup_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_Type)

typedef struct BPy_IDProperty {
  PyObject_VAR_HEAD struct ID *id; /* can be NULL */
  struct IDProperty *prop;         /* must be second member */
  struct IDProperty *parent;
  PyObject *data_wrap;
} BPy_IDProperty;

typedef struct BPy_IDArray {
  PyObject_VAR_HEAD struct ID *id; /* can be NULL */
  struct IDProperty *prop;         /* must be second member */
} BPy_IDArray;

typedef struct BPy_IDGroup_Iter {
  PyObject_VAR_HEAD BPy_IDProperty *group;
  struct IDProperty *cur;
  int mode;
} BPy_IDGroup_Iter;

PyObject *BPy_Wrap_GetKeys(struct IDProperty *prop);
PyObject *BPy_Wrap_GetValues(struct ID *id, struct IDProperty *prop);
PyObject *BPy_Wrap_GetItems(struct ID *id, struct IDProperty *prop);
int BPy_Wrap_SetMapItem(struct IDProperty *prop, PyObject *key, PyObject *val);

PyObject *BPy_IDGroup_WrapData(struct ID *id, struct IDProperty *prop, struct IDProperty *parent);
bool BPy_IDProperty_Map_ValidateAndCreate(PyObject *key, struct IDProperty *group, PyObject *ob);

void IDProp_Init_Types(void);

PyObject *BPyInit_idprop(void);

#define IDPROP_ITER_KEYS 0
#define IDPROP_ITER_ITEMS 1

#endif /* __IDPROP_PY_API_H__ */
