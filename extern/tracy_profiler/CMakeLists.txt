# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include(ExternalProject)

set(TRACY_INSTALL_DIR ${CMAKE_BINARY_DIR})
# NOTE: Keep version in sync with the tracy client dependency in build_files/build_environment/cmake/versions.cmake.
set(TRACY_GIT_TAG a64b9a20294d59421a2f57aeca3c6383d8c48169)

# NOTE: Exceptionally, use tracy_profiler instead of `extern_tracy_profiler` as target name
# as this target is meant to be manually built by developers using `make/ninja tracy_profiler`.
# See `./README.blender` for more details.

function(add_tracy_profiler target_name use_legacy_x11_backend)
    ExternalProject_Add(${target_name}
      GIT_REPOSITORY https://github.com/wolfpld/tracy.git
      GIT_TAG ${TRACY_GIT_TAG}

      PREFIX ${CMAKE_BINARY_DIR}/${target_name}
      SOURCE_SUBDIR profiler
      EXCLUDE_FROM_ALL TRUE

      INSTALL_DIR ${TRACY_INSTALL_DIR}

      BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>

      CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${TRACY_INSTALL_DIR}
        -DCMAKE_DEBUG_POSTFIX=_d
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DLEGACY=${use_legacy_x11_backend}
    )
endfunction()

add_tracy_profiler(tracy_profiler OFF)

# Target variant to allow building using the "legacy" X11 backend.
add_tracy_profiler(tracy_profiler_x11 ON)
