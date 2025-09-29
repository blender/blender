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

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "gpu_shader_create_info.hh"

void gpu_shader_dependency_init();

void gpu_shader_dependency_exit();

namespace blender::gpu::shader {

BuiltinBits gpu_shader_dependency_get_builtins(const StringRefNull source_name);

/* Returns true is any shader code has a printf statement. */
bool gpu_shader_dependency_has_printf();

bool gpu_shader_dependency_force_gpu_print_injection();

struct PrintfFormat {
  struct Block {
    enum ArgumentType {
      NONE = 0,
      UINT,
      INT,
      FLOAT,
    } type = NONE;
    std::string fmt;
  };

  Vector<Block> format_blocks;
  std::string format_str;
};

const PrintfFormat &gpu_shader_dependency_get_printf_format(uint32_t format_hash);

Vector<StringRefNull> gpu_shader_dependency_get_resolved_source(
    StringRefNull source_name,
    const GeneratedSourceList &generated_sources,
    StringRefNull shader_name = "");
StringRefNull gpu_shader_dependency_get_source(StringRefNull source_name);

/**
 * \brief Find the name of the file from which the given string was generated.
 * \return filename or empty string.
 * \note source_string needs to be identical to the one given by gpu_shader_dependency_get_source()
 */
StringRefNull gpu_shader_dependency_get_filename_from_source_string(StringRef source_string);

}  // namespace blender::gpu::shader
