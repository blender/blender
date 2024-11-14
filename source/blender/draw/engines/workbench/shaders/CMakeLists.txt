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
  workbench_effect_smaa_vert.glsl
  workbench_prepass_hair_vert.glsl
  workbench_prepass_pointcloud_vert.glsl
  workbench_prepass_vert.glsl
  workbench_shadow_caps_vert.glsl
  workbench_shadow_vert.glsl
  workbench_volume_vert.glsl
)

set(SRC_GLSL_FRAG
  workbench_composite_frag.glsl
  workbench_effect_dof_frag.glsl
  workbench_effect_outline_frag.glsl
  workbench_effect_smaa_frag.glsl
  workbench_effect_taa_frag.glsl
  workbench_extract_stencil.glsl
  workbench_merge_depth_frag.glsl
  workbench_overlay_depth_frag.glsl
  workbench_prepass_frag.glsl
  workbench_shadow_debug_frag.glsl
  workbench_transparent_accum_frag.glsl
  workbench_transparent_resolve_frag.glsl
  workbench_volume_frag.glsl
)

set(SRC_GLSL_COMP
  workbench_shadow_visibility_comp.glsl
)

set(SRC_GLSL_LIB
  workbench_cavity_lib.glsl
  workbench_common_lib.glsl
  workbench_curvature_lib.glsl
  workbench_image_lib.glsl
  workbench_matcap_lib.glsl
  workbench_material_lib.glsl
  workbench_shadow_lib.glsl
  workbench_world_light_lib.glsl
)

add_definitions(-DGPU_SHADER)
# TODO Remove
add_definitions(-DUSE_GPU_SHADER_CREATE_INFO)

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
  compile_sources_as_cpp(workbench_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(workbench_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(workbench_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about pragma once.
  # compile_sources_as_cpp(workbench_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
