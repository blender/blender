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
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup pybmesh
 */

#ifndef __BMESH_PY_TYPES_MESHDATA_H__
#define __BMESH_PY_TYPES_MESHDATA_H__

extern PyTypeObject BPy_BMLoopUV_Type;
extern PyTypeObject BPy_BMDeformVert_Type;

#define BPy_BMLoopUV_Check(v) (Py_TYPE(v) == &BPy_BMLoopUV_Type)

typedef struct BPy_BMGenericMeshData {
  PyObject_VAR_HEAD void *data;
} BPy_BMGenericMeshData;

struct MDeformVert;
struct MLoopCol;
struct MLoopUV;
struct MVertSkin;

int BPy_BMLoopUV_AssignPyObject(struct MLoopUV *data, PyObject *value);
PyObject *BPy_BMLoopUV_CreatePyObject(struct MLoopUV *data);

int BPy_BMVertSkin_AssignPyObject(struct MVertSkin *data, PyObject *value);
PyObject *BPy_BMVertSkin_CreatePyObject(struct MVertSkin *data);

int BPy_BMLoopColor_AssignPyObject(struct MLoopCol *data, PyObject *value);
PyObject *BPy_BMLoopColor_CreatePyObject(struct MLoopCol *data);

int BPy_BMDeformVert_AssignPyObject(struct MDeformVert *dvert, PyObject *value);
PyObject *BPy_BMDeformVert_CreatePyObject(struct MDeformVert *dvert);

void BPy_BM_init_types_meshdata(void);

#endif /* __BMESH_PY_TYPES_MESHDATA_H__ */
