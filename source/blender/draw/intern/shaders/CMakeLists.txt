# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..

  # For variadic macros
  ../../../blenlib

  ../../../gpu
  ../../../gpu/intern
  ../../../gpu/shaders
  ../../../gpu/shaders/common
  ../../../gpu/shaders/compat
  ../../../gpu/shaders/compat

  # For grease pencil.
  ../../engines/gpencil
)

set(SRC_BSL
  bsl_shader_linting.cc
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  compile_sources_as_cpp(draw_cpp_shaders_bsl  "${SRC_BSL}" "")
endif()
