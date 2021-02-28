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

#pragma once

#include "BLI_compiler_attrs.h"

extern PyTypeObject BPyGPUTexture_Type;

#define BPyGPUTexture_Check(v) (Py_TYPE(v) == &BPyGPUTexture_Type)

typedef struct BPyGPUTexture {
  PyObject_HEAD struct GPUTexture *tex;
} BPyGPUTexture;

int bpygpu_ParseTexture(PyObject *o, void *p);
PyObject *BPyGPUTexture_CreatePyObject(struct GPUTexture *tex) ATTR_NONNULL(1);
