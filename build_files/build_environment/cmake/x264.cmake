# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(X264_EXTRA_ARGS --enable-win32thread)
endif()

if(UNIX AND NOT BLENDER_PLATFORM_ARM)
  set(X264_CONFIGURE_ENV ${CONFIGURE_ENV} &&
    export AS=${LIBDIR}/nasm/bin/nasm
  )
elseif(WIN32)
  if(BLENDER_PLATFORM_ARM)
    set(X264_CONFIGURE_ENV
      ${CONFIGURE_ENV_CLANG_CL_NO_PERL} &&
      set "AS=${DOWNLOAD_DIR}/msys2/msys64/usr/bin/gas-preprocessor.pl -arch aarch64 -as-type armasm -- armasm64 -nologo"
    )
  else()
    set(X264_CONFIGURE_ENV ${CONFIGURE_ENV_NO_PERL})
  endif()
else()
  set(X264_CONFIGURE_ENV ${CONFIGURE_ENV})
endif()

ExternalProject_Add(external_x264
  URL file://${PACKAGE_DIR}/${X264_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${X264_HASH_TYPE}=${X264_HASH}
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d
    ${BUILD_DIR}/x264/src/external_x264 <
    ${PATCH_DIR}/x264_enable_clang-cl.diff
  PREFIX ${BUILD_DIR}/x264

  CONFIGURE_COMMAND ${X264_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/x264/src/external_x264/ &&
    ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/x264
      --enable-static
      --enable-pic
      --disable-lavf
      ${X264_EXTRA_ARGS}

  BUILD_COMMAND ${X264_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/x264/src/external_x264/ &&
    make -j${MAKE_THREADS}

  INSTALL_COMMAND ${X264_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/x264/src/external_x264/ &&
    make install

  INSTALL_DIR ${LIBDIR}/x264
)

if(WIN32)
  set_target_properties(external_x264 PROPERTIES FOLDER Mingw)
  if(BLENDER_PLATFORM_ARM)
    add_dependencies(
      external_x264
      ll
    )
  endif()
else()
  add_dependencies(
    external_x264
    external_nasm
  )

  harvest(external_x264 x264/lib ffmpeg/lib "*.a")
endif()
