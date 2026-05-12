# SPDX-FileCopyrightText: 2012-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SDL_EXTRA_ARGS
  -DSDL_STATIC=OFF
  -DSDL_SHARED=ON
  -DSDL_TESTS=OFF
  -DSDL_TEST_LIBRARY=OFF
  -DSDL_SNDIO=OFF
)

if(UNIX AND NOT APPLE)
  set(SDL_EXTRA_ARGS
    ${SDL_EXTRA_ARGS}
    -DSDL_X11_XSCRNSAVER=OFF
    -DSDL_X11_XTEST=OFF
  )
endif()

ExternalProject_Add(external_sdl
  URL file://${PACKAGE_DIR}/${SDL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SDL_HASH_TYPE}=${SDL_HASH}
  PREFIX ${BUILD_DIR}/sdl
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/sdl
    ${DEFAULT_CMAKE_FLAGS}
    ${SDL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/sdl
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_sdl after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/sdl
        ${HARVEST_TARGET}/sdl

      DEPENDEES install
    )
  endif()
else()
  harvest(external_sdl sdl/include sdl/include "*.h")
  # CMake files first because harvest_rpath_lib edits them.
  harvest(external_sdl sdl/lib/cmake/SDL3 sdl/lib/cmake/SDL3 "*.cmake")
  harvest_rpath_lib(external_sdl sdl/lib sdl/lib "*${SHAREDLIBEXT}*")
endif()
