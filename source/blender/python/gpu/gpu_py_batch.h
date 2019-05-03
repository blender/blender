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
 * \ingroup bpygpu
 */

#ifndef __GPU_PY_BATCH_H__
#define __GPU_PY_BATCH_H__

#include "BLI_compiler_attrs.h"

#define USE_GPU_PY_REFERENCES

extern PyTypeObject BPyGPUBatch_Type;

#define BPyGPUBatch_Check(v) (Py_TYPE(v) == &BPyGPUBatch_Type)

typedef struct BPyGPUBatch {
  PyObject_VAR_HEAD
      /* The batch is owned, we may support thin wrapped batches later. */
      struct GPUBatch *batch;
#ifdef USE_GPU_PY_REFERENCES
  /* Just to keep a user to prevent freeing buf's we're using */
  PyObject *references;
#endif
} BPyGPUBatch;

PyObject *BPyGPUBatch_CreatePyObject(struct GPUBatch *batch) ATTR_NONNULL(1);

#endif /* __GPU_PY_BATCH_H__ */
