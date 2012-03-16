/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_types_customdata.h
 *  \ingroup pybmesh
 */

#ifndef __BMESH_PY_TYPES_CUSTOMDATA_H__
#define __BMESH_PY_TYPES_CUSTOMDATA_H__

extern PyTypeObject BPy_BMLayerAccess_Type;
extern PyTypeObject BPy_BMLayerCollection_Type;
extern PyTypeObject BPy_BMLayerItem_Type;

#define BPy_BMLayerAccess_Check(v)      (Py_TYPE(v) == &BPy_BMLayerAccess_Type)
#define BPy_BMLayerCollection_Check(v)  (Py_TYPE(v) == &BPy_BMLayerCollection_Type)
#define BPy_BMLayerItem_Check(v)        (Py_TYPE(v) == &BPy_BMLayerItem_Type)

/* all layers for vert/edge/face/loop */
typedef struct BPy_BMLayerAccess {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	char htype;
} BPy_BMLayerAccess;

/* access different layer types deform/uv/vertexcolor */
typedef struct BPy_BMLayerCollection {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	char htype;
	int  type; /* customdata type - CD_XXX */
} BPy_BMLayerCollection;

/* access a spesific layer directly */
typedef struct BPy_BMLayerItem {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	char htype;
	int  type;  /* customdata type - CD_XXX */
	int  index; /* index of this layer type */
} BPy_BMLayerItem;

PyObject *BPy_BMLayerAccess_CreatePyObject(BMesh *bm, const char htype);
PyObject *BPy_BMLayerCollection_CreatePyObject(BMesh *bm, const char htype, int type);
PyObject *BPy_BMLayerItem_CreatePyObject(BMesh *bm, const char htype, int type, int index);

void BPy_BM_init_types_customdata(void);

#endif /* __BMESH_PY_TYPES_CUSTOMDATA_H__ */
