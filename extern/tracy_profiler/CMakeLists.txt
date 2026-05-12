# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include(ExternalProject)

set(TRACY_INSTALL_DIR ${CMAKE_BINARY_DIR})

# NOTE: Exceptionally, use tracy_profiler instead of `extern_tracy_profiler` as target name
# as this target is meant to be manually built by developers using `make/ninja tracy_profiler`.
# See `./README.blender` for more details.
ExternalProject_Add(tracy_profiler
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git

  # NOTE: Keep version in sync with the tracy client dependency in:
  # `build_files/build_environment/cmake/versions.cmake`
  GIT_TAG a64b9a20294d59421a2f57aeca3c6383d8c48169
  PREFIX ${CMAKE_BINARY_DIR}/tracy_profiler
  SOURCE_SUBDIR profiler
  EXCLUDE_FROM_ALL TRUE

  INSTALL_DIR ${TRACY_INSTALL_DIR}

  BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>

  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX:PATH=${TRACY_INSTALL_DIR}
  -DCMAKE_DEBUG_POSTFIX=_d
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)
