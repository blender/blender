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

#include "gpu_py_buffer.h"

#include "gpu_py_batch.h"
#include "gpu_py_element.h"
#include "gpu_py_framebuffer.h"
#include "gpu_py_offscreen.h"
#include "gpu_py_shader.h"
#include "gpu_py_texture.h"
#include "gpu_py_uniformbuffer.h"
#include "gpu_py_vertex_buffer.h"
#include "gpu_py_vertex_format.h"

PyObject *bpygpu_types_init(void);
