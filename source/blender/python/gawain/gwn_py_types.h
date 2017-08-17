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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/gawain/gwn_py_types.h
 *  \ingroup pygawain
 */

#ifndef __GWN_PY_TYPES_H__
#define __GWN_PY_TYPES_H__

#include "BLI_compiler_attrs.h"

#define USE_GWN_PY_REFERENCES

extern PyTypeObject BPy_Gwn_VertFormat_Type;
extern PyTypeObject BPy_Gwn_VertBuf_Type;
extern PyTypeObject BPy_Gwn_Batch_Type;

#define BPy_Gwn_VertFormat_Check(v)     (Py_TYPE(v) == &BPy_Gwn_VertFormat_Type)
#define BPy_Gwn_VertBuf_Check(v)        (Py_TYPE(v) == &BPy_Gwn_VertBuf_Type)
#define BPy_Gwn_Batch_Check(v)          (Py_TYPE(v) == &BPy_Gwn_Batch_Type)

typedef struct BPy_Gwn_VertFormat {
	PyObject_VAR_HEAD
	struct Gwn_VertFormat fmt;
} BPy_Gwn_VertFormat;

typedef struct BPy_Gwn_VertBuf {
	PyObject_VAR_HEAD
	/* The buf is owned, we may support thin wrapped batches later. */
	struct Gwn_VertBuf *buf;
} BPy_Gwn_VertBuf;

typedef struct BPy_Gwn_Batch {
	PyObject_VAR_HEAD
	/* The batch is owned, we may support thin wrapped batches later. */
	struct Gwn_Batch *batch;
#ifdef USE_GWN_PY_REFERENCES
	/* Just to keep a user to prevent freeing buf's we're using */
	PyObject *references;
#endif
} BPy_Gwn_Batch;

PyObject *BPyInit_gawain_types(void);

PyObject *BPy_Gwn_VertFormat_CreatePyObject(struct Gwn_VertFormat *fmt);
PyObject *BPy_Gwn_VertBuf_CreatePyObject(struct Gwn_VertBuf *vbo) ATTR_NONNULL(1);
PyObject *BPy_Gwn_Batch_CreatePyObject(struct Gwn_Batch *batch) ATTR_NONNULL(1);

#endif /* __GWN_PY_TYPES_H__ */
