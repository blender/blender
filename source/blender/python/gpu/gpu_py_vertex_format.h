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

#ifndef __GPU_PY_VERTEX_FORMAT_H__
#define __GPU_PY_VERTEX_FORMAT_H__

#include "GPU_vertex_format.h"

extern PyTypeObject BPyGPUVertFormat_Type;

#define BPyGPUVertFormat_Check(v) (Py_TYPE(v) == &BPyGPUVertFormat_Type)

typedef struct BPyGPUVertFormat {
  PyObject_VAR_HEAD struct GPUVertFormat fmt;
} BPyGPUVertFormat;

PyObject *BPyGPUVertFormat_CreatePyObject(struct GPUVertFormat *fmt);

#endif /* __GPU_PY_VERTEX_FORMAT_H__ */
