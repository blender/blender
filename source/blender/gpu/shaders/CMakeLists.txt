# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..
  ../intern

  # For variadic macros
  ../../blenlib

  common
  infos
  compat
)

set(SRC_BSL
  bsl_shader_linting.cc
)

set(SRC_GLSL_LIB
  common/gpu_shader_print_lib.glsl
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  compile_sources_as_cpp(gpu_cpp_shaders_bsl  "${SRC_BSL}" "")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(gpu_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
