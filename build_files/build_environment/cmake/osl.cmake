# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

string(REPLACE "-DCMAKE_CXX_STANDARD=20" " " OSL_CMAKE_FLAGS "${DEFAULT_CMAKE_FLAGS}")

if(WIN32)
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
  set(OSL_CMAKE_LINKER_FLAGS "")
  set(OSL_FLEX_BISON -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe)
else()
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES "")
  # llvm-config will add -lxml2. Make sure it can be found and that no system
  # library is used instead.
  set(OSL_CMAKE_LINKER_FLAGS "-L${LIBDIR}/xml2/lib")
  set(OSL_OPENIMAGEIO_LIBRARY "${LIBDIR}/openimageio/lib/OpenImageIO${SHAREDLIBEXT};${LIBDIR}/openexr/lib/IlmImf${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}")

  if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
    set(OSL_FLEX_BISON -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison)
  else()
    set(OSL_FLEX_BISON "")
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
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DPython3_ROOT=${LIBDIR}/python
  -DPython3_INCLUDE_DIR=${LIBDIR}/python/include/python${PYTHON_SHORT_VERSION}
  -Dlibdeflate_DIR=${LIBDIR}/deflate/lib/cmake/libdeflate
)

if(NOT (APPLE OR BLENDER_PLATFORM_WINDOWS_ARM OR CMAKE_CROSSCOMPILING))
  list(APPEND OSL_EXTRA_ARGS
    -DOSL_USE_OPTIX=ON
    -DCUDA_TARGET_ARCH=sm_50
    -DCUDA_TOOLKIT_ROOT_DIR=${CUDAToolkit_ROOT}
  )
endif()

set(OSL_CROSSCOMPILE_PATCH "true")  # Nullop when not cross-compiling.
if(CMAKE_CROSSCOMPILING)
  # There are muliple cross-compiling issues we need to fix:
  # 1st: The base OSL FindLLVM.cmake heavily relies on llvm-config, which cannot be executed in a cross-compiled environment.
  #      To workaround this, patch OSL to remove the LLVM find_package, and manually provide all variables by hand instead.
  set(OSL_CROSSCOMPILE_PATCH
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/osl/src/external_osl <
      ${PATCH_DIR}/osl_crosscompile_llvm.diff
  )

  file(GLOB _LLVM_LIBRARIES "${LIBDIR}/llvm/lib/*.a")
  string(REPLACE ";" "^^" _LLVM_LIBRARIES "${_LLVM_LIBRARIES}")  # Use ^^ list separators (as passed in ExternalProject)
  list(APPEND OSL_EXTRA_ARGS
    -DLLVM_FOUND=YES
    -DLLVM_VERSION=${LLVM_VERSION}  # Set in versions.cmake
    -DLLVM_INCLUDES=${LIBDIR}/llvm/include
    -DLLVM_LIBRARIES=${_LLVM_LIBRARIES}
    -DLLVM_LIB_DIR=${LIBDIR}/llvm/lib
    -DLLVM_TARGETS=${LLVM_TARGETS}  # Set in llvm.cmake

    # Interesting hack: We provide the LIBDIR crosscompiled LLVM static libs, but the HOST_LIBDIR LLVM_DIRECTORY for
    #                   the clang++ executable. Magically, this works.
    -DLLVM_DIRECTORY=${HOST_LIBDIR}/llvm
  )
  if(ANDROID)
    # Android specific LLVM build flags.
    string(REPLACE "-none-" "-" _ANDROID_TRIPLE ${ANDROID_LLVM_TRIPLE})
    list(APPEND OSL_EXTRA_ARGS
      -DLLVM_COMPILE_FLAGS=--target=${_ANDROID_TRIPLE}^^--sysroot=${CMAKE_SYSROOT}^^-stdlib=libc++
    )
  endif()

  # 2nd: OSL compiles two executables which it uses at build-time: oslc (external, shipped) and genluts (internal).
  #      As these executables cannot be launched on our host architecture, we instead fetch them from the host deps
  #      build, and patch OSL to use the provided executables accordingly.
  set(OSL_CROSSCOMPILE_PATCH
    ${OSL_CROSSCOMPILE_PATCH} &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/osl/src/external_osl <
      ${PATCH_DIR}/osl_crosscompile_host_genluts.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/osl/src/external_osl <
      ${PATCH_DIR}/osl_crosscompile_host_oslc.diff
  )

  set(_GENLUTS_PATH ${HOST_DEPS_BUILD_DIR}/build/osl/src/external_osl-build/bin/genluts)
  set(_OSLC_PATH ${HOST_LIBDIR}/osl/bin/oslc)
  if(NOT EXISTS ${_GENLUTS_PATH})
    message(FATAL_ERROR "OSL: Couldn't find required genluts executable in host deps build at path: ${_GENLUTS_PATH}")
  endif()
  if(NOT EXISTS ${_OSLC_PATH})
    message(FATAL_ERROR "OSL: Couldn't find required oslc executable in host deps build at path: ${_OSLC_PATH}")
  endif()

  list(APPEND OSL_EXTRA_ARGS
    -DGENLUTS_EXECUTABLE=${_GENLUTS_PATH}
    -DOSLC_EXECUTABLE=${_OSLC_PATH}
  )

  if(ANDROID)
    # Disable Python bindings, due to it causing similar linking issue as OIIO, OCIO, etc.. (see comment in opencolorio.cmake).
    list(APPEND OSL_EXTRA_ARGS
      -DUSE_PYTHON=OFF
    )
  endif()

  unset(_LLVM_LIBRARIES)
  unset(_ANDROID_TRIPLE)
  unset(_GENLUTS_PATHS)
  unset(_OSLC_PATHS)
endif()

ExternalProject_Add(external_osl
  URL file://${PACKAGE_DIR}/${OSL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  LIST_SEPARATOR ^^
  URL_HASH ${OSL_HASH_TYPE}=${OSL_HASH}
  PREFIX ${BUILD_DIR}/osl

  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/osl/src/external_osl <
      ${PATCH_DIR}/osl.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/osl/src/external_osl <
      ${PATCH_DIR}/osl_ptx_version.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/osl/src/external_osl <
      ${PATCH_DIR}/osl_relative_inc_cmake.diff &&
    ${OSL_CROSSCOMPILE_PATCH}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/osl
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    ${OSL_CMAKE_FLAGS}
    ${OSL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/osl
)

add_dependencies(
  external_osl
  external_llvm
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
        ${LIBDIR}/osl/lib/cmake/OSL/OSLTargets-debug.cmake
        ${HARVEST_TARGET}/osl/lib/cmake/OSL/OSLTargets-debug.cmake
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
  harvest_rpath_bin(external_osl osl/bin osl/bin "*")
  harvest(external_osl osl/include osl/include "*.h")
  # Cmake files first because harvest_rpath_lib edits them.
  harvest(external_osl osl/lib/cmake/OSL osl/lib/cmake/OSL "*.cmake")
  harvest_rpath_lib(external_osl osl/lib osl/lib "*${SHAREDLIBEXT}*")
  harvest(external_osl osl/share/OSL/shaders osl/share/OSL/shaders "*.h")
  harvest_rpath_python(external_osl
    osl/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*"
  )
endif()
