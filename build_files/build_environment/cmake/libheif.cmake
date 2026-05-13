# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(AOM_LIBRARY aom${LIBEXT})
else()
  set(AOM_LIBRARY libaom${LIBEXT})
endif()

if(ANDROID)
  set(LIBHEIF_EXTRA_ARGS
    # Override the find root path back to ANDROID_NDK only to workaround a curious issue in tiff (transitive dep of
    # libheif) which exports the CMath::CMath target incorrectly.
    # Also discussed here: https://gitlab.com/libtiff/libtiff/-/work_items/625
    -DCMAKE_FIND_ROOT_PATH=${ANDROID_NDK}
  )
endif()

set(LIBHEIF_EXTRA_ARGS
  ${LIBHEIF_EXTRA_ARGS}
  -DWITH_AOM_ENCODER=ON
  -DWITH_AOM_DECODER=ON
  -DWITH_DAV1D=OFF
  -DWITH_FFMPEG_DECODER=OFF
  -DWITH_JPEG_ENCODER=OFF
  -DWITH_KVAZAAR=OFF
  -DWITH_LIBDE265=OFF
  -DWITH_LIBSHARPYUV=OFF
  -DWITH_OPENJPH_ENCODER=OFF
  -DWITH_OpenH264_DECODER=OFF
  -DWITH_OpenJPEG_DECODER=OFF
  -DWITH_OpenJPEG_ENCODER=OFF
  -DWITH_RAV1E=OFF
  -DWITH_SvtEnc=OFF
  -DWITH_UVG266=OFF
  -DWITH_VVDEC=OFF
  -DWITH_VVENC=OFF
  -DWITH_X265=OFF
  -DAOM_INCLUDE_DIR=${LIBDIR}/aom/include/
  -DAOM_LIBRARY=${LIBDIR}/aom/lib/${AOM_LIBRARY}
  -DJPEG_LIBRARY=${LIBDIR}/jpeg/lib/libjpeg.a
  -DJPEG_INCLUDE_DIR=${LIBDIR}/jpeg/include
  -DBUILD_SHARED_LIBS=OFF
  -DBUILD_TESTING=OFF
  -DWITH_EXAMPLES=OFF
  -DWITH_EXAMPLE_HEIF_VIEW=OFF
  -DWITH_GDK_PIXBUF=OFF
)

ExternalProject_Add(external_libheif
  URL file://${PACKAGE_DIR}/${LIBHEIF_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LIBHEIF_HASH_TYPE}=${LIBHEIF_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/libheif

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/libheif
    ${DEFAULT_CMAKE_FLAGS}
    ${LIBHEIF_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/libheif
)
add_dependencies(
  external_libheif
  external_aom
)
