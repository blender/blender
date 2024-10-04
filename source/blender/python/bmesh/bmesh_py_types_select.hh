/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#pragma once

struct BBMesh;
struct BMEditSelection;
struct BPy_BMesh;

extern PyTypeObject BPy_BMEditSelSeq_Type;
extern PyTypeObject BPy_BMEditSelIter_Type;

#define BPy_BMSelectHistory_Check(v) (Py_TYPE(v) == &BPy_BMEditSelSeq_Type)
#define BPy_BMSelectHistoryIter_Check(v) (Py_TYPE(v) == &BPy_BMEditSelIter_Type)

struct BPy_BMEditSelSeq {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
};

struct BPy_BMEditSelIter {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMEditSelection *ese;
};

void BPy_BM_init_types_select();

PyObject *BPy_BMEditSel_CreatePyObject(BMesh *bm);
PyObject *BPy_BMEditSelIter_CreatePyObject(BMesh *bm);
/**
 * \note doesn't actually check selection.
 */
int BPy_BMEditSel_Assign(BPy_BMesh *self, PyObject *value);
