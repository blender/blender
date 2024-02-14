# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
  set(OSL_FLEX_BISON -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe)
else()
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES)
  set(OSL_FLEX_BISON)
  set(OSL_OPENIMAGEIO_LIBRARY "${LIBDIR}/openimageio/lib/OpenImageIO${SHAREDLIBEXT};${LIBDIR}/png/lib/${LIBPREFIX}png16${LIBEXT};${LIBDIR}/jpeg/lib/${LIBPREFIX}jpeg${LIBEXT};${LIBDIR}/tiff/lib/${LIBPREFIX}tiff${LIBEXT};${LIBDIR}/openexr/lib/IlmImf${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}")
endif()

set(OSL_EXTRA_ARGS
  ${DEFAULT_BOOST_FLAGS}
  -DOpenEXR_ROOT=${LIBDIR}/openexr/
  -DOpenImageIO_ROOT=${LIBDIR}/openimageio/
  -DOSL_BUILD_TESTS=OFF
  -DOSL_BUILD_MATERIALX=OFF
  -DPNG_ROOT=${LIBDIR}/png
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  ${OSL_FLEX_BISON}
  -DCMAKE_CXX_STANDARD_LIBRARIES=${OSL_CMAKE_CXX_STANDARD_LIBRARIES}
  -DBUILD_SHARED_LIBS=ON
  -DLINKSTATIC=OFF
  -DOSL_BUILD_PLUGINS=OFF
  -DSTOP_ON_WARNING=OFF
  -DUSE_LLVM_BITCODE=OFF
  -DLLVM_ROOT=${LIBDIR}/llvm/
  -DLLVM_DIRECTORY=${LIBDIR}/llvm/
  -DUSE_PARTIO=OFF
  -DUSE_QT=OFF
  -DUSE_Qt5=OFF
  -DINSTALL_DOCS=OFF
  -Dpugixml_ROOT=${LIBDIR}/pugixml
  -DTIFF_ROOT=${LIBDIR}/tiff
  -DJPEG_ROOT=${LIBDIR}/jpeg
  -DUSE_PYTHON=OFF
  -DImath_ROOT=${LIBDIR}/imath
  -DCMAKE_DEBUG_POSTFIX=_d
  -DPython_ROOT=${LIBDIR}/python
  -DPython_EXECUTABLE=${PYTHON_BINARY}
)

if(NOT APPLE)
  list(APPEND OSL_EXTRA_ARGS -DOSL_USE_OPTIX=ON)
endif()

ExternalProject_Add(external_osl
  URL file://${PACKAGE_DIR}/${OSL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  LIST_SEPARATOR ^^
  URL_HASH ${OSL_HASH_TYPE}=${OSL_HASH}
  PREFIX ${BUILD_DIR}/osl
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/osl/src/external_osl < ${PATCH_DIR}/osl.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/osl -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${DEFAULT_CMAKE_FLAGS} ${OSL_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/osl
)

add_dependencies(
  external_osl
  external_boost
  ll
  external_openexr
  external_zlib
  external_openimageio
  external_pugixml
)
if(WIN32)
  add_dependencies(
    external_osl
    external_flexbison
  )
elseif(UNIX AND NOT APPLE)
  add_dependencies(
    external_osl
    external_flex
  )
endif()

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_osl after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/osl/ ${HARVEST_TARGET}/osl
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_osl after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslcomp_d.lib ${HARVEST_TARGET}/osl/lib/oslcomp_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslexec_d.lib ${HARVEST_TARGET}/osl/lib/oslexec_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslquery_d.lib ${HARVEST_TARGET}/osl/lib/oslquery_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslnoise_d.lib ${HARVEST_TARGET}/osl/lib/oslnoise_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/bin/oslcomp_d.dll ${HARVEST_TARGET}/osl/bin/oslcomp_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/bin/oslexec_d.dll ${HARVEST_TARGET}/osl/bin/oslexec_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/bin/oslquery_d.dll ${HARVEST_TARGET}/osl/bin/oslquery_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/bin/oslnoise_d.dll ${HARVEST_TARGET}/osl/bin/oslnoise_d.dll
      DEPENDEES install
    )
  endif()
endif()
