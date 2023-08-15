/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BPy_IDGroup_Iter;
struct ID;
struct IDProperty;

extern PyTypeObject BPy_IDArray_Type;
extern PyTypeObject BPy_IDGroup_Type;

extern PyTypeObject BPy_IDGroup_ViewKeys_Type;
extern PyTypeObject BPy_IDGroup_ViewValues_Type;
extern PyTypeObject BPy_IDGroup_ViewItems_Type;

extern PyTypeObject BPy_IDGroup_IterKeys_Type;
extern PyTypeObject BPy_IDGroup_IterValues_Type;
extern PyTypeObject BPy_IDGroup_IterItems_Type;

#define BPy_IDArray_Check(v) (PyObject_TypeCheck(v, &BPy_IDArray_Type))
#define BPy_IDArray_CheckExact(v) (Py_TYPE(v) == &BPy_IDArray_Type)
#define BPy_IDGroup_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_Type))
#define BPy_IDGroup_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_Type)

#define BPy_IDGroup_ViewKeys_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_ViewKeys_Type))
#define BPy_IDGroup_ViewKeys_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_ViewKeys_Type)
#define BPy_IDGroup_ViewValues_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_ViewValues_Type))
#define BPy_IDGroup_ViewValues_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_ViewValues_Type)
#define BPy_IDGroup_ViewItems_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_ViewItems_Type))
#define BPy_IDGroup_ViewItems_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_ViewItems_Type)

#define BPy_IDGroup_IterKeys_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_IterKeys_Type))
#define BPy_IDGroup_IterKeys_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_IterKeys_Type)
#define BPy_IDGroup_IterValues_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_IterValues_Type))
#define BPy_IDGroup_IterValues_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_IterValues_Type)
#define BPy_IDGroup_IterItems_Check(v) (PyObject_TypeCheck(v, &BPy_IDGroup_IterItems_Type))
#define BPy_IDGroup_IterItems_CheckExact(v) (Py_TYPE(v) == &BPy_IDGroup_IterItems_Type)

typedef struct BPy_IDProperty {
  PyObject_VAR_HEAD
  struct ID *owner_id;     /* can be NULL */
  struct IDProperty *prop; /* must be second member */
  struct IDProperty *parent;
} BPy_IDProperty;

typedef struct BPy_IDArray {
  PyObject_VAR_HEAD
  struct ID *owner_id;     /* can be NULL */
  struct IDProperty *prop; /* must be second member */
} BPy_IDArray;

typedef struct BPy_IDGroup_Iter {
  PyObject_VAR_HEAD
  BPy_IDProperty *group;
  struct IDProperty *cur;
  /** Use for detecting manipulation during iteration (which is not allowed). */
  int len_init;
  /** Iterate in the reverse direction. */
  bool reversed;
} BPy_IDGroup_Iter;

/** Use to implement `IDPropertyGroup.keys/values/items` */
typedef struct BPy_IDGroup_View {
  PyObject_VAR_HEAD
  /** This will be NULL when accessing keys on data that has no ID properties. */
  BPy_IDProperty *group;
  bool reversed;
} BPy_IDGroup_View;

PyObject *BPy_Wrap_GetKeys(struct IDProperty *prop);
PyObject *BPy_Wrap_GetValues(struct ID *id, struct IDProperty *prop);
PyObject *BPy_Wrap_GetItems(struct ID *id, struct IDProperty *prop);

PyObject *BPy_Wrap_GetKeys_View_WithID(struct ID *id, struct IDProperty *prop);
PyObject *BPy_Wrap_GetValues_View_WithID(struct ID *id, struct IDProperty *prop);
PyObject *BPy_Wrap_GetItems_View_WithID(struct ID *id, struct IDProperty *prop);

int BPy_Wrap_SetMapItem(struct IDProperty *prop, PyObject *key, PyObject *val);

/**
 * For simple, non nested types this is the same as #BPy_IDGroup_WrapData.
 */
PyObject *BPy_IDGroup_MapDataToPy(struct IDProperty *prop);
PyObject *BPy_IDGroup_WrapData(struct ID *id, struct IDProperty *prop, struct IDProperty *parent);
/**
 * \note group can be a pointer array or a group.
 * assume we already checked key is a string.
 *
 * \return success.
 */
bool BPy_IDProperty_Map_ValidateAndCreate(PyObject *key, struct IDProperty *group, PyObject *ob);

void IDProp_Init_Types(void);

PyObject *BPyInit_idprop(void);

#ifdef __cplusplus
}
#endif
