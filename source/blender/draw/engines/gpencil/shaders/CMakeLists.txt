# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# avoid noisy warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_c_flag(
    "-Wno-unused-result"
  )
  remove_cc_flag(
    "-Wmissing-declarations"
  )
endif()

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
  ../../../../gpu/shaders/infos
  ../../../../gpu/shaders/common
)

set(SRC_GLSL_VERT
  gpencil_antialiasing_vert.glsl
  gpencil_depth_merge_vert.glsl
  gpencil_vert.glsl
)

set(SRC_GLSL_FRAG
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

add_definitions(-DGPU_SHADER)

function(compile_sources_as_cpp
  executable
  sources
  define
  )

  foreach(glsl_file ${sources})
    set_source_files_properties(${glsl_file} PROPERTIES LANGUAGE CXX)
  endforeach()

  add_library(${executable} OBJECT ${sources})
  set_target_properties(${executable} PROPERTIES LINKER_LANGUAGE CXX)
  target_include_directories(${executable} PUBLIC ${INC_GLSL})
  target_compile_definitions(${executable} PRIVATE ${define})
endfunction()


# Compile shaders with shader code.
if (WITH_GPU_SHADER_CPP_COMPILATION)
  compile_sources_as_cpp(gpencil_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(gpencil_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  # compile_sources_as_cpp(gpencil_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(gpencil_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
