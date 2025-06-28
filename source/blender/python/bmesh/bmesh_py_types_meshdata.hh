/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#pragma once

#include <Python.h>

#include "bmesh.hh"

extern PyTypeObject BPy_BMLoopUV_Type;
extern PyTypeObject BPy_BMDeformVert_Type;

#define BPy_BMLoopUV_Check(v) (Py_TYPE(v) == &BPy_BMLoopUV_Type)

struct BPy_BMGenericMeshData {
  PyObject_VAR_HEAD
  void *data;
};

struct MDeformVert;
struct MLoopCol;
struct MVertSkin;
struct BMesh;

[[nodiscard]] int BPy_BMLoopUV_AssignPyObject(struct BMesh *bm, BMLoop *loop, PyObject *value);
[[nodiscard]] PyObject *BPy_BMLoopUV_CreatePyObject(struct BMesh *bm, BMLoop *loop, int layer);

[[nodiscard]] int BPy_BMVertSkin_AssignPyObject(struct MVertSkin *mvertskin, PyObject *value);
[[nodiscard]] PyObject *BPy_BMVertSkin_CreatePyObject(struct MVertSkin *mvertskin);

[[nodiscard]] int BPy_BMLoopColor_AssignPyObject(struct MLoopCol *mloopcol, PyObject *value);
[[nodiscard]] PyObject *BPy_BMLoopColor_CreatePyObject(struct MLoopCol *mloopcol);

[[nodiscard]] int BPy_BMDeformVert_AssignPyObject(struct MDeformVert *dvert, PyObject *value);
[[nodiscard]] PyObject *BPy_BMDeformVert_CreatePyObject(struct MDeformVert *dvert);

/* call to init all types */
void BPy_BM_init_types_meshdata();
