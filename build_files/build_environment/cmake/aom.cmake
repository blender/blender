# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  # The default generator on windows is msbuild, which we do not
  # want to use for this dep, as needs to build with mingw
  set(AOM_GENERATOR "Ninja")
  # The default flags are full of MSVC options given this will be
  # building with mingw, it'll have an unhappy time with that and
  # we need to clear them out.
  set(AOM_CMAKE_FLAGS )
else()
  set(AOM_GENERATOR "Unix Makefiles")
  set(AOM_CMAKE_FLAGS ${DEFAULT_CMAKE_FLAGS})
endif()

set(AOM_EXTRA_ARGS
  -DENABLE_TESTDATA=OFF
  -DENABLE_TESTS=OFF
  -DENABLE_TOOLS=OFF
  -DENABLE_EXAMPLES=OFF
  ${AOM_EXTRA_ARGS_WIN32}
)

# This is slightly different from all other deps in the way that
# aom uses cmake as a build system, but still needs the environment setup
# to include perl so we manually setup the environment and call
# cmake directly for the configure, build and install commands.

ExternalProject_Add(external_aom
  URL file://${PACKAGE_DIR}/${AOM_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${AOM_HASH_TYPE}=${AOM_HASH}
  PREFIX ${BUILD_DIR}/aom
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/aom/src/external_aom < ${PATCH_DIR}/aom.diff
  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/aom/src/external_aom-build/ &&
    ${CMAKE_COMMAND} -G "${AOM_GENERATOR}" -DCMAKE_INSTALL_PREFIX=${LIBDIR}/aom ${AOM_CMAKE_FLAGS} ${AOM_EXTRA_ARGS} ${BUILD_DIR}/aom/src/external_aom/
  BUILD_COMMAND ${CMAKE_COMMAND} --build .
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install
  INSTALL_DIR ${LIBDIR}/aom
)
