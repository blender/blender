# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
  set(OSL_CMAKE_LINKER_FLAGS)
  set(OSL_FLEX_BISON -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe)
else()
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES)
  # llvm-config will add -lmxl2. Make sure it can be found and that no system
  # library is used instead.
  set(OSL_CMAKE_LINKER_FLAGS "-L${LIBDIR}/xml2/lib")
  set(OSL_OPENIMAGEIO_LIBRARY "${LIBDIR}/openimageio/lib/OpenImageIO${SHAREDLIBEXT};${LIBDIR}/openexr/lib/IlmImf${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}")

  if(APPLE)
    # Explicitly specify Homebrew path, so we don't use the old system one.
    if(BLENDER_PLATFORM_ARM)
      set(OSL_FLEX_BISON -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison)
    else()
      set(OSL_FLEX_BISON -DBISON_EXECUTABLE=/usr/local/opt/bison/bin/bison)
    endif()
  else()
    set(OSL_FLEX_BISON)
  endif()
endif()

set(OSL_EXTRA_ARGS
  -DOpenImageIO_ROOT=${LIBDIR}/openimageio/
  -DOSL_BUILD_TESTS=OFF
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  ${OSL_FLEX_BISON}
  -DCMAKE_CXX_STANDARD_LIBRARIES=${OSL_CMAKE_CXX_STANDARD_LIBRARIES}
  -DCMAKE_EXE_LINKER_FLAGS=${OSL_CMAKE_LINKER_FLAGS}
  -DCMAKE_SHARED_LINKER_FLAGS=${OSL_CMAKE_LINKER_FLAGS}
  -DBUILD_SHARED_LIBS=ON
  -DLINKSTATIC=OFF
  -DOSL_BUILD_PLUGINS=OFF
  -DSTOP_ON_WARNING=OFF
  -DUSE_LLVM_BITCODE=ON
  -DLLVM_ROOT=${LIBDIR}/llvm/
  -DLLVM_STATIC=ON
  -DUSE_PARTIO=OFF
  -DUSE_QT=OFF
  -DINSTALL_DOCS=OFF
  -Dpugixml_ROOT=${LIBDIR}/pugixml
  -DUSE_PYTHON=ON
  -DImath_ROOT=${LIBDIR}/imath
  -DCMAKE_DEBUG_POSTFIX=_d
  -Dpybind11_ROOT=${LIBDIR}/pybind11
  -DPython_ROOT=${LIBDIR}/python
  -DPython_EXECUTABLE=${PYTHON_BINARY}
  -DPython3_ROOT=${LIBDIR}/python
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DPython3_INCLUDE_DIR=${LIBDIR}/python/include/python${PYTHON_SHORT_VERSION}/
  -Dlibdeflate_DIR=${LIBDIR}/deflate/lib/cmake/libdeflate
)

if(NOT (APPLE OR BLENDER_PLATFORM_WINDOWS_ARM))
  list(APPEND OSL_EXTRA_ARGS
    -DOSL_USE_OPTIX=ON
    -DCUDA_TARGET_ARCH=sm_50
    -DCUDA_TOOLKIT_ROOT_DIR=${CUDAToolkit_ROOT}
  )
endif()
if(WIN32)
  # Needed to make Clang compile CUDA code with VS2019
  list(APPEND OSL_EXTRA_ARGS
    -DLLVM_COMPILE_FLAGS=-D__CUDACC_VER_MAJOR__=${CUDAToolkit_VERSION_MAJOR}
  )
endif()

# IOS build has trouble locating correct builds
if(WITH_APPLE_CROSSPLATFORM)
  
  # Use iOS utility to set some env vars to help us build for iOS
  include(cmake/ios_defines.cmake)
  ios_get_dependency_env_vars(OPENIMAGEIO OPENEXR IMATH LLVM PNG PUGIXML ROBINMAP DEFLATE PYBIND11)
  
  set(OSL_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${IOSDEP_INCLUDES_STRING}")
  set(OSL_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${IOSDEP_INCLUDES_STRING}")
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} ${IOSDEP_LIBDIRS_STRING} ${IOSDEP_LIBRARIES_STRING}")
 
  # Disable bitcode for now as issues finding llvm bitcode generator
  set(OSL_EXTRA_ARGS
    ${OSL_EXTRA_ARGS}
    -DCMAKE_C_FLAGS=${OSL_CMAKE_C_FLAGS}
    -DCMAKE_CXX_FLAGS=${OSL_CMAKE_CXX_FLAGS}
    -DCMAKE_CXX_STANDARD_LIBRARIES=${OSL_CMAKE_CXX_STANDARD_LIBRARIES}
    -DOSL_BUILD_SHADERS=OFF
    -DUSE_LLVM_BITCODE=OFF
    ${IOSDEP_DEFINES}
  )

  # iOS patch removes system() calls and ensures bundles can build with correct compatibility.
  set(OSL_PATCH_FILE  ${PATCH_DIR}/osl_ios.diff)
else()
  set(OSL_PATCH_FILE  ${PATCH_DIR}/osl.diff)
endif()

ExternalProject_Add(external_osl
  URL file://${PACKAGE_DIR}/${OSL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  LIST_SEPARATOR ^^
  URL_HASH ${OSL_HASH_TYPE}=${OSL_HASH}
  PREFIX ${BUILD_DIR}/osl

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/osl/src/external_osl <
    ${OSL_PATCH_FILE}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/osl
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    ${DEFAULT_CMAKE_FLAGS}
    ${OSL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/osl
)

add_dependencies(
  external_osl
  ll
  external_openexr
  external_zlib
  external_openimageio
  external_pugixml
  external_python
  external_pybind11
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
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/osl/
        ${HARVEST_TARGET}/osl

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_osl after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/lib/oslcomp_d.lib
        ${HARVEST_TARGET}/osl/lib/oslcomp_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/lib/oslexec_d.lib
        ${HARVEST_TARGET}/osl/lib/oslexec_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/lib/oslquery_d.lib
        ${HARVEST_TARGET}/osl/lib/oslquery_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/lib/oslnoise_d.lib
        ${HARVEST_TARGET}/osl/lib/oslnoise_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/bin/oslcomp_d.dll
        ${HARVEST_TARGET}/osl/bin/oslcomp_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/bin/oslexec_d.dll
        ${HARVEST_TARGET}/osl/bin/oslexec_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/bin/oslquery_d.dll
        ${HARVEST_TARGET}/osl/bin/oslquery_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/osl/bin/oslnoise_d.dll
        ${HARVEST_TARGET}/osl/bin/oslnoise_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/osl/lib/python${PYTHON_SHORT_VERSION}/
        ${HARVEST_TARGET}/osl/lib/python${PYTHON_SHORT_VERSION}_debug/

      DEPENDEES install
    )
  endif()
else()
  harvest_rpath_bin(external_osl osl/bin osl/bin "oslc")
  harvest(external_osl osl/include osl/include "*.h")
  harvest_rpath_lib(external_osl osl/lib osl/lib "*${SHAREDLIBEXT}*")
  harvest(external_osl osl/share/OSL/shaders osl/share/OSL/shaders "*.h")
  harvest_rpath_python(external_osl
    osl/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*"
  )
endif()
