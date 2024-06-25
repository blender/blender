# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(LIB_PREFIX "")
  set(LIB_SUFFIX "-static")
else()
  set(LIB_PREFIX "lib")
  set(LIB_SUFFIX "")
endif()

if(BLENDER_PLATFORM_WINDOWS_ARM)
  set(X265_COMMON_ARGS
    -DCMAKE_C_COMPILER=${LIBDIR}/llvm/bin/clang-cl.exe
    -DCMAKE_CXX_COMPILER=${LIBDIR}/llvm/bin/clang-cl.exe
    -DCMAKE_C_FLAGS_INIT="--target=arm64-pc-windows-msvc"
    -DCMAKE_CXX_FLAGS_INIT="--target=arm64-pc-windows-msvc"
    -DCMAKE_CXX_STANDARD=11
  )

  set(X265_12_PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/x265_12/src/external_x265_12 < ${PATCH_DIR}/x265_windows_arm.diff)
  set(X265_10_PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/x265_10/src/external_x265_10 < ${PATCH_DIR}/x265_windows_arm.diff)
  set(X265_PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/x265/src/external_x265 < ${PATCH_DIR}/x265_windows_arm.diff)
elseif(APPLE)
  set(X265_COMMON_ARGS)
  set(X265_12_PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/x265_12/src/external_x265_12 < ${PATCH_DIR}/x265_apple.diff)
  set(X265_10_PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/x265_10/src/external_x265_10 < ${PATCH_DIR}/x265_apple.diff)
  set(X265_PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/x265/src/external_x265 < ${PATCH_DIR}/x265_apple.diff)
else()
  set(X265_COMMON_ARGS)
  set(X265_12_PATCH_COMMAND)
  set(X265_10_PATCH_COMMAND)
  set(X265_PATCH_COMMAND)
endif()

# 12 bit build flags
set(X265_12_EXTRA_ARGS
  -DHIGH_BIT_DEPTH=ON
  -DEXPORT_C_API=OFF
  -DENABLE_SHARED=OFF
  -DENABLE_PIC=ON
  -DENABLE_CLI=OFF
  -DMAIN12=ON
)

# 10 bit build flags
set(X265_10_EXTRA_ARGS
  -DHIGH_BIT_DEPTH=ON
  -DEXPORT_C_API=OFF
  -DENABLE_SHARED=OFF
  -DENABLE_PIC=ON
  -DENABLE_CLI=OFF
)

# 8 bit build flags
set(X265_EXTRA_ARGS
 -DENABLE_SHARED=OFF
 -DEXTRA_LIB=${LIBDIR}/x265_12/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}^^${LIBDIR}/x265_10/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
 -DLINKED_10BIT=ON
 -DLINKED_12BIT=ON
)

if(UNIX)
  # Use the suffix `.part` so this library isn't included when installing,
  # as the library which is merged is used instead.
  list(APPEND X265_EXTRA_ARGS
    -DCMAKE_STATIC_LIBRARY_SUFFIX_C=_unmerged${LIB_SUFFIX}${LIBEXT}.part
    -DCMAKE_STATIC_LIBRARY_SUFFIX_CXX=_unmerged${LIB_SUFFIX}${LIBEXT}.part
  )
endif()

if(WIN32)
  list(APPEND X265_EXTRA_ARGS -DNASM_EXECUTABLE=${NASM_PATH})
  list(APPEND X265_10_EXTRA_ARGS -DNASM_EXECUTABLE=${NASM_PATH})
  list(APPEND X265_12_EXTRA_ARGS -DNASM_EXECUTABLE=${NASM_PATH})
endif()

