# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(GMP_EXTRA_ARGS -enable-cxx)

if(WIN32)
  cmake_to_msys_path("${BUILD_DIR}/gmp/src/external_gmp/compile" compilescript_path)

  set(arlib_joint_path "ar-lib lib.exe")
  set(GMP_CFLAGS "-nologo -W3 -utf-8 -MP -MD -Z7 -Ob0 -Od -Xcompiler -RTC1 -DWIN32 -D_WINDOWS")
  set(GMP_CC_CXX "${compilescript_path} cl")
  set(GMP_NM "dumpbin.exe -symbols -headers")

  set(GMP_CONFIGURE_ENV set AR=${arlib_joint_path} && set NM=${GMP_NM} && ${CONFIGURE_ENV_NO_PERL} && set CC=${GMP_CC_CXX} && set CXX=${GMP_CC_CXX} && set CFLAGS=${GMP_CFLAGS} && set AS=:)

  set(GMP_OPTIONS --disable-static --enable-shared --enable-cxx --verbose gmp_cv_check_libm_for_build=no ac_cv_prog_LEX=: ac_cv_prog_YACC=: ac_cv_prog_ac_ct_STRIP=: ac_cv_prog_RANLIB=:)

  if(BLENDER_PLATFORM_ARM)
    set(GMP_OPTIONS ${GMP_OPTIONS} --enable-assembly=no --build=aarch64-pc-mingw32)
  else()
    set(GMP_OPTIONS ${GMP_OPTIONS} --enable-assembly=no --build=x86_64-pc-mingw32 ac_cv_func_memset=yes gmp_cv_asm_w32=.word)
  endif()
else()
  set(GMP_CONFIGURE_ENV ${CONFIGURE_ENV_NO_PERL})
  set(GMP_OPTIONS --enable-static --disable-shared )
endif()

if(APPLE AND NOT BLENDER_PLATFORM_ARM)
  set(GMP_OPTIONS
    ${GMP_OPTIONS}
    --with-pic
  )
elseif(UNIX AND NOT APPLE)
  set(GMP_OPTIONS
    ${GMP_OPTIONS}
    --with-pic
    --enable-fat
  )
endif()

# Boolean crashes with Arm assembly, see #103423.
if(BLENDER_PLATFORM_ARM)
  set(GMP_OPTIONS
    ${GMP_OPTIONS}
    --disable-assembly
  )
endif()

ExternalProject_Add(external_gmp
  URL file://${PACKAGE_DIR}/${GMP_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${GMP_HASH_TYPE}=${GMP_HASH}
  PREFIX ${BUILD_DIR}/gmp
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/gmp/src/external_gmp < ${PATCH_DIR}/gmp.diff
  CONFIGURE_COMMAND ${GMP_CONFIGURE_ENV} && cd ${BUILD_DIR}/gmp/src/external_gmp/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/gmp ${GMP_OPTIONS} ${GMP_EXTRA_ARGS}
  BUILD_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/gmp/src/external_gmp/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/gmp/src/external_gmp/ && make install
  INSTALL_DIR ${LIBDIR}/gmp
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_gmp after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/gmp/bin/gmp-10.dll ${HARVEST_TARGET}/gmp/lib/gmp-10.dll
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/gmp/lib/gmp.dll.lib ${HARVEST_TARGET}/gmp/lib/gmp.dll.lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/gmp/include ${HARVEST_TARGET}/gmp/include

    DEPENDEES install
  )
endif()

if(WIN32)
  # gmpxx is somewhat special, it builds on top of the C style gmp library but exposes C++ bindings
  # given the C++ ABI between MSVC and mingw is not compatible, we need to build the bindings
  # with MSVC, while GMP can only be build with mingw.
  ExternalProject_Add(external_gmpxx
    URL file://${PACKAGE_DIR}/${GMP_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${GMP_HASH_TYPE}=${GMP_HASH}
    PREFIX ${BUILD_DIR}/gmpxx
    PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_gmpxx.txt ${BUILD_DIR}/gmpxx/src/external_gmpxx/CMakeLists.txt &&
                          ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/config_gmpxx.h ${BUILD_DIR}/gmpxx/src/external_gmpxx/config.h
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/gmpxx ${DEFAULT_CMAKE_FLAGS} -DGMP_LIBRARY=${BUILD_DIR}/gmp/src/external_gmp/.libs/gmp.dll.lib -DGMP_INCLUDE_DIR=${BUILD_DIR}/gmp/src/external_gmp -DCMAKE_DEBUG_POSTFIX=_d
    INSTALL_DIR ${LIBDIR}/gmpxx
  )

  add_dependencies(
    external_gmpxx
    external_gmp
  )

  ExternalProject_Add_Step(external_gmpxx after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/gmpxx/ ${HARVEST_TARGET}/gmp
    DEPENDEES install
  )

endif()
