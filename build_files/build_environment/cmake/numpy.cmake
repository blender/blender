# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  if(BUILD_MODE STREQUAL Debug)
    set(NUMPY_DIR_POSTFIX -pydebug)
    set(NUMPY_ARCHIVE_POSTFIX d)
    set(NUMPY_BUILD_OPTION --debug)
  else()
    set(NUMPY_DIR_POSTFIX "")
    set(NUMPY_ARCHIVE_POSTFIX "")
    set(NUMPY_BUILD_OPTION "")
  endif()
endif()

set(NUMPY_POSTFIX "")

if(WIN32)
  file(WRITE ${CMAKE_BINARY_DIR}/fix_path.bat
    "set PATH=${LIBDIR}/python;${LIBDIR}/python/scripts;%PATH%\n"
  )
  set(NUMPY_CONF ${CMAKE_BINARY_DIR}/fix_path.bat)
else()
  if(WITH_APPLE_CROSSPLATFORM)
    set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_SHORT_VERSION}")
    set(NUMPY_CONF
    export PATH=${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/python/bin:$ENV{PATH} &&
    export _PYTHON_SYSCONFIGDATA_NAME=_sysconfigdata__darwin_arm64-${APPLE_SDK_CROSSPLATFORM_NAME_LOWER} &&
    export IPHONEOS_DEPLOYMENT_TARGET=${OSX_MIN_DEPLOYMENT_TARGET}
    )
    set(CROSS_COMPILE_COMMANDS
    -Csetup-args=--cross-file=${MESON_APPLE_CONFIGURATION_FILE}
    -Csetup-args=-Dblas=none
    -Csetup-args=-Dlapack=none
    --config-settings=build-dir=${BUILD_DIR}/numpy/src/external_numpy-build
    )
  else()
    set(NUMPY_CONF export CYTHON=${LIBDIR}/python/bin/cython)
    set(CROSS_COMPILE_COMMANDS "")
  endif()
endif()

ExternalProject_Add(external_numpy
  URL file://${PACKAGE_DIR}/${NUMPY_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NUMPY_HASH_TYPE}=${NUMPY_HASH}
  PREFIX ${BUILD_DIR}/numpy
  PATCH_COMMAND ${NUMPY_PATCH}
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE 1

  BUILD_COMMAND ${NUMPY_CONF} && ${PYTHON_BINARY} -m pip install --no-build-isolation ${CROSS_COMPILE_COMMANDS} .

  INSTALL_COMMAND ""
)

if(NOT WITH_APPLE_CROSSPLATFORM)
add_dependencies(
  external_numpy
  external_python
  external_python_site_packages
  external_cython
)
endif()

if(WITH_APPLE_CROSSPLATFORM)
  ExternalProject_Add_Step(external_numpy after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${BUILD_DIR}/numpy/src/external_numpy/numpy
      ${BUILD_DIR}/numpy/src/external_numpy-build/numpy
    COMMAND find ${BUILD_DIR}/numpy/src/external_numpy-build/numpy -type d -name "*.p" -exec rm -rf {} +
    COMMAND find ${BUILD_DIR}/numpy/src/external_numpy-build/numpy -type f -name "*.a" -delete
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${BUILD_DIR}/numpy/src/external_numpy-build/numpy
      ${LIBDIR}/python/lib/python${PYTHON_SHORT_VERSION}/site-packages/numpy
    DEPENDEES install
  )
endif()