ExternalProject_Add(external_x265_12
  URL file://${PACKAGE_DIR}/${X265_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${X265_HASH_TYPE}=${X265_HASH}
  PREFIX ${BUILD_DIR}/x265_12
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  SOURCE_SUBDIR source
  PATCH_COMMAND ${X265_12_PATCH_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/x265_12 ${DEFAULT_CMAKE_FLAGS} ${X265_COMMON_ARGS} ${X265_12_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/x265_12
)

ExternalProject_Add(external_x265_10
  URL file://${PACKAGE_DIR}/${X265_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${X265_HASH_TYPE}=${X265_HASH}
  PREFIX ${BUILD_DIR}/x265_10
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  SOURCE_SUBDIR source
  PATCH_COMMAND ${X265_10_PATCH_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/x265_10 ${DEFAULT_CMAKE_FLAGS} ${X265_COMMON_ARGS} ${X265_10_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/x265_10
)

ExternalProject_Add(external_x265
  URL file://${PACKAGE_DIR}/${X265_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${X265_HASH_TYPE}=${X265_HASH}
  PREFIX ${BUILD_DIR}/x265
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  SOURCE_SUBDIR source
  PATCH_COMMAND ${X265_PATCH_COMMAND}
  LIST_SEPARATOR ^^
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/x265 ${DEFAULT_CMAKE_FLAGS} ${X265_COMMON_ARGS} ${X265_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/x265
)

add_dependencies(
  external_x265
  external_x265_12
  external_x265_10
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_x265 after_install
    COMMAND "${CMAKE_AR}"
      /OUT:${LIBDIR}/x265/lib/x265.lib
      ${LIBDIR}/x265/lib/${LIB_PREFIX}x265${LIB_SUFFIX}.lib
      ${LIBDIR}/x265_10/lib/${LIB_PREFIX}x265${LIB_SUFFIX}.lib
      ${LIBDIR}/x265_12/lib/${LIB_PREFIX}x265${LIB_SUFFIX}.lib
    DEPENDEES install
  )
endif()

if(UNIX)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Write a script and the STDIN for CMAKE_AR.
    # This is needed because `ar` requires STDIN instead of command line arguments, sigh!
    set(_ar_stdin "${BUILD_DIR}/x265/tmp/x265_ar_script.stdin")
    set(_ar_cmake "${BUILD_DIR}/x265/tmp/x265_ar_script.cmake")
    file(WRITE ${_ar_stdin} "\
CREATE ${LIBDIR}/x265/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
ADDLIB ${LIBDIR}/x265/lib/${LIB_PREFIX}x265${LIB_SUFFIX}_unmerged${LIBEXT}.part
ADDLIB ${LIBDIR}/x265_10/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
ADDLIB ${LIBDIR}/x265_12/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
SAVE
END
")
    file(WRITE ${_ar_cmake} "\
math(EXPR INDEX_AR \"$\{CMAKE_ARGC\}-2\")
math(EXPR INDEX_AR_INPUT \"$\{CMAKE_ARGC\}-1\")

set(ARG_AR $\{CMAKE_ARGV$\{INDEX_AR\}\})
set(ARG_AR_INPUT $\{CMAKE_ARGV$\{INDEX_AR_INPUT\}\})

execute_process(
  COMMAND $\{ARG_AR\} -M
  INPUT_FILE $\{ARG_AR_INPUT\}
)
")

    ExternalProject_Add_Step(external_x265 after_install
      COMMAND ${CMAKE_COMMAND} -P ${_ar_cmake} -- ${CMAKE_AR} ${_ar_stdin}
      DEPENDEES install
    )
    unset(_ar_stdin)
    unset(_ar_cmake)
  else()
    ExternalProject_Add_Step(external_x265 after_install
      COMMAND libtool -static -o
        ${LIBDIR}/x265/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
        ${LIBDIR}/x265/lib/${LIB_PREFIX}x265${LIB_SUFFIX}_unmerged${LIBEXT}.part
        ${LIBDIR}/x265_10/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
        ${LIBDIR}/x265_12/lib/${LIB_PREFIX}x265${LIB_SUFFIX}${LIBEXT}
      DEPENDEES install
    )
  endif()
endif()

unset(LIB_PREFIX)
unset(LIB_SUFFIX)
