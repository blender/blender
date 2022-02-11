# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENSUBDIV_EXTRA_ARGS
  -DNO_EXAMPLES=ON
  -DNO_REGRESSION=ON
  -DNO_PYTHON=ON
  -DNO_MAYA=ON
  -DNO_PTEX=ON
  -DNO_DOC=ON
  -DNO_CLEW=OFF
  -DNO_OPENCL=OFF
  -DNO_TUTORIALS=ON
  -DGLEW_INCLUDE_DIR=${LIBDIR}/glew/include
  -DGLEW_LIBRARY=${LIBDIR}/glew/lib/libGLEW${LIBEXT}
  -DGLFW_INCLUDE_DIR=${LIBDIR}/glfw/include
  -DGLFW_LIBRARIES=${LIBDIR}/glfw/lib/glfw3${LIBEXT}
)

if(WIN32)
  set(OPENSUBDIV_EXTRA_ARGS
    ${OPENSUBDIV_EXTRA_ARGS}
    -DTBB_INCLUDE_DIR=${LIBDIR}/tbb/include
    -DTBB_LIBRARIES=${LIBDIR}/tbb/lib/tbb.lib
    -DCLEW_INCLUDE_DIR=${LIBDIR}/clew/include/CL
    -DCLEW_LIBRARY=${LIBDIR}/clew/lib/clew${LIBEXT}
    -DCUEW_INCLUDE_DIR=${LIBDIR}/cuew/include
    -DCUEW_LIBRARY=${LIBDIR}/cuew/lib/cuew${LIBEXT}
  )
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(OPENSUBDIV_EXTRA_ARGS
      ${OPENSUBDIV_EXTRA_ARGS}
      -DNO_CUDA=OFF
    )
  else()
    set(OPENSUBDIV_EXTRA_ARGS
      ${OPENSUBDIV_EXTRA_ARGS}
      -DNO_CUDA=ON
    )
  endif()

else()
  set(OPENSUBDIV_EXTRA_ARGS
    ${OPENSUBDIV_EXTRA_ARGS}
    -DNO_CUDA=ON
    -DCUEW_INCLUDE_DIR=${LIBDIR}/cuew/include
    -DCLEW_INCLUDE_DIR=${LIBDIR}/clew/include/CL
    -DCLEW_LIBRARY=${LIBDIR}/clew/lib/static/${LIBPREFIX}clew${LIBEXT}
  )
endif()

ExternalProject_Add(external_opensubdiv
  URL file://${PACKAGE_DIR}/${OPENSUBDIV_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENSUBDIV_HASH_TYPE}=${OPENSUBDIV_HASH}
  PREFIX ${BUILD_DIR}/opensubdiv
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opensubdiv -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${OPENSUBDIV_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/opensubdiv
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_opensubdiv after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opensubdiv/lib ${HARVEST_TARGET}/opensubdiv/lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opensubdiv/include ${HARVEST_TARGET}/opensubdiv/include
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_opensubdiv after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opensubdiv/lib/osdCPU.lib ${HARVEST_TARGET}/opensubdiv/lib/osdCPU_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opensubdiv/lib/osdGPU.lib ${HARVEST_TARGET}/opensubdiv/lib/osdGPU_d.lib
      DEPENDEES install
    )
  endif()
endif()

add_dependencies(
  external_opensubdiv
  external_glew
  external_glfw
  external_clew
  external_cuew
  external_tbb
)
