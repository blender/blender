# SPDX-FileCopyrightText: 2012-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SDL_PATCH
  ${PATCH_CMD} -p 0 -N -d
    ${BUILD_DIR}/sdl/src/external_sdl <
    ${PATCH_DIR}/sdl.diff
)

if(WIN32)
  set(SDL_EXTRA_ARGS
    -DSDL_STATIC=Off
  )
else()
  set(SDL_EXTRA_ARGS
    -DSDL_STATIC=ON
    -DSDL_SHARED=OFF
    -DSDL_VIDEO=OFF
    -DSNDIO=OFF
  )

  # Core Haptics only available once macOS 11.0 becomes minimum.
  if(APPLE AND NOT BLENDER_PLATFORM_ARM)
    list(APPEND SDL_EXTRA_ARGS -DSDL_HAPTICS=OFF)
    set(SDL_PATCH
      ${SDL_PATCH} &&
      ${PATCH_CMD} -p 0 -N -d
        ${BUILD_DIR}/sdl/src/external_sdl <
        ${PATCH_DIR}/sdl_haptics.diff
    )
  endif()
endif()

ExternalProject_Add(external_sdl
  URL file://${PACKAGE_DIR}/${SDL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SDL_HASH_TYPE}=${SDL_HASH}
  PREFIX ${BUILD_DIR}/sdl
  PATCH_COMMAND ${SDL_PATCH}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/sdl
    ${DEFAULT_CMAKE_FLAGS}
    ${SDL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/sdl
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_sdl after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/sdl/include/sdl2
      ${HARVEST_TARGET}/sdl/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/sdl/lib
      ${HARVEST_TARGET}/sdl/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/sdl/bin
      ${HARVEST_TARGET}/sdl/lib

    DEPENDEES install
  )
endif()
