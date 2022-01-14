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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "glew-mx.h"

#include "GPU_batch.h"
#include "gl_shader_interface.hh"

namespace blender {
namespace gpu {

namespace GLVertArray {

/**
 * Update the Attribute Binding of the currently bound VAO.
 */
void update_bindings(const GLuint vao,
                     const GPUBatch *batch,
                     const ShaderInterface *interface,
                     int base_instance);

/**
 * Another version of update_bindings for Immediate mode.
 */
void update_bindings(const GLuint vao,
                     uint v_first,
                     const GPUVertFormat *format,
                     const ShaderInterface *interface);

}  // namespace GLVertArray

}  // namespace gpu
}  // namespace blender
