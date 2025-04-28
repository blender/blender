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
  ../../../../gpu/shaders/infos
)

set(SRC_GLSL_VERT
  gpencil_antialiasing_vert.glsl
  gpencil_depth_merge_vert.glsl
  gpencil_vert.glsl
)

set(SRC_GLSL_FRAG
  gpencil_antialiasing_accumulation_frag.glsl
  gpencil_antialiasing_frag.glsl
  gpencil_depth_merge_frag.glsl
  gpencil_frag.glsl
  gpencil_layer_blend_frag.glsl
  gpencil_mask_invert_frag.glsl
  gpencil_vfx_frag.glsl
)

set(SRC_GLSL_COMP
)

set(SRC_GLSL_LIB
  gpencil_common_lib.glsl
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  with_shader_cpp_compilation_config()

  compile_sources_as_cpp(gpencil_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(gpencil_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  # compile_sources_as_cpp(gpencil_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(gpencil_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
