# SPDX-FileCopyrightText: 2017-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  # VPX is determined to use pthreads which it will tell ffmpeg to dynamically
  # link, which is not something we're super into distribution wise. However
  # if it cannot find pthread.h it'll happily provide a pthread emulation
  # layer using win32 threads. So all this patch does is make it not find
  # pthead.h
  set(VPX_PATCH ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/vpx/src/external_vpx < ${PATCH_DIR}/vpx_windows.diff)
  set(VPX_EXTRA_FLAGS --target=x86_64-win64-gcc )
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

if(NOT BLENDER_PLATFORM_ARM)
  list(APPEND VPX_EXTRA_FLAGS
    --enable-sse4_1
    --enable-sse3
    --enable-ssse3
    --enable-avx
    --enable-avx2
  )
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
      --disable-unit-tests
      --disable-examples
      --enable-vp8
      --enable-vp9
      ${VPX_EXTRA_FLAGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make install
  PATCH_COMMAND ${VPX_PATCH}
  INSTALL_DIR ${LIBDIR}/vpx
)

if(MSVC)
  set_target_properties(external_vpx PROPERTIES FOLDER Mingw)
endif()
