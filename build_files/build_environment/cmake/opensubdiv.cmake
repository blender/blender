# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENSUBDIV_EXTRA_ARGS
  -DNO_LIB=OFF
  -DNO_EXAMPLES=ON
  -DNO_TUTORIALS=ON
  -DNO_REGRESSION=ON
  -DNO_PTEX=ON
  -DNO_DOC=ON
  -DNO_OMP=ON
  -DNO_TBB=OFF
  -DNO_CUDA=ON
  -DNO_OPENCL=ON
  -DNO_CLEW=ON
  -DNO_OPENGL=OFF
  -DNO_METAL=OFF
  -DNO_DX=ON
  -DNO_TESTS=ON
  -DNO_GLTESTS=ON
  -DNO_GLEW=ON
  -DNO_GLFW=ON
  -DNO_GLFW_X11=ON
  -DTBB_DIR=${LIBDIR}/tbb/lib/cmake/tbb
)

ExternalProject_Add(external_opensubdiv
  URL file://${PACKAGE_DIR}/${OPENSUBDIV_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENSUBDIV_HASH_TYPE}=${OPENSUBDIV_HASH}
  PREFIX ${BUILD_DIR}/opensubdiv
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opensubdiv
    -Wno-dev ${DEFAULT_CMAKE_FLAGS}
    ${OPENSUBDIV_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/opensubdiv
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_opensubdiv after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opensubdiv/lib
        ${HARVEST_TARGET}/opensubdiv/lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opensubdiv/include
        ${HARVEST_TARGET}/opensubdiv/include

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_opensubdiv after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/opensubdiv/lib/osdCPU.lib
        ${HARVEST_TARGET}/opensubdiv/lib/osdCPU_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/opensubdiv/lib/osdGPU.lib
        ${HARVEST_TARGET}/opensubdiv/lib/osdGPU_d.lib

      DEPENDEES install
    )
  endif()
endif()

add_dependencies(
  external_opensubdiv
  external_tbb
)
