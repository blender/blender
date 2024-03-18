# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(MATERIALX_EXTRA_ARGS
  -DMATERIALX_BUILD_PYTHON=ON
  -DMATERIALX_BUILD_RENDER=ON
  -DMATERIALX_INSTALL_PYTHON=OFF
  -DMATERIALX_PYTHON_EXECUTABLE=${PYTHON_BINARY}
  -DMATERIALX_PYTHON_VERSION=${PYTHON_SHORT_VERSION}
  -DMATERIALX_BUILD_SHARED_LIBS=ON
  -DMATERIALX_BUILD_TESTS=OFF
  -DCMAKE_DEBUG_POSTFIX=_d
  # This policy makes pybind11_ROOT work.
  -DCMAKE_POLICY_DEFAULT_CMP0074=NEW
  -Dpybind11_ROOT=${LIBDIR}/pybind11
  -DPython_EXECUTABLE=${PYTHON_BINARY}
)

if(WIN32)
  LIST(APPEND MATERIALX_EXTRA_ARGS -DPYTHON_LIBRARIES=${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib)
endif()

ExternalProject_Add(external_materialx
  URL file://${PACKAGE_DIR}/${MATERIALX_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${MATERIALX_HASH_TYPE}=${MATERIALX_HASH}
  PREFIX ${BUILD_DIR}/materialx
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/materialx ${DEFAULT_CMAKE_FLAGS} ${MATERIALX_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/materialx
)

if(WIN32)
  set(MATERIALX_PYTHON_TARGET ${HARVEST_TARGET}/materialx/python/${BUILD_MODE})
  string(REPLACE "/" "\\" MATERIALX_PYTHON_TARGET_DOS "${MATERIALX_PYTHON_TARGET}")
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_materialx after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/include ${HARVEST_TARGET}/materialx/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/libraries ${HARVEST_TARGET}/materialx/libraries
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/lib/ ${HARVEST_TARGET}/materialx/lib/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/bin/ ${HARVEST_TARGET}/materialx/bin/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/python/ ${MATERIALX_PYTHON_TARGET}
      COMMAND del ${MATERIALX_PYTHON_TARGET_DOS}\\MaterialX\\*.lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_materialx after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/lib/ ${HARVEST_TARGET}/materialx/lib/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/bin/ ${HARVEST_TARGET}/materialx/bin/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/materialx/python/ ${MATERIALX_PYTHON_TARGET}
      COMMAND del ${MATERIALX_PYTHON_TARGET_DOS}\\MaterialX\\*.lib
      DEPENDEES install
    )
  endif()
  unset(MATERIALX_PYTHON_TARGET)
  unset(MATERIALX_PYTHON_TARGET_DOS)
endif()

add_dependencies(
  external_materialx
  external_python
  external_pybind11
)
