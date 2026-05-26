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

  # For grease pencil.
  ../../gpencil
)


set(SRC_GLSL_LIB
  eevee_attributes_curves_lib.glsl
  eevee_attributes_mesh_lib.glsl
  eevee_attributes_pointcloud_lib.glsl
  eevee_attributes_volume_lib.glsl
  eevee_attributes_world_lib.glsl
  # eevee_nodetree_lib.glsl # Has dependency on draw_model_lib that is hard to resolve
)

set(SRC_BSL
  bsl_shader_linting.cc
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  # TODO Remove
  add_definitions(-DUSE_GPU_SHADER_CREATE_INFO)

  compile_sources_as_cpp(eevee_cpp_shaders_bsl  "${SRC_BSL}" "")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(eevee_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
