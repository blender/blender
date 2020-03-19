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

#ifndef __GPU_PY_TYPES_H__
#define __GPU_PY_TYPES_H__

#include "gpu_py_batch.h"
#include "gpu_py_element.h"
#include "gpu_py_offscreen.h"
#include "gpu_py_shader.h"
#include "gpu_py_vertex_buffer.h"
#include "gpu_py_vertex_format.h"

PyObject *BPyInit_gpu_types(void);

#endif /* __GPU_PY_TYPES_H__ */
