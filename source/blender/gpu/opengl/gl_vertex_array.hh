/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_batch.hh"
#include "gl_shader_interface.hh"

namespace blender::gpu::GLVertArray {

/**
 * Update the Attribute Binding of the currently bound VAO.
 */
void update_bindings(const GLuint vao, const Batch *batch, const ShaderInterface *interface);

/**
 * Another version of update_bindings for Immediate mode.
 */
void update_bindings(const GLuint vao,
                     uint v_first,
                     const GPUVertFormat *format,
                     const ShaderInterface *interface);

}  // namespace blender::gpu::GLVertArray
