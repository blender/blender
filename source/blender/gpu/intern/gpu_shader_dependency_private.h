/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Shader source dependency builder that make possible to support #include directive inside the
 * shader files.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void gpu_shader_dependency_init(void);

void gpu_shader_dependency_exit(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BLI_string_ref.hh"
#  include "BLI_vector.hh"

#  include "gpu_shader_create_info.hh"

namespace blender::gpu::shader {

BuiltinBits gpu_shader_dependency_get_builtins(const StringRefNull source_name);

Vector<const char *> gpu_shader_dependency_get_resolved_source(const StringRefNull source_name);
StringRefNull gpu_shader_dependency_get_source(const StringRefNull source_name);

/**
 * \brief Find the name of the file from which the given string was generated.
 * \return filename or empty string.
 * \note source_string needs to be identical to the one given by gpu_shader_dependency_get_source()
 */
StringRefNull gpu_shader_dependency_get_filename_from_source_string(
    const StringRefNull source_string);

}  // namespace blender::gpu::shader

#endif
