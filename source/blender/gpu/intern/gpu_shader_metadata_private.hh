/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Shader source dependency builder that make possible to support #include directive inside the
 * shader files.
 */

#pragma once

#include "glsl_preprocess/glsl_preprocess.hh"
#include "gpu_shader_create_info.hh"

namespace blender::gpu::shader {

BuiltinBits convert_builtin_bit(metadata::Builtin builtin);

}  // namespace blender::gpu::shader
