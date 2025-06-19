/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_compilation_subprocess.hh"

#if BLI_SUBPROCESS_SUPPORT

#  include "BLI_sys_types.h"

namespace blender::gpu {

/* The size of the memory pools shared by Blender and the compilation subprocesses. */
constexpr size_t compilation_subprocess_shared_memory_size = 1024 * 1024 * 5; /* 5 MiB */

struct ShaderSourceHeader {
  enum Type { COMPUTE, GRAPHICS, GRAPHICS_WITH_GEOMETRY_STAGE };
  /* The type of program being compiled. */
  Type type;
  /* The source code for all the shader stages (Separated by a null terminator).
   * The stages follows the execution order (eg. vert > geom > frag). */
  char sources[compilation_subprocess_shared_memory_size - sizeof(type)];
};

static_assert(sizeof(ShaderSourceHeader) == compilation_subprocess_shared_memory_size,
              "Size must match the shared memory size");

struct ShaderBinaryHeader {
  /* Size of the shader binary data. */
  int32_t size;
  /* Magic number that identifies the format of this shader binary (Driver-defined).
   * This (and size) is set to 0 when the shader has failed to compile. */
  uint32_t format;
  /* The serialized shader binary data. */
  uint8_t data[compilation_subprocess_shared_memory_size - sizeof(size) - sizeof(format)];
};

static_assert(sizeof(ShaderBinaryHeader) == compilation_subprocess_shared_memory_size,
              "Size must match the shared memory size");

void GL_shader_cache_dir_clear_old();

std::string GL_shader_cache_dir_get();

}  // namespace blender::gpu

#endif
