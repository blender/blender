# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(VPX_EXTRA_FLAGS --target=x86_64-win64-gcc --disable-multithread)
  else()
    set(VPX_EXTRA_FLAGS --target=x86-win32-gcc --disable-multithread)
  endif()
else()
  if(APPLE)
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
      set(VPX_EXTRA_FLAGS --target=generic-gnu)
    else()
      set(VPX_EXTRA_FLAGS --target=x86_64-darwin17-gcc)
    endif()
  else()
    set(VPX_EXTRA_FLAGS --target=generic-gnu)
  endif()
endif()

ExternalProject_Add(external_vpx
  URL file://${PACKAGE_DIR}/${VPX_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${VPX_HASH_TYPE}=${VPX_HASH}
  PREFIX ${BUILD_DIR}/vpx
  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/vpx/src/external_vpx/ &&
    ${CONFIGURE_COMMAND_NO_TARGET} --prefix=${LIBDIR}/vpx
      --disable-shared
      --enable-static
      --disable-install-bins
      --disable-install-srcs
      --disable-sse4_1
      --disable-sse3
      --disable-ssse3
      --disable-avx
      --disable-avx2
      --disable-unit-tests
      --disable-examples
      --enable-vp8
      --enable-vp9
      ${VPX_EXTRA_FLAGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make install
  INSTALL_DIR ${LIBDIR}/vpx
)

if(MSVC)
  set_target_properties(external_vpx PROPERTIES FOLDER Mingw)
endif()
