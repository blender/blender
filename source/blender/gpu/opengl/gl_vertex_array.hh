/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

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
