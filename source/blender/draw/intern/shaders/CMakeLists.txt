# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..

  ../../../gpu
  ../../../gpu/intern
  ../../../gpu/shaders
  ../../../gpu/shaders/common

  # For grease pencil.
  ../../engines/gpencil
)

set(SRC_GLSL_VERT
  draw_debug_draw_display_vert.glsl
)

set(SRC_GLSL_FRAG
  draw_debug_draw_display_frag.glsl
)

set(SRC_GLSL_COMP
  common_hair_refine_comp.glsl
  draw_command_generate_comp.glsl
  # Failed because of mixed definition of ObjectInfos
  # draw_resource_finalize_comp.glsl
  draw_view_finalize_comp.glsl
  draw_visibility_comp.glsl
)

# Compile shaders with shader code.
if (WITH_GPU_SHADER_CPP_COMPILATION)
  with_shader_cpp_compilation_config()
  # TODO Remove
  add_definitions(-DUSE_GPU_SHADER_CREATE_INFO)

  compile_sources_as_cpp(draw_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(draw_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(draw_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
endif()
