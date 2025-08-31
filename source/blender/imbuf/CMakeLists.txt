# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(movie)
add_subdirectory(opencolorio)

set(INC
  PUBLIC .
  ./movie
  ../makesrna
  ../../../intern/memutil
)

set(INC_SYS
  ${JPEG_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIRS}
  ${OPENIMAGEIO_INCLUDE_DIRS}
)

set(SRC
  intern/allocimbuf.cc
  intern/colormanagement.cc
  intern/colormanagement_inline.h
  intern/conversion.cc
  intern/filetype.cc
  intern/filter.cc
  intern/format_bmp.cc
  intern/format_dds.cc
  intern/format_hdr.cc
  intern/format_iris.cc
  intern/format_jpeg.cc
  intern/format_png.cc
  intern/format_psd.cc
  intern/format_svg.cc
  intern/format_targa.cc
  intern/format_tiff.cc
  intern/imageprocess.cc
  intern/interp.cc
  intern/metadata.cc
  intern/module.cc
  intern/moviecache.cc
  intern/readimage.cc
  intern/rectop.cc
  intern/rotate.cc
  intern/scaling.cc
  intern/stereoimbuf.cc
  intern/thumbs.cc
  intern/thumbs_blend.cc
  intern/thumbs_font.cc
  intern/transform.cc
  intern/util.cc
  intern/util_gpu.cc
  intern/writeimage.cc

  IMB_colormanagement.hh
  IMB_imbuf.hh
  IMB_imbuf_enums.h
  IMB_imbuf_types.hh
  IMB_interp.hh
  IMB_metadata.hh
  IMB_moviecache.hh
  IMB_openexr.hh
  IMB_thumbs.hh
  intern/IMB_allocimbuf.hh
  intern/IMB_colormanagement_intern.hh
  intern/IMB_filetype.hh
  intern/IMB_filter.hh
  intern/imbuf.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  bf_imbuf_openimageio
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  bf_intern_memutil
  PRIVATE bf::imbuf::opencolorio
  PRIVATE bf::extern::nanosvg

  ${JPEG_LIBRARIES}
  PRIVATE bf::sequencer
)

if(WITH_IMAGE_OPENEXR)
  list(APPEND LIB
    bf_imbuf_openexr
  )
  add_definitions(-DWITH_IMAGE_OPENEXR)
else()
  list(APPEND SRC
    intern/openexr/openexr_stub.cpp
  )
endif()

# OpenImageIO headers include `Imath` headers when there is no SSE support for
# matrix operations. This depends on the specific architecture and compiler
# flags, most reliable is to always include the `Imath` headers if we have them.
if(DEFINED IMATH_INCLUDE_DIRS)
  list(APPEND INC_SYS
    ${IMATH_INCLUDE_DIRS}
  )
endif()

if(WITH_IMAGE_OPENJPEG)
  list(APPEND INC_SYS
    ${OPENJPEG_INCLUDE_DIRS}
  )
  list(APPEND SRC
    intern/format_jp2.cc
  )
  list(APPEND LIB
    ${OPENJPEG_LIBRARIES}
  )

  add_definitions(-DWITH_IMAGE_OPENJPEG ${OPENJPEG_DEFINES})
endif()

if(WITH_IMAGE_CINEON)
  list(APPEND SRC
    intern/format_dpx.cc
  )
  list(APPEND LIB
    bf_imbuf_cineon
  )
  add_definitions(-DWITH_IMAGE_CINEON)
endif()

if(WITH_IMAGE_WEBP)
  list(APPEND SRC
    intern/format_webp.cc
  )
  list(APPEND INC_SYS
    ${WEBP_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${WEBP_LIBRARIES}
  )
  add_definitions(-DWITH_IMAGE_WEBP)
endif()

if(WIN32)
  list(APPEND INC
    ../../../intern/utfconv
  )
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

# no need to compile object files for inline headers.
set_source_files_properties(
  intern/colormanagement_inline.cc
  PROPERTIES HEADER_FILE_ONLY TRUE
)

blender_add_lib(bf_imbuf "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::imbuf ALIAS bf_imbuf)

if(WITH_GTESTS)
  set(TEST_SRC
    tests/IMB_scaling_test.cc
    tests/IMB_transform_test.cc
  )
  blender_add_test_suite_lib(imbuf "${TEST_SRC}" "${INC}" "${INC_SYS}" "${LIB}")
endif()
