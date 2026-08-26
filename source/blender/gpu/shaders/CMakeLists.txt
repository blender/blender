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
)

set(SRC_GLSL_VERT
  gpu_shader_3D_polyline_vert.glsl
  gpu_shader_display_fallback_vert.glsl
  gpu_shader_gpencil_stroke_vert.glsl
  gpu_shader_keyframe_shape_vert.glsl
  gpu_shader_sequencer_strips_vert.glsl
  gpu_shader_sequencer_thumbs_vert.glsl
  gpu_shader_text_vert.glsl
)

set(SRC_GLSL_FRAG
  gpu_shader_3D_polyline_frag.glsl
  gpu_shader_display_fallback_frag.glsl
  gpu_shader_gpencil_stroke_frag.glsl
  gpu_shader_keyframe_shape_frag.glsl
  gpu_shader_sequencer_scope_frag.glsl
  gpu_shader_sequencer_strips_frag.glsl
  gpu_shader_sequencer_thumbs_frag.glsl
  gpu_shader_sequencer_zebra_frag.glsl
  gpu_shader_text_frag.glsl
)

set(SRC_GLSL_COMP
  gpu_shader_sequencer_scope_comp.glsl
)

set(SRC_VULKAN_GLSL_COMP
  # TODO incorrect path, if corrected fails c++ compile
  # vk_backbuffer_blit_comp.glsl
)

set(SRC_BSL
  bsl_shader_linting.cc
)

if(WITH_VULKAN_BACKEND)
  list(APPEND SRC_GLSL_COMP ${SRC_VULKAN_GLSL_COMP})
endif()

set(SRC_GLSL_LIB
  common/gpu_shader_print_lib.glsl
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  compile_sources_as_cpp(gpu_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(gpu_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(gpu_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  compile_sources_as_cpp(gpu_cpp_shaders_bsl  "${SRC_BSL}" "")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(gpu_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
