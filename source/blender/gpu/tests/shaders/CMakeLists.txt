# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..
  ../..
  ../../intern

  ../../shaders/common
  ../../shaders
)

set(SRC_GLSL_VERT
  gpu_framebuffer_layer_viewport_test.glsl
  gpu_framebuffer_subpass_input_test.glsl
  gpu_texture_test.glsl
)

set(SRC_GLSL_FRAG
  gpu_framebuffer_layer_viewport_test.glsl
  gpu_framebuffer_layer_viewport_test.glsl
  gpu_framebuffer_subpass_input_test.glsl
  gpu_math_test.glsl
  gpu_specialization_test.glsl
  gpu_texture_test.glsl
)

set(SRC_GLSL_COMP
  gpu_buffer_texture_test.glsl
  gpu_compute_1d_test.glsl
  gpu_compute_2d_test.glsl
  gpu_compute_dummy_test.glsl
  gpu_compute_ibo_test.glsl
  gpu_compute_ssbo_test.glsl
  gpu_compute_vbo_test.glsl
  gpu_push_constants_test.glsl
  gpu_specialization_test.glsl
  gpu_texture_atomic_test.glsl
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  with_shader_cpp_compilation_config()

  compile_sources_as_cpp(gpu_test_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(gpu_test_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(gpu_test_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(gpu_test_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
