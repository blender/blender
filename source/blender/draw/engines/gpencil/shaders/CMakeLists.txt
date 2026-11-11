# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..

  # For variadic macros
  ../../../../blenlib

  ../../../intern
  ../../../intern/shaders

  ../../../../gpu
  ../../../../gpu/intern
  ../../../../gpu/shaders
  ../../../../gpu/shaders/common
  ../../../../gpu/shaders/compat
  ../../../../gpu/shaders/infos
  ../../../../gpu/shaders/nodes
)

set(SRC_GLSL_VERT
  gpencil_antialiasing_vert.glsl
  gpencil_vert.glsl
)

set(SRC_GLSL_FRAG
  gpencil_antialiasing_frag.glsl
  gpencil_frag.glsl
)


set(SRC_BSL
  bsl_shader_linting.cc
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  compile_sources_as_cpp(gpencil_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(gpencil_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(gpencil_cpp_shaders_bsl  "${SRC_BSL}" "")
endif()
