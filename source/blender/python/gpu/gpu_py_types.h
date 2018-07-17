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

/** \file blender/python/gpu/gpu_py_types.h
 *  \ingroup pygpu
 */

#ifndef __GPU_PY_TYPES_H__
#define __GPU_PY_TYPES_H__

#include "BLI_compiler_attrs.h"

#define USE_GPU_PY_REFERENCES

extern PyTypeObject BPyGPUVertFormat_Type;
extern PyTypeObject BPyGPUVertBuf_Type;
extern PyTypeObject BPyGPUBatch_Type;

#define BPyGPUVertFormat_Check(v)     (Py_TYPE(v) == &BPyGPUVertFormat_Type)
#define BPyGPUVertBuf_Check(v)        (Py_TYPE(v) == &BPyGPUVertBuf_Type)
#define BPyGPUBatch_Check(v)          (Py_TYPE(v) == &BPyGPUBatch_Type)

typedef struct BPyGPUVertFormat {
	PyObject_VAR_HEAD
	struct GPUVertFormat fmt;
} BPyGPUVertFormat;

typedef struct BPyGPUVertBuf {
	PyObject_VAR_HEAD
	/* The buf is owned, we may support thin wrapped batches later. */
	struct GPUVertBuf *buf;
} BPyGPUVertBuf;

typedef struct BPyGPUBatch {
	PyObject_VAR_HEAD
	/* The batch is owned, we may support thin wrapped batches later. */
	struct GPUBatch *batch;
#ifdef USE_GPU_PY_REFERENCES
	/* Just to keep a user to prevent freeing buf's we're using */
	PyObject *references;
#endif
} BPyGPUBatch;

PyObject *BPyInit_gpu_types(void);

PyObject *BPyGPUVertFormat_CreatePyObject(struct GPUVertFormat *fmt);
PyObject *BPyGPUVertBuf_CreatePyObject(struct GPUVertBuf *vbo) ATTR_NONNULL(1);
PyObject *BPyGPUBatch_CreatePyObject(struct GPUBatch *batch) ATTR_NONNULL(1);

#endif /* __GPU_PY_TYPES_H__ */
