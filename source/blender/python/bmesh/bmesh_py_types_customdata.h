/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* All use #BPy_BMLayerAccess struct. */

extern PyTypeObject BPy_BMLayerAccessVert_Type;
extern PyTypeObject BPy_BMLayerAccessEdge_Type;
extern PyTypeObject BPy_BMLayerAccessFace_Type;
extern PyTypeObject BPy_BMLayerAccessLoop_Type;

extern PyTypeObject BPy_BMLayerCollection_Type;
extern PyTypeObject BPy_BMLayerItem_Type;

#define BPy_BMLayerAccess_Check(v) (Py_TYPE(v) == &BPy_BMLayerAccess_Type)
#define BPy_BMLayerCollection_Check(v) (Py_TYPE(v) == &BPy_BMLayerCollection_Type)
#define BPy_BMLayerItem_Check(v) (Py_TYPE(v) == &BPy_BMLayerItem_Type)

/** All layers for vert/edge/face/loop. */
typedef struct BPy_BMLayerAccess {
  PyObject_VAR_HEAD
  struct BMesh *bm; /* keep first */
  char htype;
} BPy_BMLayerAccess;

/** Access different layer types deform/uv/vertex-color. */
typedef struct BPy_BMLayerCollection {
  PyObject_VAR_HEAD
  struct BMesh *bm; /* keep first */
  char htype;
  int type; /* customdata type - CD_XXX */
} BPy_BMLayerCollection;

/** Access a specific layer directly. */
typedef struct BPy_BMLayerItem {
  PyObject_VAR_HEAD
  struct BMesh *bm; /* keep first */
  char htype;
  int type;  /* customdata type - CD_XXX */
  int index; /* index of this layer type */
} BPy_BMLayerItem;

PyObject *BPy_BMLayerAccess_CreatePyObject(BMesh *bm, char htype);
PyObject *BPy_BMLayerCollection_CreatePyObject(BMesh *bm, char htype, int type);
PyObject *BPy_BMLayerItem_CreatePyObject(BMesh *bm, char htype, int type, int index);

void BPy_BM_init_types_customdata(void);

/**
 *\brief BMElem.__getitem__() / __setitem__()
 *
 * Assume all error checks are done, eg: `uv = vert[uv_layer]`
 */
PyObject *BPy_BMLayerItem_GetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer);
int BPy_BMLayerItem_SetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer, PyObject *value);

#ifdef __cplusplus
}
#endif
