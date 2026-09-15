# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..
  ../..
  ../../..
  ../../../intern

  # For variadic macros
  ../../../blenlib

  ../common
  ../compat
  ../infos
  ../material
)

set(SRC_BSL
  bsl_shader_linting.cc
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  compile_sources_as_cpp(gpu_cpp_shaders_material_bsl  "${SRC_BSL}" "")
endif()
