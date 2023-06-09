/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#pragma once

extern PyTypeObject BPy_BMLoopUV_Type;
extern PyTypeObject BPy_BMDeformVert_Type;

#define BPy_BMLoopUV_Check(v) (Py_TYPE(v) == &BPy_BMLoopUV_Type)

typedef struct BPy_BMGenericMeshData {
  PyObject_VAR_HEAD
  void *data;
} BPy_BMGenericMeshData;

struct MDeformVert;
struct MLoopCol;
struct MVertSkin;
struct BMesh;

int BPy_BMLoopUV_AssignPyObject(struct BMesh *bm, BMLoop *loop, PyObject *value);
PyObject *BPy_BMLoopUV_CreatePyObject(struct BMesh *bm, BMLoop *loop, int layer);

int BPy_BMVertSkin_AssignPyObject(struct MVertSkin *mvertskin, PyObject *value);
PyObject *BPy_BMVertSkin_CreatePyObject(struct MVertSkin *mvertskin);

int BPy_BMLoopColor_AssignPyObject(struct MLoopCol *mloopcol, PyObject *value);
PyObject *BPy_BMLoopColor_CreatePyObject(struct MLoopCol *mloopcol);

int BPy_BMDeformVert_AssignPyObject(struct MDeformVert *dvert, PyObject *value);
PyObject *BPy_BMDeformVert_CreatePyObject(struct MDeformVert *dvert);

/* call to init all types */
void BPy_BM_init_types_meshdata(void);
