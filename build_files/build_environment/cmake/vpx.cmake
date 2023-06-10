# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  # VPX is determined to use pthreads which it will tell ffmpeg to dynamically
  # link, which is not something we're super into distribution wise. However
  # if it cannot find pthread.h it'll happily provide a pthread emulation
  # layer using win32 threads. So all this patch does is make it not find
  # pthead.h

  set(VPX_PATCH ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/vpx/src/external_vpx < ${PATCH_DIR}/vpx_windows.diff)

  if(MSVC_VERSION GREATER_EQUAL 1920) # 2019
    set(VPX_COMPILER_STRING vs16)
  else() # 2017
    set(VPX_COMPILER_STRING vs15)
  endif()

  if(BLENDER_PLATFORM_ARM)
    # ARM64 requires a min of vc142
    set(VPX_EXTRA_FLAGS --target=arm64-win64-vs16 --as=nasm)
    set(VPX_INCL_ARCH nopost-nodocs-arm64)
  else()
    set(VPX_EXTRA_FLAGS --target=x86_64-win64-${VPX_COMPILER_STRING} --as=nasm)
    set(VPX_INCL_ARCH nodocs-x86_64-win64)
  endif()

  set(VPX_CONFIGURE_COMMAND ${CONFIGURE_ENV_MSVC})

  set(VPX_INCLUDE_PATH ${BUILD_DIR}/vpx/src/external_vpx/vpx-vp8-vp9-${VPX_INCL_ARCH}md-${VPX_COMPILER_STRING}-v${VPX_VERSION})

  set(VPX_BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make dist && msbuild /m vpx.sln /p:OutDir=${BUILD_DIR}/vpx/src/external_vpx-build/ /p:Configuration=Release)
  set(VPX_INSTALL_COMMAND ${CONFIGURE_ENV} && ${CMAKE_COMMAND} -E copy_directory ${VPX_INCLUDE_PATH}/include ${LIBDIR}/vpx/include &&
                          ${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/vpx/src/external_vpx-build/ ${LIBDIR}/vpx/lib/ &&
                          ${CMAKE_COMMAND} -E copy ${LIBDIR}/vpx/lib/vpxmd.lib ${LIBDIR}/vpx/lib/vpx.lib)
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

  set(VPX_CONFIGURE_COMMAND ${CONFIGURE_ENV})

  set(VPX_BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make -j${MAKE_THREADS})
  set(VPX_INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make install)
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
  CONFIGURE_COMMAND ${VPX_CONFIGURE_COMMAND} &&
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
  BUILD_COMMAND ${VPX_BUILD_COMMAND}
  INSTALL_COMMAND ${VPX_INSTALL_COMMAND}
  PATCH_COMMAND ${VPX_PATCH}
  INSTALL_DIR ${LIBDIR}/vpx
)

if(MSVC)
  set_target_properties(external_vpx PROPERTIES FOLDER Mingw)
endif()
