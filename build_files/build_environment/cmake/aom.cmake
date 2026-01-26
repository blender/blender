# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(BLENDER_PLATFORM_WINDOWS_ARM)
  set(AOM_EXTRA_ARGS_WIN32 -DAOM_TARGET_CPU=generic)
else()
  set(AOM_CMAKE_FLAGS ${DEFAULT_CMAKE_FLAGS})
endif()

set(AOM_EXTRA_ARGS
  -DENABLE_TESTDATA=OFF
  -DENABLE_TESTS=OFF
  -DENABLE_TOOLS=OFF
  -DENABLE_EXAMPLES=OFF
  -DBUILD_SHARED_LIBS=ON
  ${AOM_EXTRA_ARGS_WIN32}
)
if(WIN32 AND NOT BLENDER_PLATFORM_WINDOWS_ARM)
  set(AOM_EXTRA_ARGS ${AOM_EXTRA_ARGS}-DCMAKE_ASM_NASM_COMPILER=)
endif()

if(WIN32)
  # This is slightly different from all other deps in the way that
  # aom uses cmake as a build system, but still needs the environment setup
  # to include perl so we manually setup the environment and call
  # cmake directly for the configure, build and install commands.

  ExternalProject_Add(external_aom
    URL file://${PACKAGE_DIR}/${AOM_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${AOM_HASH_TYPE}=${AOM_HASH}
    PREFIX ${BUILD_DIR}/aom

    PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d
      ${BUILD_DIR}/aom/src/external_aom <
      ${PATCH_DIR}/aom_6d2b7f71b98bfa28e372b1f2d85f137280bdb3de.diff

    CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/aom/src/external_aom-build/ &&
      ${CMAKE_COMMAND}
        -DCMAKE_INSTALL_PREFIX=${LIBDIR}/aom
        -G "${PLATFORM_ALT_GENERATOR}"
        ${AOM_CMAKE_FLAGS}
        ${AOM_EXTRA_ARGS}
        ${BUILD_DIR}/aom/src/external_aom/

    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install
    INSTALL_DIR ${LIBDIR}/aom
)
else()
  ExternalProject_Add(external_aom
    URL file://${PACKAGE_DIR}/${AOM_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${AOM_HASH_TYPE}=${AOM_HASH}
    CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
    PREFIX ${BUILD_DIR}/aom

    PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d
      ${BUILD_DIR}/aom/src/external_aom <
      ${PATCH_DIR}/aom_6d2b7f71b98bfa28e372b1f2d85f137280bdb3de.diff

    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${LIBDIR}/aom
      ${AOM_CMAKE_FLAGS}
      ${AOM_EXTRA_ARGS}

    INSTALL_DIR ${LIBDIR}/aom
  )
endif()

if(NOT WIN32)
  harvest(external_aom aom/lib ffmpeg/lib "*.a")
else()
    if(BUILD_MODE STREQUAL Release)
      # aom insists on building a static version even if you
      # do not want it, get rid of it by copying the import lib
      # on top of it, so other projects don't accidentally link
      # the static library.
      ExternalProject_Add_Step(external_aom after_install
        COMMAND ${CMAKE_COMMAND} -E copy
          ${LIBDIR}/aom/lib/aom_dll.lib
          ${LIBDIR}/aom/lib/aom.lib
        COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${LIBDIR}/aom/
          ${HARVEST_TARGET}/aom/
        DEPENDEES install
      )
    else()
      ExternalProject_Add_Step(external_aom after_install
        COMMAND ${CMAKE_COMMAND} -E copy
          ${LIBDIR}/aom/lib/aom_dll.lib
          ${LIBDIR}/aom/lib/aom.lib
        DEPENDEES install
      )
    endif()
endif()
