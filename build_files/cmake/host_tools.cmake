# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Host-architecture code generation tools for cross-compilation.
#
# Blender provides build-time code-gen tools (makesdna, makesrna, datatoc, shader_tool)
# that must run on the host machine performing the build, not on the cross-compile target.
#
# When CMAKE_CROSSCOMPILING is TRUE, this module spawns a second Blender configure/build
# under <build_dir>/host_tools using the host's default  compiler (no toolchain file),
# and exposes the resulting binaries  via BLENDER_*_EXE variables. BLENDER_*_DEPENDENCY
# variables are also  exposed to allow for consumer targets to depend on the host_tools
# target, as the tool targets do not exist in the cross-compile case.

if(CMAKE_CROSSCOMPILING)
  include(ExternalProject)

  set(HOST_TOOLS_BUILD_DIR ${CMAKE_BINARY_DIR}/host_tools)
  set(HOST_TOOLS_BIN_DIR ${HOST_TOOLS_BUILD_DIR}/bin)

  # Forward every WITH_* cache variable so the host build's preprocessor
  # state matches the target build's.
  get_cmake_property(_host_tools_all_cache_vars CACHE_VARIABLES)
  set(_host_tools_forwarded_args "")
  foreach(_v IN LISTS _host_tools_all_cache_vars)
    if(_v MATCHES "^WITH_")
      list(APPEND _host_tools_forwarded_args "-D${_v}=${${_v}}")
    endif()
  endforeach()
  unset(_host_tools_all_cache_vars)

  ExternalProject_Add(host_tools
    SOURCE_DIR ${CMAKE_SOURCE_DIR}
    BINARY_DIR ${HOST_TOOLS_BUILD_DIR}
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      ${_host_tools_forwarded_args}

    # Ensure the target is always checked for incremental compilation / tool rebuild on file changes.
    BUILD_ALWAYS TRUE
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
                  --target makesdna
                  --target makesrna
                  --target datatoc
                  --target shader_tool
    INSTALL_COMMAND ""

    # Nicer build output on ninja: hide verbose configure, show build steps
    USES_TERMINAL_CONFIGURE FALSE
    USES_TERMINAL_BUILD TRUE

    BUILD_BYPRODUCTS
      ${HOST_TOOLS_BIN_DIR}/makesdna
      ${HOST_TOOLS_BIN_DIR}/makesrna
      ${HOST_TOOLS_BIN_DIR}/datatoc
      ${HOST_TOOLS_BIN_DIR}/shader_tool
  )
  unset(_host_tools_forwarded_args)

  set(MAKESDNA_EXECUTABLE     ${HOST_TOOLS_BIN_DIR}/makesdna)
  set(MAKESRNA_EXECUTABLE     ${HOST_TOOLS_BIN_DIR}/makesrna)
  set(DATATOC_EXECUTABLE      ${HOST_TOOLS_BIN_DIR}/datatoc)
  set(SHADER_TOOL_EXECUTABLE  ${HOST_TOOLS_BIN_DIR}/shader_tool)

  set(MAKESDNA_DEPENDENCY     host_tools)
  set(MAKESRNA_DEPENDENCY     host_tools)
  set(DATATOC_DEPENDENCY      host_tools)
  set(SHADER_TOOL_DEPENDENCY  host_tools)
else()
  set(MAKESDNA_EXECUTABLE     "$<TARGET_FILE:makesdna>")
  set(MAKESRNA_EXECUTABLE     "$<TARGET_FILE:makesrna>")
  set(DATATOC_EXECUTABLE      "$<TARGET_FILE:datatoc>")
  set(SHADER_TOOL_EXECUTABLE  "$<TARGET_FILE:shader_tool>")

  set(MAKESDNA_DEPENDENCY     makesdna)
  set(MAKESRNA_DEPENDENCY     makesrna)
  set(DATATOC_DEPENDENCY      datatoc)
  set(SHADER_TOOL_DEPENDENCY  shader_tool)
endif()
